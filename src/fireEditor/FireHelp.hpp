/* SCENARIO INCENDIE - fire scenario editor (Simulator-fe).
   Explanations shown when the instructor clicks on a time field, plus a live check of the
   resulting timeline (so an impossible set of values is visible before saving).
   Source kept ASCII: accented text is written with \u escapes. */
#ifndef __FIREHELP_HPP_INCLUDED__
#define __FIREHELP_HPP_INCLUDED__

#include <string>
#include "../IncidentConfig.hpp"

namespace FireHelp {

enum Field {
    Field_None = 0,
    Field_FireDuration,
    Field_SinkLeadTime,
    Field_FireSpreadTime,
    Field_AbandonTime,
    Field_SurvivorInterval,
    Field_PermanentListTime,
    Field_HeloDelay,
    Field_HeloSpeed,
    Field_SarBoatDelay
};

// Short label + explanation of what the value does in the simulator.
std::wstring title(Field f);
std::wstring explanation(Field f);

// One-line tooltip (hover), shorter than the explanation.
std::wstring tooltip(Field f);

// The timeline these values produce ("Abandon a T+2:00 ...") followed by any warnings.
std::wstring timeline(const IncidentConfig& c);

// Everything together, ready for the help box: title, explanation, blank line, timeline.
std::wstring fullText(Field f, const IncidentConfig& c);

// True if the values cannot work (e.g. she starts sinking before the survivors are in the water).
bool hasBlockingProblem(const IncidentConfig& c);

}

#endif
