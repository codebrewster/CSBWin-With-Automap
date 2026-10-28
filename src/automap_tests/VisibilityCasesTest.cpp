#include "TestSupport.h"
#include "../AutoMapVisibilityCases.h"

static const char *const EXAMPLE =
    "# Things in the prepared save\n"
    "save csb-Visibility-test\n"
    "\n"
    "at L0 (12,7) facing N light 4\n"
    "  decoration (12,5) S   seen        # the ring\n"
    "  item (12,6)           glimpsed\n"
    "  monster (13,5)        not seen\n"
    "  door (12,3)           seen\n"
    "at L2 (3,40) facing W light 0\n";

static void ACaseFileGivesTheSaveTheCasesAndTheirChecks(void)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  CHECK(AutoMapVisibilityCases_Parse(EXAMPLE, &parsed, &error));
  CHECK(error.empty());
  CHECK(parsed.sections.size() == 1);
  if (parsed.sections.size() != 1) return;
  CHECK(parsed.sections[0].save == "csb-Visibility-test" && parsed.sections[0].lineNumber == 2);
  const std::vector<AutoMapVisibilityCase> &cases = parsed.sections[0].cases;
  CHECK(cases.size() == 2);
  if (cases.size() != 2) return;
  const AutoMapVisibilityCase &first = cases[0];
  CHECK(first.lineNumber == 4);
  CHECK(first.level == 0 && first.x == 12 && first.y == 7);
  CHECK(first.facing == 0 && first.light == 4);
  CHECK(first.checks.size() == 4);
  if (first.checks.size() != 4) return;
  CHECK(first.checks[0].lineNumber == 5);
  CHECK(first.checks[0].thing == AutoMapVisibilityDecoration);
  CHECK(first.checks[0].x == 12 && first.checks[0].y == 5 && first.checks[0].side == 2);
  CHECK(first.checks[0].expected == AutoMapVisibilitySeen);
  CHECK(first.checks[1].thing == AutoMapVisibilityItem && first.checks[1].side == -1);
  CHECK(first.checks[1].expected == AutoMapVisibilityGlimpsed);
  CHECK(first.checks[2].thing == AutoMapVisibilityMonster);
  CHECK(first.checks[2].x == 13 && first.checks[2].y == 5);
  CHECK(first.checks[2].expected == AutoMapVisibilityNotSeen);
  CHECK(first.checks[3].thing == AutoMapVisibilityDoor);
  const AutoMapVisibilityCase &survey = cases[1];
  CHECK(survey.level == 2 && survey.x == 3 && survey.y == 40 && survey.facing == 3);
  CHECK(survey.light == 0 && survey.checks.empty());
}

static bool FailsOnLine(const char *text, const char *lineMention)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  if (AutoMapVisibilityCases_Parse(text, &parsed, &error)) return false;
  return error.find(lineMention) != std::string::npos;
}

