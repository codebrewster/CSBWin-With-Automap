#ifndef SHOTMISSILES_H
#define SHOTMISSILES_H

// The missiles that a champion shot with a launcher: they hit harder than thrown ones. The
// missile records are frozen by the format of the saved game, so the mark is kept beside them,
// under the index of the record: its address changes when the engine enlarges the database.
// This header declares no structures, so that it can be included after CSB.h, which leaves
// #pragma pack(1) switched on.

static const int SHOT_MISSILE_DAMAGE_FACTOR = 4;

// The attack announces a shot just before it launches the missile. The launch takes the
// announcement whether or not it finds a free record, so that it never marks a later missile.
void ExpectShotMissile(void);
bool TakeExpectedShotMissile(void);

void RememberShotMissile(int recordIndex);
bool IsShotMissile(int recordIndex);
void ForgetShotMissile(int recordIndex);
// The records of the next dungeon reuse the indices of the current ones. An expected shot is
// forgotten too.
void ForgetAllShotMissiles(void);

#endif
