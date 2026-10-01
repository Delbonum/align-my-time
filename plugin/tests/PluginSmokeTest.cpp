// End-to-end test of the plug-in without a DAW: a simulated host plays a drifting recording
// through the plug-in (non-ARA path), a "foot switch" taps along via MIDI, then the result is
// rendered and checked against the project grid. Also saves screenshots of the three steps.
//
// Usage: AlignMyTimeSmokeTest [screenshot-folder]

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <amt/OnsetDetector.h>
#include <AlignMyTimeAssets.h>

#include <cstdio>
#include <random>

using namespace amt;
using namespace amt::plugin;

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    std::printf ("  [%s] %s\n", condition ? "ok" : "FAILED", what);
    failures += condition ? 0 : 1;
}

struct FakeHost : juce::AudioPlayHead
{
    bool playing = false;
    juce::int64 sample = 0;
    double rate = 48000.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setTimeInSamples (sample);
        info.setTimeInSeconds ((double) sample / rate);
        info.setBpm (120.0);
        info.setTimeSignature (TimeSignature { 4, 4 });
        info.setPpqPosition ((double) sample / rate * 2.0);
        info.setPpqPositionOfLastBarStart (std::floor ((double) sample / rate * 2.0 / 4.0) * 4.0);
        return info;
    }
};

/** 12 bars at ~114 BPM with drift, plucks on every beat, starting at 1.03 s. */
struct Take
{
    std::vector<double> downbeats;
    std::vector<float> left, right;
};

Take makeTake (double rate)
{
    Take take;
    double t = 1.03;
    std::vector<std::pair<double, float>> onsets;
    for (int bar = 0; bar < 12; ++bar)
    {
        const double beat = 60.0 / (114.0 + 2.0 * std::sin (bar * 0.7));
        take.downbeats.push_back (t);
        for (int b = 0; b < 4; ++b)
            onsets.push_back ({ t + b * beat, b == 0 ? 0.8f : 0.45f });
        t += 4 * beat;
    }
    take.downbeats.push_back (t);
    onsets.push_back ({ t, 0.8f });

    const auto length = (size_t) ((t + 1.5) * rate);
    take.left.assign (length, 0.0f);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    for (const auto& [onset, amp] : onsets)
    {
        const auto start = (size_t) std::llround (onset * rate);
        for (size_t i = 0; i < (size_t) (0.25 * rate) && start + i < length; ++i)
        {
            const double s = (double) i / rate;
            const float click = i < 240 ? 0.6f * noise (rng) : 0.0f;
            take.left[start + i] += amp * (float) std::exp (-s * 18.0)
                                    * ((float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * s) + click);
        }
    }
    take.right = take.left;
    return take;
}

