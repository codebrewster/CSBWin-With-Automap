#ifndef AUTOMAPCOMPLETION_H
#define AUTOMAPCOMPLETION_H

// When the Statistics window's Profile reads a level of the dungeon again. No engine.

// Whether a level read for the Profile is read again: never read yet, or the party has spent time
// on it since it was read (version, the game ticks spent there, moved from readVersion), whether
// anyone was looking or not. The party's own level changes all the time: at most every 2 seconds.
bool AutoMapCompletion_LevelIsStale(bool cached, long long readVersion, long long version, bool partyOnLevel,
                                    unsigned msSinceRead);

#endif
