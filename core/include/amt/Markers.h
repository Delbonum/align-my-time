#pragma once

#include <vector>

namespace amt
{

class OnsetDetector;

/** How far apart the taps are on the project grid. Step 1 offers "every one" (bar) and
    "every beat"; the review step can reinterpret the taps, e.g. when someone tapped on 1 and 3. */
enum class TapUnit
{
    twoBars,  ///< every other bar
    bar,      ///< one tap per bar, on the "one"
    halfBar,  ///< e.g. on 1 and 3 in 4/4
    beat,     ///< one tap per beat (counts as given by the time signature)
    halfBeat  ///< eighths in 4/4
};

/** Grid units from largest to smallest. */
constexpr TapUnit tapUnitsBySize[] { TapUnit::twoBars, TapUnit::bar, TapUnit::halfBar, TapUnit::beat, TapUnit::halfBeat };

enum class MarkerOrigin
{
    tapped,   ///< came from a tap during playback
    inserted, ///< filled in automatically for a missed tap
    manual    ///< added or moved by the user in the review step
};

struct Marker
{
    double seconds = 0.0;       ///< position in song time, after snapping / editing
    double tappedSeconds = 0.0; ///< where the tap actually landed (after latency compensation)
    MarkerOrigin origin = MarkerOrigin::tapped;
    bool snappedToAttack = false;
};

/** Something the review step should point the user to. */
struct MarkerIssue
{
    enum class Kind
    {
        doubleTapRemoved,  ///< a tap much too close to the previous one was dropped
        missedTapFilled,   ///< one or more markers were inserted into a gap
        irregularInterval  ///< interval deviates strongly from its neighbours but was kept
    };

    Kind kind;
    int markerIndex; ///< index into the cleaned-up marker list the issue refers to
};

struct TapCleanupSettings
{
    double doubleTapRatio = 0.45;      ///< intervals shorter than this fraction of the local median are double taps
    double missedTapRatio = 1.6;       ///< intervals longer than this multiple of the local median contain missed taps
    double irregularRatio = 0.18;      ///< relative deviation that is flagged but kept
    int neighbourhood = 4;             ///< intervals on each side used for the local median
};

/** Turns raw tap times (song seconds, sorted or not) into a clean marker list:
    removes double taps, fills single/multiple missed taps by interpolation and reports issues. */
std::vector<Marker> cleanUpTaps (const std::vector<double>& tapSeconds,
                                 const TapCleanupSettings& settings = {},
                                 std::vector<MarkerIssue>* issues = nullptr);

/** Moves every tapped (not manually placed) marker onto the nearest clear attack within
    +-windowSeconds. Returns how many markers were snapped. */
int snapMarkersToAttacks (std::vector<Marker>& markers, const OnsetDetector& detector, double windowSeconds = 0.07);

/** Pulls markers towards a smooth tempo curve, which evens out sloppy taps.

    `grid` holds each marker's position on the project grid (e.g. in quarter notes), so time
    signature changes and other grid units are handled. For every marker a robust local curve
    through its neighbours (+-`neighbourhood`, the marker itself left out) predicts where it should
    be; the marker moves `amount` (0..1) of the way there. A single bad tap therefore barely bends the
    curve, and gradual tempo changes (ritardando) are kept. Manually placed markers stay where they
    are but still guide their neighbours. Returns the new positions; the order is always kept.

    With a `detector` the audio decides where it can: a marker resting on an attack that fits the
    curve stays there (it is the actual beat). Markers without an attack, and clear outliers caught on
    the wrong attack (ghost note, flam), aim at the clear attack closest to the predicted position
    (within an eighth of the marker spacing, at most 25 ms), or at the curve where there is none.
    Markers on an attack also guide their neighbours more than those without. `onAttack` (optional)
    tells which markers ended up aimed at an attack. */
std::vector<double> straightenMarkers (const std::vector<Marker>& markers, const std::vector<double>& grid,
                                       double amount, int neighbourhood = 4, const OnsetDetector* detector = nullptr,
                                       std::vector<bool>* onAttack = nullptr);

} // namespace amt
