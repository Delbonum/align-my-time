#pragma once

#include <vector>

namespace amt
{

/** One point of the host's tempo map: at `seconds` (song time) the song is at `quarters`.
    Between points the tempo is constant, matching how ARA describes a tempo map. */
struct TempoPoint
{
    double seconds = 0.0;
    double quarters = 0.0;
};

/** A time signature that becomes active at `quarters` (always on a bar line). */
struct TimeSignature
{
    double quarters = 0.0;
    int numerator = 4;
    int denominator = 4;

    double quartersPerBar() const noexcept { return numerator * quartersPerBeat(); }
    double quartersPerBeat() const noexcept { return 4.0 / denominator; }
};

/** Maps song seconds <-> quarter notes <-> bars.
    Beyond the first and last tempo point the nearest tempo is extrapolated,
    so a constant tempo needs only a single point plus the tempo value. */
class TempoMap
{
public:
    /** Constant tempo in quarter notes per minute (what Cubase shows as BPM). */
    static TempoMap constant (double bpm, int numerator = 4, int denominator = 4);

    /** Piecewise tempo map. `points` must be sorted and strictly increasing in both
        seconds and quarters; `signatures` must be sorted by quarters. Empty lists fall back
        to 120 BPM / 4/4. */
    TempoMap (std::vector<TempoPoint> points, std::vector<TimeSignature> signatures);

    double secondsToQuarters (double seconds) const noexcept;
    double quartersToSeconds (double quarters) const noexcept;

    /** Tempo in quarter notes per minute at the given song time. */
    double bpmAt (double seconds) const noexcept;

    /** Bars are zero-based and fractional: bar 0.0 is the first bar line (quarter 0),
        bar 2.5 is half way through the third bar. */
    double quartersToBars (double quarters) const noexcept;
    double barsToQuarters (double bars) const noexcept;

    const TimeSignature& signatureAt (double quarters) const noexcept;

    const std::vector<TempoPoint>& points() const noexcept { return tempoPoints; }
    const std::vector<TimeSignature>& signatures() const noexcept { return timeSignatures; }

private:
    std::vector<TempoPoint> tempoPoints;
    std::vector<TimeSignature> timeSignatures;
    double startQuartersPerSecond = 2.0;
    double endQuartersPerSecond = 2.0;
};

} // namespace amt
