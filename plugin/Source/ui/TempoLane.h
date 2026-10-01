#pragma once

#include "LookAndFeel.h"

#include <amt/Alignment.h>
#include <amt/TempoMap.h>

namespace amt::plugin::ui
{

/** Tempo of the recording per bar (stepped line) against the dashed project tempo,
    so outliers - usually a misplaced marker - are visible at a glance. */
class TempoLane : public juce::Component
{
public:
    void setData (const AlignmentPlan& plan, const std::vector<double>& markerSeconds, const TempoMap& project,
                  double viewStart, double viewEnd);
    void paint (juce::Graphics&) override;

private:
    float timeToX (double seconds) const;

    std::vector<double> bpm, starts;
    double average = 0.0, minimum = 0.0, maximum = 0.0, target = 120.0;
    double viewStart = 0.0, viewEnd = 1.0;
};

} // namespace amt::plugin::ui
