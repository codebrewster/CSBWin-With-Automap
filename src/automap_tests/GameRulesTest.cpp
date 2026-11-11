#include "TestSupport.h"
#include "../GameRules.h"

#include <cstring>
#include <string>

static bool SameRules(const GameRules &a, const GameRules &b)
{
  return    a.viAltarRecharge == b.viAltarRecharge
         && a.fountainDrink == b.fountainDrink
         && a.shootExperience20 == b.shootExperience20
         && a.vulnerableOnAttack == b.vulnerableOnAttack
         && a.shotDamageFactor == b.shotDamageFactor;
}

static void ClassicRulesAreThoseOfCsbwin176(void)
{
  GameRules classic = GameRules_Classic();
  CHECK(!classic.viAltarRecharge);
  CHECK(!classic.fountainDrink);
  CHECK(!classic.shootExperience20);
  CHECK(!classic.vulnerableOnAttack);
  CHECK(classic.shotDamageFactor == 1);
  CHECK(GameRules_AreClassic(classic));
}

static void NostalgiaModeGivesTheClassicRulesWhateverTheOtherSettings(void)
{
  CHECK(SameRules(GameRules_FromSettings(true, true, 4), GameRules_Classic()));
  CHECK(SameRules(GameRules_FromSettings(true, false, 1), GameRules_Classic()));
}

static void WithoutNostalgiaModeThePortsRulesFollowTheSettings(void)
{
  GameRules r = GameRules_FromSettings(false, true, 4);
  CHECK(r.viAltarRecharge);
  CHECK(r.fountainDrink);
  CHECK(r.shootExperience20);
  CHECK(r.vulnerableOnAttack);
  CHECK(r.shotDamageFactor == 4);
  CHECK(!GameRules_AreClassic(r));
  // Launchers power 1 and not vulnerable: still the port's fountain, altar and experience
  CHECK(!GameRules_AreClassic(GameRules_FromSettings(false, false, 1)));
}

static void LauncherPowerOutsideTheMenuStepsIsKeptInRange(void)
{
  CHECK(GameRules_FromSettings(false, false, 0).shotDamageFactor == 1);
  CHECK(GameRules_FromSettings(false, false, 99).shotDamageFactor == 6);
}

// The port's recordings from before the rules were written down: the 4x bonus was always on,
// the fountain and the SHOOT experience too, the altar and vulnerability followed the settings
static void OlderRecordingsOfThePortKeepTheRulesTheyWereMadeWith(void)
{
  GameRules r = GameRules_PortBefore(true, false);
  CHECK(!r.viAltarRecharge);
  CHECK(r.fountainDrink);
  CHECK(r.shootExperience20);
  CHECK(!r.vulnerableOnAttack);
  CHECK(r.shotDamageFactor == 4);
  r = GameRules_PortBefore(false, true);
  CHECK(r.viAltarRecharge);
  CHECK(r.vulnerableOnAttack);
}

static void ClassicRulesHaveNoDifferences(void)
{
  CHECK(GameRules_Differences(GameRules_Classic()).empty());
}

static void EachRuleOfThePortIsNamedAsADifference(void)
{
  GameRules r = GameRules_FromSettings(false, true, 3);
  std::vector<std::string> differences = GameRules_Differences(r);
  CHECK(differences.size() == 5);
  r = GameRules_Classic();
  r.shotDamageFactor = 2;
  differences = GameRules_Differences(r);
  CHECK(differences.size() == 1);
  CHECK(differences[0].find("2") != std::string::npos);
}

static void ReadLines(const std::string &text, GameRules *r)
{
  size_t start = 0;
  while (start < text.size())
  {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    CHECK(GameRules_ReadLine(line.c_str(), r));
    start = end + 1;
  }
}

static void WrittenRulesAreReadBackTheSame(void)
{
  GameRules written = GameRules_FromSettings(false, true, 5);
  std::string text = GameRules_RecordLines(written);
  GameRules read = GameRules_Classic();
  ReadLines(text, &read);
  CHECK(SameRules(read, written));
  written = GameRules_Classic();
  text = GameRules_RecordLines(written);
  read = GameRules_FromSettings(false, true, 6);
  ReadLines(text, &read);
  CHECK(SameRules(read, written));
}

// CSBwin skips a # line it does not know; each rule line is one word after the #, as the
// playfile reader cuts a # line at the first blank or slash
static void RuleLinesAreSingleWordCommentLines(void)
{
  std::string text = GameRules_RecordLines(GameRules_FromSettings(false, true, 4));
  size_t start = 0;
  int lines = 0;
  while (start < text.size())
  {
    size_t end = text.find('\n', start);
    CHECK(end != std::string::npos);
    std::string line = text.substr(start, end - start);
    CHECK(line.size() > 1 && line[0] == '#' && line[1] != '#');
    CHECK(line.find(' ') == std::string::npos && line.find('/') == std::string::npos);
    lines++;
    start = end + 1;
  }
  CHECK(lines == 5);
}

static void OtherCommentLinesAreNotRules(void)
{
  GameRules r = GameRules_Classic();
  CHECK(!GameRules_ReadLine("#DMRules", &r));
  CHECK(!GameRules_ReadLine("#ShotDamageFactor", &r));
  CHECK(!GameRules_ReadLine("#ShotDamageFactorX", &r));
  CHECK(!GameRules_ReadLine("00000010 0001 0002 0003 12345678", &r));
  CHECK(SameRules(r, GameRules_Classic()));
}

static void ReplayRulesHoldOnlyWhileTheReplayRuns(void)
{
  GameRules settings = GameRules_FromSettings(false, true, 4);
  GameRules_SetSettings(settings);
  CHECK(SameRules(GameRules_Current(), settings));
  GameRules_BeginReplay(GameRules_Classic());
  CHECK(SameRules(GameRules_Current(), GameRules_Classic()));
  CHECK(GameRules_ReadLine("#ShotDamageFactor3", GameRules_Replay()));
  CHECK(GameRules_Current().shotDamageFactor == 3);
  // The settings change meanwhile: they count once the replay is over
  GameRules later = GameRules_FromSettings(true, false, 1);
  GameRules_SetSettings(later);
  CHECK(GameRules_Current().shotDamageFactor == 3);
  GameRules_EndReplay();
  CHECK(SameRules(GameRules_Current(), later));
  CHECK(GameRules_Replay() == NULL);
}

void TestGameRules(void)
{
  ClassicRulesAreThoseOfCsbwin176();
  NostalgiaModeGivesTheClassicRulesWhateverTheOtherSettings();
  WithoutNostalgiaModeThePortsRulesFollowTheSettings();
  LauncherPowerOutsideTheMenuStepsIsKeptInRange();
  OlderRecordingsOfThePortKeepTheRulesTheyWereMadeWith();
  ClassicRulesHaveNoDifferences();
  EachRuleOfThePortIsNamedAsADifference();
  WrittenRulesAreReadBackTheSame();
  RuleLinesAreSingleWordCommentLines();
  OtherCommentLinesAreNotRules();
  ReplayRulesHoldOnlyWhileTheReplayRuns();
}
