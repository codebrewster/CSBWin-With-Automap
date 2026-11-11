#ifndef GAMERULES_H
#define GAMERULES_H

// The rules where this port and CSBwin 17.6 differ. With Nostalgia Mode on the game follows
// 17.6, so a recording replays there too. A recording names the rules it was made with in
// # lines, which CSBwin skips; a replay plays by them, not by the settings of the moment.
// CSB.h leaves #pragma pack(1) switched on: this header sets the default packing for itself,
// so that GameRules and the standard library look the same in every file.

#pragma pack(push)
#pragma pack()

#include <string>
#include <vector>

struct GameRules
{
  bool viAltarRecharge;     // a weapon left in the Vi altar gets its charges back
  bool fountainDrink;       // an empty hand on a fountain gives the leader water
  bool shootExperience20;   // SHOOT gives 20 experience where graphics.dat says 9 (DM 1.1)
  bool vulnerableOnAttack;  // a monster that is attacking has half its defence
  int  shotDamageFactor;    // a missile shot with a launcher hits this many times as hard
};

GameRules GameRules_Classic(void);
// Nostalgia Mode gives the classic rules; without it, the port's, with "Monsters vulnerable
// on attack" and "Launchers power" (1 to 6) as set
GameRules GameRules_FromSettings(bool nostalgiaMode, bool vulnerableOnAttack, int launcherPower);
// A recording of this port from before the rules were written into it
GameRules GameRules_PortBefore(bool nostalgiaMode, bool vulnerableOnAttack);
bool GameRules_AreClassic(const GameRules &rules);
// What differs from CSBwin 17.6, one phrase each, for the warning when a recording starts
std::vector<std::string> GameRules_Differences(const GameRules &rules);

// The # lines that name the rules in a recording, one per rule
std::string GameRules_RecordLines(const GameRules &rules);
// A rule line changes that rule and gives true; any other line gives false
bool GameRules_ReadLine(const char *line, GameRules *rules);

// The rules in force: the settings, or a replay's while it runs
void GameRules_SetSettings(const GameRules &rules);
void GameRules_BeginReplay(const GameRules &rules);
GameRules *GameRules_Replay(void);   // NULL when no replay runs
void GameRules_EndReplay(void);
const GameRules &GameRules_Current(void);

#pragma pack(pop)

#endif
