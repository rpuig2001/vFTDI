#pragma once

#include <atomic>

extern std::atomic<bool> SequenceAddRequested;
extern std::atomic<bool> SequenceSelectionActive;
extern std::atomic<bool> SequencePickArmed;
extern std::atomic<bool> SequenceAircraftClickRequested;
extern std::atomic<bool> SequenceCancelRequested;
