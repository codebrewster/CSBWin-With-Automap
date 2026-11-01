#ifndef AUTOMAPEXPORT_H
#define AUTOMAPEXPORT_H

// What the map shows, written to files for the reference mockup (docs/automap-mockup): set
// CSB_MAP_EXPORT=<dir> and run csb --visibility-test with a case file naming a save. Declarations
// only, so it can be included after CSB.h.

class AutoMapRecorder;

bool AutoMapExport_IsRequested(void);
// Writes index.json, one draw list per level, vision and spoiler level (L<level>_v<vision>_s<spoilers>.json)
// and the game's pictures (.bmp) into dir
void AutoMapExport_Run(const char *dir, AutoMapRecorder &recorder, int partyLevel);

#endif
