#ifndef AUTOMAPEXPORT_H
#define AUTOMAPEXPORT_H

// What the map shows, written to files for the reference mockup (docs/automap-mockup): set
// CSB_MAP_EXPORT=<dir> and run csb --visibility-test with a case file naming a save. Declarations
// only, so it can be included after CSB.h.

class AutoMapRecorder;

bool AutoMapExport_IsRequested(void);
// CSB_MAP_EXPORT=<dir>: writes index.json, one draw list per level, vision and spoiler level
// (L<level>_v<vision>_s<spoilers>.json) and the game's pictures (.bmp) into dir.
// CSB_MAP_RENDER=<out.bmp>,<level>,<vision>,<spoilers>,<cell>,<x>,<y>[,shift]: the map window as it
// would look, drawn off screen into out.bmp (see AutoMapExport.cpp)
void AutoMapExport_Run(AutoMapRecorder &recorder, int partyLevel);

#endif
