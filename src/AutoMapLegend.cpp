// AutoMapLegend.cpp : see AutoMapLegend.h
#include "AutoMapLegend.h"

#include <algorithm>

#include "AutoMapScene.h"

const std::vector<AutoMapLegendEntry> &AutoMapLegend_Entries(void)
{
  static const AutoMapLegendEntry ROWS[] = {
      {"CELLS", AutoMapLegendHeading},
      {"ROCK", AutoMapLegendRock},
      {"FLOOR", AutoMapLegendFloor},
      {"EXPLORED (VISION ALL, TINT)", AutoMapLegendExplored},
      {"WALKED BY THE PARTY", AutoMapLegendWalked},
      {"DOOR", AutoMapLegendDoor},
      {"OPEN DOOR", AutoMapLegendOpenDoor},
      {"BROKEN DOOR", AutoMapLegendBrokenDoor},
      {"SEALED DOOR (SPOILERS ALL)", AutoMapLegendSealedDoor},
      {"DOOR AN AXE BREAKS", AutoMapLegendAxeDoor},
      {"DOOR A FIREBALL BREAKS", AutoMapLegendFireballDoor},
      {"A SWITCH OR PAD WORKS IT", AutoMapLegendLinkedDoor},
      {"STAIRS UP, TO THE LEVEL SHOWN", AutoMapLegendStairsUp},
      {"STAIRS DOWN", AutoMapLegendStairsDown},
      {"PIT", AutoMapLegendPit},
      {"PIT, ZOOMED OUT", AutoMapLegendPitZoomedOut},
      {"PIT THAT LOOKS LIKE FLOOR", AutoMapLegendHiddenPit},
      {"THE SAME, OPEN", AutoMapLegendHiddenPitOpen},
      {"PIT A SWITCH CAN OPEN", AutoMapLegendSwitchablePit},
      {"FALSE WALL, SOLID", AutoMapLegendFalseWallSolid},
      {"FALSE WALL, WALK-THROUGH", AutoMapLegendFalseWallWalkThrough},
      {"FALSE WALL, OPEN", AutoMapLegendFalseWallOpen},
      {"TELEPORTER", AutoMapLegendTeleporter},
      {"TELEPORTER TO ANOTHER LEVEL", AutoMapLegendTeleporterToLevel},
      {"INVISIBLE TELEPORTER", AutoMapLegendInvisibleTeleporter},
      {"DORMANT TELEPORTER", AutoMapLegendDormantTeleporter},
      {"SPINNER: TURNS RIGHT", AutoMapLegendSpinnerRight},
      {"SPINNER: TURNS LEFT", AutoMapLegendSpinnerLeft},
      {"SPINNER: TURNS AROUND", AutoMapLegendSpinnerAround},
      {"SPINNER: FACE NORTH", AutoMapLegendSpinnerFaceNorth},
      {"SPINNER: KEEPS FACING", AutoMapLegendSpinnerKeeps},
      {"DORMANT SPINNER", AutoMapLegendDormantSpinner},
      {"WALLS", AutoMapLegendHeading},
      {"SWITCH, LEVER, BUTTON", AutoMapLegendSwitch},
      {"DECORATION", AutoMapLegendDecoration},
      {"HIDDEN SWITCH", AutoMapLegendHiddenSwitch},
      {"WALL WRITING", AutoMapLegendWriting},
      {"ALCOVE AND WHAT LIES IN IT", AutoMapLegendAlcove},
      {"LOCK AND THE KEY IT WANTS", AutoMapLegendLock},
      {"PADS AND WHO SETS THEM OFF", AutoMapLegendHeading},
      {"PAD: ANYTHING", AutoMapLegendPadAnything},
      {"THE PARTY", AutoMapLegendPadParty},
      {"THE PARTY FACING EAST", AutoMapLegendPadPartyFacing},
      {"MONSTERS", AutoMapLegendPadMonsters},
      {"THE PARTY OR MONSTERS", AutoMapLegendPadPartyOrMonsters},
      {"THIS ITEM LYING ON IT", AutoMapLegendPadItem},
      {"THE PARTY CARRYING THIS", AutoMapLegendPadCarried},
      {"HIDDEN PAD", AutoMapLegendHiddenPad},
      {"PAD THAT CANNOT FIRE", AutoMapLegendDormantPad},
      {"TELEPORTER: PARTY, OBJECTS", AutoMapLegendTeleporterPartyObjects},
      {"TELEPORTER: OBJECTS", AutoMapLegendTeleporterObjects},
      {"TELEPORTER: MONSTERS", AutoMapLegendTeleporterMonsters},
      {"THINGS", AutoMapLegendHeading},
      {"THE PARTY", AutoMapLegendParty},
      {"MONSTERS, WHERE THEY FACE", AutoMapLegendMonsters},
      {"GENERATOR", AutoMapLegendGenerator},
      {"GENERATOR, ASLEEP", AutoMapLegendGeneratorAsleep},
      {"GENERATOR, SPENT", AutoMapLegendGeneratorSpent},
      {"ITEMS, MORE THAN THREE", AutoMapLegendItems},
      {"AN ITEM ONLY GLIMPSED", AutoMapLegendGlimpse},
      {"FLYING: THROWN, SPELL", AutoMapLegendMissile},
      {"CLOUD", AutoMapLegendCloud},
      {"FLUXCAGE", AutoMapLegendFluxCage},
      {"HOVERING A CELL", AutoMapLegendHeading},
      {"WHAT IT SENDS", AutoMapLegendSends},
      {"WHAT FEEDS IT", AutoMapLegendFeeds},
      {"SETS: OPENS, WAKES", AutoMapLegendSets},
      {"CLEARS: CLOSES, STOPS", AutoMapLegendClears},
      {"TOGGLES", AutoMapLegendToggles},
      {"HOLDS WHILE PRESSED", AutoMapLegendHolds},
      {"WORKS ONCE ONLY", AutoMapLegendOnce},
      {"SEEN TO WORK (LEARNED)", AutoMapLegendLearned},
      {"GOAL: HAPPENS NOW, SOON, DONE", AutoMapLegendGoalNow},
      {"GOAL: WAITS ON SOMETHING", AutoMapLegendGoalWaits},
      {"THE DOOR AS IT WILL BE", AutoMapLegendDoorPreview},
      {"WHERE A TELEPORTER SENDS", AutoMapLegendTeleportLine},
      {"WHERE IT WOULD, WHEN WOKEN", AutoMapLegendDormantTeleportLine},
      {"YOUR MARKS, THE SEARCH", AutoMapLegendHeading},
      {"YOUR PIT MARK", AutoMapLegendPitNote},
      {"YOUR PIT MARK, OPEN", AutoMapLegendPitNoteOpen},
      {"YOUR LINK", AutoMapLegendPlayerLink},
      {"SEARCH HIT", AutoMapLegendSearchHit},
      {"CHOSEN HIT, OR WHERE YOU LANDED", AutoMapLegendChosenHit},
  };
  static const std::vector<AutoMapLegendEntry> rows(ROWS, ROWS + sizeof(ROWS) / sizeof(ROWS[0]));
  return rows;
}

