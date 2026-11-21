#ifndef AUTOMAPLEGEND_H
#define AUTOMAPLEGEND_H

#include <string>
#include <vector>

// The SYMBOLS tab of the map's ? panel: every mark the map draws, a sample and a word on it. The
// window draws each sample with its own painters on made-up records (AutoMapWindow_DrawLegendSample);
// this unit holds the rows and where they go. The exported page has the same rows in the same
// order (LEGEND in docs/automap-mockup/page/ui.js); LegendTest holds the two to each other.
// No engine, no SDL.

enum AutoMapLegendSample
{
  AutoMapLegendHeading,
  // cells
  AutoMapLegendRock,
  AutoMapLegendFloor,
  AutoMapLegendExplored,
  AutoMapLegendWalked,
  AutoMapLegendDoor,
  AutoMapLegendOpenDoor,
  AutoMapLegendBrokenDoor,
  AutoMapLegendSealedDoor,
  AutoMapLegendAxeDoor,
  AutoMapLegendFireballDoor,
  AutoMapLegendLinkedDoor,
  AutoMapLegendStairsUp,
  AutoMapLegendStairsDown,
  AutoMapLegendPit,
  AutoMapLegendPitZoomedOut,
  AutoMapLegendHiddenPit,
  AutoMapLegendHiddenPitOpen,
  AutoMapLegendSwitchablePit,
  AutoMapLegendFalseWallSolid,
  AutoMapLegendFalseWallWalkThrough,
  AutoMapLegendFalseWallOpen,
  AutoMapLegendTeleporter,
  AutoMapLegendTeleporterToLevel,
  AutoMapLegendInvisibleTeleporter,
  AutoMapLegendDormantTeleporter,
  AutoMapLegendSpinnerRight,
  AutoMapLegendSpinnerLeft,
  AutoMapLegendSpinnerAround,
  AutoMapLegendSpinnerFaceNorth,
  AutoMapLegendSpinnerKeeps,
  AutoMapLegendDormantSpinner,
  // walls
  AutoMapLegendSwitch,
  AutoMapLegendDecoration,
  AutoMapLegendHiddenSwitch,
  AutoMapLegendWriting,
  AutoMapLegendAlcove,
  AutoMapLegendLock,
  // pads and who sets them off
  AutoMapLegendPadAnything,
  AutoMapLegendPadParty,
  AutoMapLegendPadPartyFacing,
  AutoMapLegendPadMonsters,
  AutoMapLegendPadPartyOrMonsters,
  AutoMapLegendPadItem,
  AutoMapLegendPadCarried,
  AutoMapLegendHiddenPad,
  AutoMapLegendDormantPad,
  AutoMapLegendTeleporterPartyObjects,
  AutoMapLegendTeleporterObjects,
  AutoMapLegendTeleporterMonsters,
  // things
  AutoMapLegendParty,
  AutoMapLegendMonsters,
  AutoMapLegendGenerator,
  AutoMapLegendGeneratorAsleep,
  AutoMapLegendGeneratorSpent,
  AutoMapLegendItems,
  AutoMapLegendGlimpse,
  AutoMapLegendMissile,
  AutoMapLegendCloud,
  AutoMapLegendFluxCage,
  // hovering a cell
  AutoMapLegendSends,
  AutoMapLegendFeeds,
  AutoMapLegendSets,
  AutoMapLegendClears,
  AutoMapLegendToggles,
  AutoMapLegendHolds,
  AutoMapLegendOnce,
  AutoMapLegendLearned,
  AutoMapLegendGoalNow,
  AutoMapLegendGoalWaits,
  AutoMapLegendDoorPreview,
  AutoMapLegendTeleportLine,
  AutoMapLegendDormantTeleportLine,
  // your marks, the search
  AutoMapLegendPitNote,
  AutoMapLegendPitNoteOpen,
  AutoMapLegendPlayerLink,
  AutoMapLegendSearchHit,
  AutoMapLegendChosenHit
};

struct AutoMapLegendEntry
{
  const char *label;
  AutoMapLegendSample sample;
};

// The rows in order: a heading (sample AutoMapLegendHeading), then the samples under it
const std::vector<AutoMapLegendEntry> &AutoMapLegend_Entries(void);

// A sample is two cells of this size wide and one high; a single cell sits in its middle
const int AUTOMAP_LEGEND_CELL = 32;
const int AUTOMAP_LEGEND_ROW = AUTOMAP_LEGEND_CELL + 6;
const int AUTOMAP_LEGEND_HEADING = 22;
const int AUTOMAP_LEGEND_MOST_COLUMNS = 3;

struct AutoMapLegendPlace
{
  // Index into AutoMapLegend_Entries
  size_t entry;
  // 0 for a heading, which spans every column
  int column;
  // From the top of the list
  int y;
};

struct AutoMapLegendLayout
{
  int columnWidth;
  int columns;
  int height;
  std::vector<AutoMapLegendPlace> places;
};

// As the page's legendLayout: as many columns of columnWidth (the sample, a gap, the widest label
// at textPixel) as fit in width, at least one, at most three. Each heading's rows fill its
// columns top to bottom, so related rows stay together.
AutoMapLegendLayout AutoMapLegend_Layout(int width, int textPixel);

#endif