void pumpMessages (int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

void saveSnapshot (juce::Component& c, const juce::File& file)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
    std::printf ("  saved %s\n", file.getFullPathName().toRawUTF8());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    setLanguage (Language::german);
    setTapKey (TapKey::space);
    const juce::File screenshots = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File();

    constexpr double rate = 48000.0;
    constexpr int block = 512;

    auto processor = std::make_unique<AlignMyTimeProcessor>();
    FakeHost host;
    processor->setPlayHead (&host);
    processor->setRateAndBufferSizeDetails (rate, block);
    processor->prepareToPlay (rate, block);

    const auto take = makeTake (rate);

    std::printf ("1. Host plays the track once, foot switch taps along (MIDI)\n");
    std::mt19937 rng (11);
    std::uniform_real_distribution<double> jitter (-0.04, 0.04);
    std::vector<juce::int64> tapSamples;
    for (auto d : take.downbeats)
        tapSamples.push_back ((juce::int64) std::llround ((d + jitter (rng)) * rate));

    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    // Plays [from, to) seconds through the plug-in like a host would, tapping via MIDI.
    auto playRange = [&] (double from, double to) {
        host.playing = true;
        size_t nextTap = 0;
        const auto start = (juce::int64) (from * rate) / block * block;
        while (nextTap < tapSamples.size() && tapSamples[nextTap] < start)
            ++nextTap;
        for (host.sample = start; host.sample < (juce::int64) (to * rate); host.sample += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const auto smp = (size_t) host.sample + (size_t) i;
                buffer.setSample (0, i, smp < take.left.size() ? take.left[smp] : 0.0f);
                buffer.setSample (1, i, smp < take.right.size() ? take.right[smp] : 0.0f);
            }
            midi.clear();
            while (nextTap < tapSamples.size() && tapSamples[nextTap] < host.sample + block)
                midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), (int) (tapSamples[nextTap++] - host.sample));

            processor->processBlock (buffer, midi);
            // We run much faster than real time: give the message thread time to drain the recorder.
            if ((host.sample / block) % 16 == 0)
                pumpMessages (40);
        }
        pumpMessages (100);
        host.playing = false;
        processor->processBlock (buffer, midi);
        pumpMessages (200);
    };

    {
        // Before anything was recorded the space bar belongs to the host (starts Cubase).
        std::unique_ptr<juce::AudioProcessorEditor> early (processor->createEditor());
        check (! early->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "space bar goes to the host while there is no track yet");
    }

    // Stopped early, then started again in the middle: both passes add up to the whole track.
    playRange (0.0, 9.0);
    check (processor->getSession().hasSource(), "first partial pass is usable right away");
    playRange (7.0, (double) take.left.size() / rate);

    auto& session = processor->getSession();
    check (session.hasSource(), "track was recorded from the input");
    std::printf ("  merged source: start %lld, %lld samples (take %zu)\n", (long long) session.getSource()->startSample,
                 (long long) session.getSource()->numSamples(), take.left.size());
    check (session.getSource()->startSample == 0 && session.getSource()->numSamples() >= (int64_t) take.left.size() - block,
           "two partial passes merged into the whole track");
    check (session.getMarkers().size() == take.downbeats.size(), "one marker per tapped downbeat");
    double worstSnap = 0.0;
    for (size_t i = 0; i < session.getMarkers().size() && i < take.downbeats.size(); ++i)
        worstSnap = juce::jmax (worstSnap, std::abs (session.getMarkers()[i].seconds - take.downbeats[i]));
    std::printf ("  worst marker error after snapping: %.2f ms\n", worstSnap * 1000.0);
    check (worstSnap < 0.004, "markers snapped onto the attacks");
    check (session.getPlan().averageBpm > 112.0 && session.getPlan().averageBpm < 116.0, "tapped tempo around 114 BPM");
    check (session.getPlan().firstBar == 1, "first marker lands on the nearest project bar (bar 2)");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    editor->setVisible (true);
    pumpMessages (100);
    if (screenshots != juce::File())
    {
        screenshots.createDirectory();
        saveSnapshot (*editor, screenshots.getChildFile ("1-tappen.png"));
    }

    std::printf ("2. Review and render (time-stretch)\n");
    session.setStep (Step::review);
    session.selectMarker (6);
    pumpMessages (100);
    if (screenshots != juce::File())
        saveSnapshot (*editor, screenshots.getChildFile ("2-pruefen.png"));

    session.startRender();
    for (int i = 0; i < 600 && session.isRendering(); ++i)
        pumpMessages (50);
    check (session.getAligned() != nullptr && session.isAlignedUpToDate(), "render finished");

    if (auto aligned = session.getAligned())
    {
        OnsetDetector detector (*aligned);
        const auto onsets = detector.detectAll (1.5);
        double worst = 0.0;
        for (int k = 0; k < 48; ++k)
        {
            const double expected = 2.0 + 0.5 * k;
            double best = 1.0;
            for (auto o : onsets)
                best = juce::jmin (best, std::abs (o - expected));
            worst = juce::jmax (worst, best);
        }
        std::printf ("  worst beat deviation from the 120 BPM grid: %.2f ms\n", worst * 1000.0);
        check (worst < 0.006, "every beat of the result sits on the project grid");
    }

    std::printf ("3. Replace in track: the host now hears the aligned audio\n");
    session.setStep (Step::render);
    session.updateSettings ([] (SessionSettings& s) { s.destination = Destination::replaceInTrack; });
    session.setReplaceActive (true);
    pumpMessages (100);
    if (screenshots != juce::File())
        saveSnapshot (*editor, screenshots.getChildFile ("3-rendern.png"));

    host.playing = true;
    host.sample = (juce::int64) (6.0 * rate);
    buffer.clear();
    processor->processBlock (buffer, midi);
    const auto aligned = session.getAligned();
    float maxDiff = 0.0f;
    for (int i = 0; i < block; ++i)
        maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (0, i) - aligned->sampleAt (0, host.sample + i)));
    check (maxDiff < 1.0e-6f, "track output is replaced by the aligned audio");
    host.playing = false;

    std::printf ("4. Project state round trip\n");
    juce::MemoryBlock state;
    processor->getStateInformation (state);
    editor.reset();

    auto restored = std::make_unique<AlignMyTimeProcessor>();
    restored->setPlayHead (&host);
    restored->prepareToPlay (rate, block);
    restored->setStateInformation (state.getData(), (int) state.getSize());
    pumpMessages (100);
    check (restored->getSession().getMarkers().size() == session.getMarkers().size(), "markers restored");
    check (restored->getSession().hasSource(), "recorded track restored");
    for (int i = 0; i < 600 && restored->getSession().isRendering(); ++i)
        pumpMessages (50);
    check (restored->getSession().isReplaceActive(), "replacement re-rendered and active after reload");

    std::printf ("5. Tapped on 1 and 3 although \"every one\" was selected\n");
    {
        auto& s = restored->getSession();
        s.setStep (Step::tap);
        s.beginTapping (0.0);
        for (size_t i = 0; i + 1 < take.downbeats.size(); ++i)
        {
            s.addTap (take.downbeats[i]);
            s.addTap (0.5 * (take.downbeats[i] + take.downbeats[i + 1]));
        }
        const auto suggestion = s.getTapUnitSuggestion();
        if (screenshots != juce::File())
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (restored->createEditor());
            s.setStep (Step::review);
            ed->setVisible (true);
            pumpMessages (100);
            saveSnapshot (*ed, screenshots.getChildFile ("2b-pruefen-tempohinweis.png"));
        }
        std::printf ("  tapped tempo read as bars: %.1f BPM (project 120)\n", s.getPlan().averageBpm);
        check (suggestion.has_value() && *suggestion == TapUnit::halfBar, "plug-in suggests counting the taps as half bars");
        if (suggestion.has_value())
            s.updateSettings ([u = *suggestion] (SessionSettings& st) { st.tapUnit = u; });
        check (std::abs (s.getPlan().averageBpm - 114.0) < 2.0 && ! s.getTapUnitSuggestion().has_value(), "after one click the tempo fits");
    }

    std::printf ("6. Tap key and Ctrl+Space\n");
    {
        auto& s = restored->getSession();
        s.setStep (Step::tap);
        std::unique_ptr<juce::AudioProcessorEditor> ed (restored->createEditor());
        check (! ed->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey, juce::ModifierKeys::ctrlModifier, 0)),
               "Ctrl+Space always goes to the host (its transport)");

        setTapKey (TapKey::tab);
        restored->startPreview (s.getSource()->startSeconds() + 2.0, false, false, false);
        juce::AudioBuffer<float> out (2, block);
        juce::MidiBuffer none;
        for (int i = 0; i < 8; ++i)
            restored->processBlock (out, none);
        s.beginTapping (0.0);
        const auto before = s.getNumTapsThisPass();
        check (ed->keyPressed (juce::KeyPress (juce::KeyPress::tabKey)), "configured tap key (Tab) is used");
        check (s.getNumTapsThisPass() == before + 1, "Tab tapped a marker");
        check (! ed->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "space stays with the host when it is not the tap key");
        restored->stopPreview();
        setTapKey (TapKey::space);
    }

    std::printf ("7. Manual target tempo\n");
    {
        auto& s = restored->getSession();
        pumpMessages (100); // the host tempo arrives via the processor's timer
        check (! s.usesManualTempo() && std::abs (s.getProjectTempo().bpmAt (0.0) - 120.0) < 1.0e-6, "host tempo used by default");
        s.updateSettings ([] (SessionSettings& st) { st.manualTempo = true; st.manualBpm = 100.0; st.manualNumerator = 3; });
        check (std::abs (s.getProjectTempo().bpmAt (0.0) - 100.0) < 1.0e-6 && s.getProjectTempo().signatureAt (0.0).numerator == 3,
               "manual tempo and time signature become the target");
        s.updateSettings ([] (SessionSettings& st) { st.manualTempo = false; });
        check (std::abs (s.getProjectTempo().bpmAt (0.0) - 120.0) < 1.0e-6, "switching back uses the project tempo again");
    }

    std::printf ("8. Standalone: audio file, manual tempo, English UI, settings\n");
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        auto standalone = std::make_unique<AlignMyTimeProcessor>();
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        standalone->setRateAndBufferSizeDetails (rate, block);
        standalone->prepareToPlay (rate, block);
        check (standalone->isStandalone(), "standalone instance detected");
        check (standalone->getBlockingReason (false).contains ("Audiodatei"), "standalone asks for an audio file");

        // Write the take at 44.1 kHz to check resampling on load.
        const auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("amt-smoke-take.wav");
        {
            wavFile.deleteFile();
            std::unique_ptr<juce::OutputStream> stream (wavFile.createOutputStream());
            auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions {}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (24));
            juce::AudioBuffer<float> buf (1, (int) (take.left.size() * 44100.0 / rate));
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample (0, i, take.left[(size_t) ((double) i * rate / 44100.0)]);
            writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        }

        standalone->loadAudioFile (wavFile);
        for (int i = 0; i < 100 && ! standalone->getSession().hasSource(); ++i)
            pumpMessages (50);
        auto& s = standalone->getSession();
        check (s.hasSource() && standalone->isUsingAudioFile(), "audio file loaded");
        check (s.hasSource() && std::abs ((double) s.getSource()->numSamples() - (double) take.left.size()) < rate * 0.01,
               "file resampled from 44.1 to 48 kHz");
        check (s.usesManualTempo(), "standalone uses the manual tempo");

        if (screenshots != juce::File())
        {
            setLanguage (Language::english);
            std::unique_ptr<juce::AudioProcessorEditor> ed (standalone->createEditor());
            ed->setVisible (true);
            pumpMessages (100);
            saveSnapshot (*ed, screenshots.getChildFile ("4-standalone-englisch.png"));
            setLanguage (Language::german);

            std::unique_ptr<juce::AudioProcessorEditor> de (standalone->createEditor());
            de->setVisible (true);
            dynamic_cast<AlignMyTimeEditor*> (de.get())->showSettings();
            pumpMessages (100);
            saveSnapshot (*de, screenshots.getChildFile ("5-einstellungen.png"));
        }
        check (tr ("Abspielen") == "Abspielen", "German texts back after switching the language");
        wavFile.deleteFile();
    }

    std::printf ("9. Icon\n");
    {
        const auto icon = juce::ImageCache::getFromMemory (AlignMyTimeAssets::icon256_png, AlignMyTimeAssets::icon256_pngSize);
        check (icon.isValid() && icon.getWidth() == 256 && icon.hasAlphaChannel(), "app icon embedded (256 px, transparent corners)");
        check (versionString() == "1.1.1", "version shown in the credits is the project version");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    restored.reset();
    processor.reset();
    return failures == 0 ? 0 : 1;
}
