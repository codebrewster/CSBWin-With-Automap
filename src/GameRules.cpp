// GameRules.cpp : the rules where this port and CSBwin 17.6 differ, and which ones are in force.
#include "GameRules.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const int MAX_LAUNCHER_POWER = 6;   // the steps of the Launchers power menu

GameRules GameRules_Classic(void)
{
  GameRules r;
  r.viAltarRecharge = false;
  r.fountainDrink = false;
  r.shootExperience20 = false;
  r.vulnerableOnAttack = false;
  r.shotDamageFactor = 1;
  return r;
}

GameRules GameRules_FromSettings(bool nostalgiaMode, bool vulnerableOnAttack, int launcherPower)
{
  if (nostalgiaMode) return GameRules_Classic();
  GameRules r;
  r.viAltarRecharge = true;
  r.fountainDrink = true;
  r.shootExperience20 = true;
  r.vulnerableOnAttack = vulnerableOnAttack;
  if (launcherPower < 1) launcherPower = 1;
  if (launcherPower > MAX_LAUNCHER_POWER) launcherPower = MAX_LAUNCHER_POWER;
  r.shotDamageFactor = launcherPower;
  return r;
}

GameRules GameRules_PortBefore(bool nostalgiaMode, bool vulnerableOnAttack)
{
  GameRules r;
  r.viAltarRecharge = !nostalgiaMode;
  r.fountainDrink = true;
  r.shootExperience20 = true;
  r.vulnerableOnAttack = vulnerableOnAttack;
  r.shotDamageFactor = 4;
  return r;
}

bool GameRules_AreClassic(const GameRules &rules)
{
  return GameRules_Differences(rules).empty();
}

std::vector<std::string> GameRules_Differences(const GameRules &rules)
{
  std::vector<std::string> d;
  if (rules.fountainDrink) d.push_back("Clicking a fountain with an empty hand drinks");
  if (rules.viAltarRecharge) d.push_back("The Vi altar recharges weapons");
  if (rules.shootExperience20) d.push_back("SHOOT gives 20 experience instead of 9");
  if (rules.vulnerableOnAttack) d.push_back("Monsters vulnerable on attack");
  if (rules.shotDamageFactor != 1)
  {
    char text[64];
    std::snprintf(text, sizeof text, "Launchers power %d", rules.shotDamageFactor);
    d.push_back(text);
  }
  return d;
}

// One word each: the playfile reader cuts a # line at its first blank or slash
static const char *ON_OFF_NAMES[4][2] = {
  {"#ViAltarRecharge", "#NoViAltarRecharge"},
  {"#FountainDrink", "#NoFountainDrink"},
  {"#ShootExperience20", "#NoShootExperience20"},
  {"#VulnerableOnAttack", "#NoVulnerableOnAttack"},
};
static const char *FACTOR_NAME = "#ShotDamageFactor";

static bool *OnOffRule(GameRules *rules, int i)
{
  switch (i)
  {
  case 0: return &rules->viAltarRecharge;
  case 1: return &rules->fountainDrink;
  case 2: return &rules->shootExperience20;
  default: return &rules->vulnerableOnAttack;
  }
}

std::string GameRules_RecordLines(const GameRules &rules)
{
  GameRules copy = rules;
  std::string text;
  for (int i = 0; i < 4; i++)
  {
    text += ON_OFF_NAMES[i][*OnOffRule(&copy, i) ? 0 : 1];
    text += "\n";
  }
  char factor[64];
  std::snprintf(factor, sizeof factor, "%s%d\n", FACTOR_NAME, rules.shotDamageFactor);
  text += factor;
  return text;
}

bool GameRules_ReadLine(const char *line, GameRules *rules)
{
  for (int i = 0; i < 4; i++)
  {
    for (int value = 0; value < 2; value++)
    {
      if (std::strcmp(line, ON_OFF_NAMES[i][value]) == 0)
      {
        *OnOffRule(rules, i) = (value == 0);
        return true;
      }
    }
  }
  size_t nameLength = std::strlen(FACTOR_NAME);
  if (std::strncmp(line, FACTOR_NAME, nameLength) != 0) return false;
  const char *number = line + nameLength;
  if (*number < '1' || *number > '9') return false;
  char *end;
  long factor = std::strtol(number, &end, 10);
  if (*end != 0 || factor > 99) return false;
  rules->shotDamageFactor = (int)factor;
  return true;
}

static GameRules settingsRules = GameRules_Classic();
static GameRules replayRules = GameRules_Classic();
static bool replayRuns = false;
static GameRules recordingRules = GameRules_Classic();
static bool recordingKeepsRules = false;

void GameRules_KeepForRecording(const GameRules &rules)
{
  recordingRules = rules;
  recordingKeepsRules = true;
}

void GameRules_EndRecording(void)
{
  recordingKeepsRules = false;
}

void GameRules_SetSettings(const GameRules &rules)
{
  settingsRules = rules;
}

void GameRules_BeginReplay(const GameRules &rules)
{
  replayRules = rules;
  replayRuns = true;
}

GameRules *GameRules_Replay(void)
{
  return replayRuns ? &replayRules : NULL;
}

void GameRules_EndReplay(void)
{
  replayRuns = false;
}

const GameRules &GameRules_Current(void)
{
  if (replayRuns) return replayRules;
  return recordingKeepsRules ? recordingRules : settingsRules;
}