static void AMistakeIsReportedWithItsLine(void)
{
  // A check before any case
  CHECK(FailsOnLine("save s\n  item (1,1) seen\n", "line 2"));
  // No save
  CHECK(FailsOnLine("at L0 (1,1) facing N light 0\n", "save"));
  CHECK(FailsOnLine("# nothing\n", "save"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing Q light 0\n", "line 2"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 6\n", "line 2"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  item (1,1) visible\n", "line 3"));
  // Only an item and a decoration can be glimpsed
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  door (1,1) glimpsed\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  decoration (1,1) seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  chest (1,1) seen\n", "line 3"));
}

static void ACheckIsWrittenAsTheFileWritesIt(void)
{
  AutoMapVisibilityCheck decoration = {9, AutoMapVisibilityDecoration, 12, 5, 1, -1, "", AutoMapVisibilitySeen};
  CHECK(AutoMapVisibilityCases_CheckLine(decoration, AutoMapVisibilityGlimpsed)
        == "decoration (12,5) E glimpsed");
  AutoMapVisibilityCheck monster = {9, AutoMapVisibilityMonster, 3, 4, -1, -1, "", AutoMapVisibilitySeen};
  CHECK(AutoMapVisibilityCases_CheckLine(monster, AutoMapVisibilityNotSeen) == "monster (3,4) not seen");
  // What is written reads back
  AutoMapVisibilityCases parsed;
  std::string error;
  std::string text = "save s\nat L0 (1,1) facing N light 0\n  "
                     + AutoMapVisibilityCases_CheckLine(decoration, AutoMapVisibilityGlimpsed) + "\n";
  CHECK(AutoMapVisibilityCases_Parse(text, &parsed, &error));
  CHECK(parsed.sections.size() == 1 && parsed.sections[0].cases.size() == 1);
  if (parsed.sections.size() != 1 || parsed.sections[0].cases.size() != 1) return;
  const AutoMapVisibilityCase &readBack = parsed.sections[0].cases[0];
  CHECK(readBack.checks.size() == 1 && readBack.checks[0].side == 1
        && readBack.checks[0].expected == AutoMapVisibilityGlimpsed);
}

// One item of a pile: by its name, with its corner when two share the name
static void AnItemIsNamedAndPlacedOnItsTile(void)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  CHECK(AutoMapVisibilityCases_Parse("save s\nat L0 (1,1) facing N light 0\n"
                                     "  item (2,12) \"Dagger\" seen\n"
                                     "  item (2,12) SE \"Magic Box\" not seen  # two words\n"
                                     "  item (2,12) NW glimpsed\n"
                                     "  item (2,12) seen\n",
                                     &parsed, &error));
  CHECK(error.empty());
  if (parsed.sections.size() != 1 || parsed.sections[0].cases.size() != 1) return;
  const std::vector<AutoMapVisibilityCheck> &checks = parsed.sections[0].cases[0].checks;
  if (checks.size() != 4) return;
  CHECK(checks[0].itemName == "Dagger" && checks[0].corner == -1);
  CHECK(checks[0].expected == AutoMapVisibilitySeen);
  CHECK(checks[1].itemName == "Magic Box" && checks[1].corner == 2);
  CHECK(checks[1].expected == AutoMapVisibilityNotSeen);
  CHECK(checks[2].itemName.empty() && checks[2].corner == 0);
  CHECK(checks[3].itemName.empty() && checks[3].corner == -1);
  // Only an item has a corner and a name; a name ends with its quote
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  door (1,1) NW seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  monster (1,1) \"Mummy\" seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  item (1,1) \"Dagger seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  item (1,1) \"\" seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  item (1,1) N seen\n", "line 3"));
  AutoMapVisibilityCheck torch = {9, AutoMapVisibilityItem, 2, 12, -1, 2, "Torch", AutoMapVisibilitySeen};
  CHECK(AutoMapVisibilityCases_CheckLine(torch, AutoMapVisibilityGlimpsed)
        == "item (2,12) SE \"Torch\" glimpsed");
  torch.corner = -1;
  CHECK(AutoMapVisibilityCases_CheckLine(torch, AutoMapVisibilitySeen) == "item (2,12) \"Torch\" seen");
}

// What an alcove holds, by the wall face it opens on: the map remembers it per face, not per item
static void AnAlcoveIsNamedByItsWallFace(void)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  CHECK(AutoMapVisibilityCases_Parse("save s\nat L0 (4,16) facing S light 5\n"
                                     "  alcove (4,18) N seen   # the Vi altar's scroll\n",
                                     &parsed, &error));
  CHECK(error.empty());
  if (parsed.sections.size() != 1 || parsed.sections[0].cases.size() != 1) return;
  const std::vector<AutoMapVisibilityCheck> &checks = parsed.sections[0].cases[0].checks;
  CHECK(checks.size() == 1);
  if (checks.size() != 1) return;
  CHECK(checks[0].thing == AutoMapVisibilityAlcove);
  CHECK(checks[0].x == 4 && checks[0].y == 18 && checks[0].side == 0);
  CHECK(checks[0].expected == AutoMapVisibilitySeen);
  // It needs its face; the map shows what an alcove holds or nothing, never a glimpse of it
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  alcove (1,1) seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  alcove (1,1) N glimpsed\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  alcove (1,1) N \"Scroll\" seen\n", "line 3"));
  AutoMapVisibilityCheck alcove = {9, AutoMapVisibilityAlcove, 4, 18, 0, -1, "", AutoMapVisibilitySeen};
  CHECK(AutoMapVisibilityCases_CheckLine(alcove, AutoMapVisibilityNotSeen) == "alcove (4,18) N not seen");
}