AutoMapLegendLayout AutoMapLegend_Layout(int width, int textPixel)
{
  const std::vector<AutoMapLegendEntry> &rows = AutoMapLegend_Entries();
  int labelWidth = 0;
  for (size_t i = 0; i < rows.size(); i++)
  {
    if (rows[i].sample != AutoMapLegendHeading)
      labelWidth = std::max(labelWidth, AutoMapScene_TextWidth(rows[i].label) * textPixel);
  }
  AutoMapLegendLayout layout;
  layout.columnWidth = 2 * AUTOMAP_LEGEND_CELL + 10 + labelWidth + 18;
  layout.columns = std::max(1, std::min(AUTOMAP_LEGEND_MOST_COLUMNS, (width - 20) / layout.columnWidth));
  int y = 0;
  size_t i = 0;
  while (i < rows.size())
  {
    AutoMapLegendPlace heading = {i, 0, y};
    layout.places.push_back(heading);
    y += AUTOMAP_LEGEND_HEADING;
    size_t first = ++i;
    while (i < rows.size() && rows[i].sample != AutoMapLegendHeading) i++;
    int count = (int)(i - first);
    int perColumn = (count + layout.columns - 1) / layout.columns;
    for (int k = 0; k < count; k++)
    {
      AutoMapLegendPlace place = {first + k, k / perColumn, y + (k % perColumn) * AUTOMAP_LEGEND_ROW};
      layout.places.push_back(place);
    }
    y += perColumn * AUTOMAP_LEGEND_ROW + 6;
  }
  layout.height = y;
  return layout;
}
