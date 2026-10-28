// AutoMapSceneInternal.h : helpers of AutoMapScene.cpp that AutoMapPress.cpp shares. Not
// part of the scene's interface: only the two scene units include it.
#ifndef AUTOMAPSCENEINTERNAL_H
#define AUTOMAPSCENEINTERNAL_H

#include <string>

#include "AutoMapScene.h"

// The draw-list cell at (x, y), or NULL when it is not shown
const AutoMapShownCell *FindShownCell(const AutoMapDrawList &drawList, int x, int y);

// N, E, S or W for an AutoMapDirection, '?' otherwise
char FacingInitial(int facing);

// A gate's four bits, highest first: 0011
std::string GateBits(int value);

// What a message does to a door, false wall or pit, with a leading space: " OPEN"
const char *DoorEffectWord(AutoMapSignal signal);

// The raw signal, with a leading space: " SET", " CLEAR", " TOGGLE", " HOLD"
const char *SignalWord(AutoMapSignal signal);

// " AFTER 1.5S" for a delay in ticks, empty for none
std::string DelayWord(int delay);

#endif