// A whole tile, floor or wall, by the explored bit the map's fog shows it by
static void ATileIsNamedByItsCell(void)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  CHECK(AutoMapVisibilityCases_Parse("save s\nat L0 (4,16) facing S light 5\n"
                                     "  tile (4,17) seen\n"
                                     "  tile (4,19) not seen   # beyond the light\n",
                                     &parsed, &error));
  CHECK(error.empty());
  if (parsed.sections.size() != 1 || parsed.sections[0].cases.size() != 1) return;
  const std::vector<AutoMapVisibilityCheck> &checks = parsed.sections[0].cases[0].checks;
  CHECK(checks.size() == 2);
  if (checks.size() != 2) return;
  CHECK(checks[0].thing == AutoMapVisibilityTile);
  CHECK(checks[0].x == 4 && checks[0].y == 17 && checks[0].side == -1);
  CHECK(checks[0].expected == AutoMapVisibilitySeen);
  CHECK(checks[1].expected == AutoMapVisibilityNotSeen);
  // A tile has no face, no name and no glimpse
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  tile (1,1) N seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  tile (1,1) \"Scroll\" seen\n", "line 3"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\n  tile (1,1) glimpsed\n", "line 3"));
  AutoMapVisibilityCheck tile = {9, AutoMapVisibilityTile, -1, 18, -1, -1, "", AutoMapVisibilitySeen};
  CHECK(AutoMapVisibilityCases_CheckLine(tile, AutoMapVisibilityNotSeen) == "tile (-1,18) not seen");
}

// Every save line starts a section; its cases are drawn in that save
static void EachSaveHasTheCasesBelowIt(void)
{
  AutoMapVisibilityCases parsed;
  std::string error;
  CHECK(AutoMapVisibilityCases_Parse("save first\n"
                                     "at L0 (1,1) facing N light 0\n"
                                     "  door (1,0) seen\n"
                                     "\n"
                                     "save second   # monsters\n"
                                     "at L3 (5,6) facing E light 4\n"
                                     "at L3 (5,7) facing E light 4\n",
                                     &parsed, &error));
  CHECK(error.empty());
  CHECK(parsed.sections.size() == 2);
  if (parsed.sections.size() != 2) return;
  CHECK(parsed.sections[0].save == "first" && parsed.sections[0].lineNumber == 1);
  CHECK(parsed.sections[0].cases.size() == 1 && parsed.sections[0].cases[0].checks.size() == 1);
  CHECK(parsed.sections[1].save == "second" && parsed.sections[1].lineNumber == 5);
  CHECK(parsed.sections[1].cases.size() == 2 && parsed.sections[1].cases[0].level == 3);
  // A case needs a save above it, a save its name
  CHECK(FailsOnLine("at L0 (1,1) facing N light 0\nsave s\n", "line 1"));
  CHECK(FailsOnLine("save s\nat L0 (1,1) facing N light 0\nsave\n", "line 3"));
  CHECK(FailsOnLine("save s t\n", "line 1"));
}

void TestAutoMapVisibilityCases(void)
{
  ACaseFileGivesTheSaveTheCasesAndTheirChecks();
  AMistakeIsReportedWithItsLine();
  ACheckIsWrittenAsTheFileWritesIt();
  AnItemIsNamedAndPlacedOnItsTile();
  AnAlcoveIsNamedByItsWallFace();
  ATileIsNamedByItsCell();
  EachSaveHasTheCasesBelowIt();
}
