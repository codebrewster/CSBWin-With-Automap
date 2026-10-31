// AutoMapHooks.cpp : connects the engine's game loop and saved games with the automap.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#ifdef MINGW
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "AutoMapFile.h"
#include "AutoMapItemMemory.h"
#include "AutoMapKnowledge.h"
#include "AutoMapLevelReader.h"
#include "AutoMapMeasure.h"
#include "AutoMapRecorder.h"
#include "AutoMapScene.h"
#include "AutoMapVisibilityCases.h"
#include "AutoMapWindow.h"
#include "confile.h"
#include "HeroStats.h"
#include "HeroStatsOrigins.h"
#include "HeroStatsHooks.h"

struct SavedGameFile
{
  // As the engine found it in its folders; empty when there is no such file
  std::string path;
  AutoMapSaveSignature signature;
};

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include "AutoMapHooks.h"

char *GETFILENAME(i32 f);
void AddToSkill(i32 chIdx, i32 skillNumber, i32 experience);

static const int LEVEL_OF_THE_PRISON_SCREEN = 255;
i32 GetPixel(pnt, i16, i16, i16); // Chaos.cpp
static const char *MAP_FILE_SUFFIX = ".map";

static AutoMapRecorder recorder;
// csb --visibility-test: the run takes over on the first tick the party's level is loaded
static bool visibilityTestIsPending = false;
static void RunVisibilityTest(void);
static AutoMapLevelSnapshot lastSnapshot;
// A rebuild walks every cell of the level: live enough at a few hertz, immediate when
// the shown level changes
static const Uint32 SMALLEST_REBUILD_GAP = 150;
static Uint32 lastRebuildTicks = 0;
static int lastBuiltLevel = AUTOMAP_NO_LEVEL;
// map_vision of csb.cfg: 0 fog of war, 1 explored, live, 2 whole level
static const int VISION_FOG_SETTING = 0;
static const int VISION_LIVE_SETTING = 1;
static const int VISION_WHOLE_LEVEL_SETTING = 2;
static int visionSetting = VISION_FOG_SETTING;
static bool exploredIsHighlighted = false;
// map_spoilers of csb.cfg: 0 seen, 1 learned, 2 all
static const int SPOILERS_SEEN_SETTING = 0;
static const int SPOILERS_LEARNED_SETTING = 1;
static const int SPOILERS_ALL_SETTING = 2;
static int spoilerSetting = SPOILERS_LEARNED_SETTING;
// The map follows the party unless locked to one level to look above or below
static bool followParty = true;
static int lockedLevel = 0;
static std::string pathOfSaveBeforeRotation;
static std::string pathOfBackupBeforeRestore;

// ---------------------------------------------------------------------------------------------
// The game loop

// The prison door screen draws the 3D view over a mock level, and the engine loads other
// levels while it works on them
static bool PartyLevelIsLoaded(void)
{
  return d.partyLevel != LEVEL_OF_THE_PRISON_SCREEN && d.LoadedLevel == d.partyLevel;
}

// The scene numbers its levels the other way round, so that a fog without them shows all
static AutoMapSpoilerLevel SpoilersOfSetting(void)
{
  if (spoilerSetting == SPOILERS_SEEN_SETTING) return AutoMapSpoilersSeen;
  if (spoilerSetting == SPOILERS_ALL_SETTING) return AutoMapSpoilersAll;
  return AutoMapSpoilersLearned;
}

static int ValidVisionSetting(int setting)
{
  if (setting < VISION_FOG_SETTING || setting > VISION_WHOLE_LEVEL_SETTING) return VISION_FOG_SETTING;
  return setting;
}

static int ValidSpoilerSetting(int setting)
{
  if (setting < SPOILERS_SEEN_SETTING || setting > SPOILERS_ALL_SETTING) return SPOILERS_LEARNED_SETTING;
  return setting;
}

static void ShowLastSnapshot(void)
{
  // VisibleNow only ever describes the party's level: on any other level no monster
  // is on a cell the party sees right now
  static const AutoMapVisibleCells nothingVisibleNow;
  const AutoMapVisibleCells *visibleNow = &recorder.VisibleNow();
  if (lastSnapshot.level != d.partyLevel) visibleNow = &nothingVisibleNow;
  // No frame has measured a monster of another level
  static const std::vector<AutoMapRecordId> noMonstersSeenNow;
  const std::vector<AutoMapRecordId> *monstersSeenNow = &recorder.MonstersSeenNow();
  if (lastSnapshot.level != d.partyLevel) monstersSeenNow = &noMonstersSeenNow;
  AutoMapFog fog = {visionSetting != VISION_WHOLE_LEVEL_SETTING, &recorder.Explored(), visibleNow,
                    exploredIsHighlighted,
                    SpoilersOfSetting(), &recorder.Knowledge(), &recorder.Items(), monstersSeenNow,
                    &recorder.Cells(), &recorder.Notes(), &recorder.Damage(),
                    visionSetting == VISION_LIVE_SETTING};
  // A cell explored outside a frame has no look yet: it keeps the one it has now
  AutoMapScene_RememberExploredLooks(lastSnapshot, recorder.Explored(), &recorder.Cells());
  AutoMapWindow_ShowDrawList(AutoMapScene_BuildDrawList(lastSnapshot, fog));
}

static void ShowNoLevel(void)
{
  if (lastSnapshot.level == AUTOMAP_NO_LEVEL) return;
  lastSnapshot = AutoMapLevelSnapshot();
  lastBuiltLevel = AUTOMAP_NO_LEVEL;
  ShowLastSnapshot();
}

// Time the automap's measuring takes in one frame (the light cut, the draw hooks, the end of the
// view), added up for map_timing, which reports it so the spec's cost claim can be checked
static Uint64 measuringCounts = 0;

class MeasuringTimer
{
public:
  MeasuringTimer() : isOn(AutoMapWindow_TimingReportsOn()), start(isOn ? SDL_GetPerformanceCounter() : 0) {}
  ~MeasuringTimer()
  {
    if (isOn) measuringCounts += SDL_GetPerformanceCounter() - start;
  }

private:
  bool isOn;
  Uint64 start;
};

// How many rows ahead the light on screen reaches, from the level's own front wall pictures in
// the palette shown. Kept until the level, the palette or the wall set changes.
static int cachedLevel = -1;

static int LightReachOfLoadedLevel(void)
{
  int shown = AutoMap_ShownPalette(d.CurrentPalette);
  if (shown < 0 || shown > 5) return AUTOMAP_REACH_ROWS;
  static int cachedPalette = -1;
  static int cachedWallSet = -1;
  static int cachedReach = AUTOMAP_REACH_ROWS;
  if (cachedLevel == d.LoadedLevel && cachedPalette == shown && cachedWallSet == d.CurrentWallGraphic)
  {
    return cachedReach;
  }
  // F1, F2, F3: the unmirrored front wall pictures and their size rectangles (Code390e.cpp)
  pnt pictures[AUTOMAP_REACH_ROWS] = {d.Pointer2242, d.Pointer2238, d.Pointer2234};
  const int sizeRectangle[AUTOMAP_REACH_ROWS] = {6, 3, 0};
  int counts[AUTOMAP_REACH_ROWS][16] = {{0}};
  for (int row = 0; row < AUTOMAP_REACH_ROWS; row++)
  {
    int byteWidth = d.wallRectangles[sizeRectangle[row]].b.uByte4;
    int height = d.wallRectangles[sizeRectangle[row]].b.uByte5;
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < 2 * byteWidth; x++)
      {
        counts[row][GetPixel(pictures[row], sw(x), sw(y), sw(byteWidth)) & 15]++;
      }
    }
  }
  const short *palette = d.Palette552[shown].color;
  cachedLevel = d.LoadedLevel;
  cachedPalette = shown;
  cachedWallSet = d.CurrentWallGraphic;
  cachedReach = AutoMapSight_LightReach(counts, palette);
  if (AutoMapWindow_TimingReportsOn())
  {
    std::printf("Automap: light reach L%d palette %d: %d (F1 %d%% F2 %d%% F3 %d%%)\n", d.LoadedLevel,
                shown, cachedReach, AutoMapSight_LitPercent(counts[0], palette),
                AutoMapSight_LitPercent(counts[1], palette), AutoMapSight_LitPercent(counts[2], palette));
  }
  return cachedReach;
}

// The doors of the frame by record: DrawDoor knows the record, not the cell
struct FrameDoor { int door; int x; int y; };
static std::vector<FrameDoor> doorsOfFrame;

void AutoMap_OnViewportDrawn(const SUMMARIZEROOMDATA *cellsInView)
{
  MeasuringTimer timer;
  AutoMapMeasure_OnViewportSummarised(cellsInView);
  doorsOfFrame.clear();
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    if (cellsInView[i].roomType != roomDOORFACING) continue;
    FrameDoor door = {cellsInView[i].decorations[1], cellsInView[i].x, cellsInView[i].y};
    doorsOfFrame.push_back(door);
  }
  // The sweep's views are not the party's: they teach the map nothing
  if (AutoMapMeasure_IsRunning()) return;
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) ShowNoLevel();
  if (!PartyLevelIsLoaded()) return;
  recorder.RecordViewport(cellsInView, LightReachOfLoadedLevel());
}

// ---------------------------------------------------------------------------------------------
// What the party finds out

static void Learn(const AutoMapFact &fact)
{
  recorder.Knowledge().Learn(fact);
}

static void LearnRecord(AutoMapFactKind kind, int record)
{
  AutoMapRecordId id = AutoMapLevelReader_RecordId(record);
  if (id != AUTOMAP_NO_RECORD) Learn(AutoMapFact_OfRecord(kind, id));
}

static void LearnCell(AutoMapFactKind kind, int level, int x, int y)
{
  AutoMapCell cell = {level, x, y};
  Learn(AutoMapFact_OfCell(kind, cell));
}

// A known actuator teaches the cells it changes when the party sees them change
static void LearnActuatorAndWatchItsLinks(int actuator)
{
  AutoMapRecordId id = AutoMapLevelReader_RecordId(actuator);
  if (id == AUTOMAP_NO_RECORD) return;
  Learn(AutoMapFact_OfRecord(AutoMapFactActuator, id));
  recorder.LinkWatch().Start(id, AutoMapLevelReader_ReadLevel(d.partyLevel), d.Time);
}

void AutoMap_OnPortraitFaceDrawn(int drawLocation)
{
  if (AutoMapMeasure_IsRunning() || !PartyLevelIsLoaded()) return;
  recorder.MarkDrawnDecoration(drawLocation);
  // Only drawn on the wall straight ahead, like the words of wall text: the same bit says so
  recorder.MarkTextRead(drawLocation);
}

void AutoMap_OnWallTextRead(int drawLocation)
{
  if (AutoMapMeasure_IsRunning() || !PartyLevelIsLoaded()) return;
  recorder.MarkDrawnDecoration(drawLocation);
  recorder.MarkTextRead(drawLocation);
}

void AutoMap_BeforeWallDecorationDrawn(int drawLocation, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_AfterWallDecorationDrawn(int drawLocation, int x1, int x2, int y1, int y2, bool isText)
{
  MeasuringTimer timer;
  AutoMapDrawKey key = {AutoMapDrawDecoration, drawLocation, AUTOMAP_NO_RECORD,
                        isText ? AUTOMAP_SCRIBBLES_DRAWN : -1, -1, -1};
  AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
}

static void PositionOfViewCell(int cellNumber, int *x, int *y);

void AutoMap_BeforeFloorDecorationDrawn(int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

// What lies on a floor, a picture or stairs, is measured as one thing of its cell
static void AfterFloorThingDrawn(int viewCell, int x1, int x2, int y1, int y2)
{
  int x = -1, y = -1;
  PositionOfViewCell(viewCell, &x, &y);
  AutoMapDrawKey key = {AutoMapDrawFloorDecoration, -1, AUTOMAP_NO_RECORD, -1, x, y};
  AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
}

void AutoMap_AfterFloorDecorationDrawn(int relativeCell, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  // The view cell of each relative cell (AutoMapSight.h numbers them): left, centre, right of
  // three ahead, then of two ahead and one ahead; the right ones (2, 5, 8) are drawn mirrored
  static const int VIEW_CELL[9] = {7, 9, 8, 12, 14, 13, 15, 17, 16};
  if (relativeCell < 0 || relativeCell > 8) return;
  AfterFloorThingDrawn(VIEW_CELL[relativeCell], x1, x2, y1, y2);
}

void AutoMap_BeforeStairsDrawn(int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_AfterStairsDrawn(int viewCell, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AfterFloorThingDrawn(viewCell, x1, x2, y1, y2);
}

// The cell whose objects the 3D view draws now: DrawSingleObject does not know it
static int objectCellX = -1;
static int objectCellY = -1;

void AutoMap_OnObjectCellDrawn(int x, int y)
{
  objectCellX = x;
  objectCellY = y;
}

static AutoMapRememberedItem ItemOnObjectCell(int item)
{
  AutoMapFloorItemTag tag = AutoMapLevelReader_FloorItemTag(item);
  AutoMapRememberedItem remembered = {tag.record, tag.descIndex, {d.partyLevel, objectCellX, objectCellY}};
  return remembered;
}

// An item already seen where it lies is not measured again, so a hoard costs nothing after the
// first look; a mere glimpse is, until the light shows what it is. The sweep measures every item: its CSV must be complete. With map_timing
// set every item is measured too, so a dropped or remembered item still reports its pixels.
void AutoMap_BeforeItemDrawn(int item, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  bool measuresAll = AutoMapMeasure_IsRunning() || AutoMapWindow_TimingReportsOn();
  if (!measuresAll && recorder.Items().Identifies(ItemOnObjectCell(item))) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_AfterItemDrawn(int item, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapRememberedItem drawn = ItemOnObjectCell(item);
  AutoMapDrawKey key = {AutoMapDrawItem, -1, drawn.record, drawn.descIndex, drawn.cell.x, drawn.cell.y};
  AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
}

void AutoMap_BeforeAlcoveItemDrawn(int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_AfterAlcoveItemDrawn(int item, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapRememberedItem drawn = ItemOnObjectCell(item);
  AutoMapDrawKey key = {AutoMapDrawAlcoveItem, -1, drawn.record, drawn.descIndex, drawn.cell.x, drawn.cell.y};
  AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
}

void AutoMap_OnViewportBlit(const unsigned char *src, int x1, int x2, int y1, int y2,
                            int srcOffsetX, int srcOffsetY, int srcByteWidth, int transparentColor)
{
  MeasuringTimer timer;
  AutoMapMeasure_OnViewportBlit(src, x1, x2, y1, y2, srcOffsetX, srcOffsetY, srcByteWidth, transparentColor);
}

// Both blits of DrawMonster, a group's members draw one after the other under one key
void AutoMap_BeforeMonsterDrawn(int group, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_BeforeDoorDrawn(int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapMeasure_BeforeBlit(x1, x2, y1, y2);
}

void AutoMap_AfterDoorDrawn(int door, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  for (size_t i = 0; i < doorsOfFrame.size(); i++)
  {
    if (doorsOfFrame[i].door != door) continue;
    AutoMapDrawKey key = {AutoMapDrawDoor, -1, AUTOMAP_NO_RECORD, -1, doorsOfFrame[i].x, doorsOfFrame[i].y};
    AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
    return;
  }
}

// Debug: each drawn group's position byte, for the map_timing trace
static std::map<AutoMapRecordId, int> monsterPositionsOfFrame;

void AutoMap_AfterMonsterDrawn(int group, int x1, int x2, int y1, int y2)
{
  MeasuringTimer timer;
  if (!PartyLevelIsLoaded()) return;
  AutoMapDrawKey key = {AutoMapDrawMonster, -1, AutoMapLevelReader_RecordId(group), -1, -1, -1};
  if (AutoMapWindow_TimingReportsOn())
  {
    RN record;
    record.ConstructFromInteger(group);
    monsterPositionsOfFrame[key.record] = MonsterPositionByte(GetRecordAddressDB4(record), d.partyLevel, false) & 255;
  }
  AutoMapMeasure_AfterBlit(key, x1, x2, y1, y2);
}

std::map<int, int> AutoMap_ExploredTilesPerLevel(void)
{
  std::map<int, int> perLevel;
  const AutoMapExplored &explored = recorder.Explored();
  std::vector<std::pair<AutoMapCell, AutoMapCellLook> > looks = recorder.Cells().Looks();
  for (size_t i = 0; i < looks.size(); i++)
  {
    const AutoMapCell &cell = looks[i].first;
    if (cell.level < 0 || cell.level >= explored.LevelCount()) continue;
    AutoMapLevelSize size = explored.SizeOfLevel(cell.level);
    if (cell.x < 0 || cell.y < 0 || cell.x >= size.width || cell.y >= size.height) continue;  // the rim
    if (!explored.IsExplored(cell)) continue;
    AutoMapCellKind kind = looks[i].second.kind;
    if (kind == AutoMapCellWall || kind == AutoMapCellFalseWall) continue;
    perLevel[cell.level]++;
  }
  return perLevel;
}

// The read bit marks wall texts and champion portraits seen straight ahead (the 3D view draws
// both only there, AutoMap_OnPortraitFaceDrawn); the looked-into bit of a portrait face marks the
// champion viewed (AutoMap_OnPortraitViewed). The level's sides tell writing from portraits.
// Reading a level is not cheap, so a level is read again only when its bits changed.
std::map<int, AutoMapWallCounts> AutoMap_WallCountsPerLevel(void)
{
  struct LevelCount { std::vector<uint8_t> readBits, lookedIntoBits; AutoMapWallCounts counts; };
  static std::map<int, LevelCount> cache;
  std::map<int, AutoMapWallCounts> perLevel;
  const AutoMapExplored &explored = recorder.Explored();
  for (int level = 0; level < explored.LevelCount(); level++)
  {
    const std::vector<uint8_t> &readBits = explored.ReadTextBitsOfLevel(level);
    const std::vector<uint8_t> &lookedIntoBits = explored.LookedIntoBitsOfLevel(level);
    bool anySet = false;
    for (size_t i = 0; i < readBits.size() && !anySet; i++) anySet = readBits[i] != 0;
    for (size_t i = 0; i < lookedIntoBits.size() && !anySet; i++) anySet = lookedIntoBits[i] != 0;
    if (!anySet)
    {
      cache.erase(level);
      continue;
    }
    std::map<int, LevelCount>::iterator cached = cache.find(level);
    if (cached == cache.end() || cached->second.readBits != readBits || cached->second.lookedIntoBits != lookedIntoBits)
    {
      AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(level);
      AutoMapLevelSize size = explored.SizeOfLevel(level);
      AutoMapWallCounts counts = {0, 0, 0};
      for (int y = 0; y < size.height; y++)
        for (int x = 0; x < size.width; x++)
        {
          int sx = x + snapshot.boundaryMarginX, sy = y + snapshot.boundaryMarginY;
          if (sx < 0 || sy < 0 || sx >= snapshot.width || sy >= snapshot.height) continue;
          const AutoMapCellRecord &record = snapshot.cells[sy * snapshot.width + sx];
          for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
          {
            AutoMapCell cell = {level, x, y};
            AutoMapScene_CountWallSide(record.wallSides[side], explored.TextIsRead(cell, side),
                                       explored.AlcoveIsLookedInto(cell, side), &counts);
          }
        }
      LevelCount fresh = {readBits, lookedIntoBits, counts};
      cached = cache.insert(std::make_pair(level, fresh)).first;
      cached->second = fresh;
    }
    const AutoMapWallCounts &counts = cached->second.counts;
    if (counts.textsRead > 0 || counts.portraitsSeen > 0 || counts.portraitsViewed > 0) perLevel[level] = counts;
  }
  return perLevel;
}

// Picked up, the item is gone from where the map showed it
void AutoMap_OnItemPickedUp(int item)
{
  if (!PartyLevelIsLoaded()) return;
  AutoMapRecordId record = AutoMapLevelReader_RecordId(item);
  if (record != AUTOMAP_NO_RECORD) recorder.Items().ForgetItem(record);
}

// The party knows what it dropped, seen or not. An item that did not stay on the floor (into a
// pit, into an alcove, whose items follow the face) is not there to remember.
void AutoMap_OnItemDropped(int item, int x, int y)
{
  if (!PartyLevelIsLoaded()) return;
  AutoMapRecordId record = AutoMapLevelReader_RecordId(item);
  if (record == AUTOMAP_NO_RECORD) return;
  std::vector<AutoMapFloorItemTag> itemsThere = AutoMapLevelReader_FloorItemsAt(x, y);
  for (size_t i = 0; i < itemsThere.size(); i++)
  {
    if (itemsThere[i].record != record) continue;
    AutoMapRememberedItem dropped = {record, itemsThere[i].descIndex, {d.partyLevel, x, y}};
    recorder.Items().See(dropped);
    return;
  }
  recorder.Items().ForgetItem(record);
}

// The party knows what it put into an alcove, seen or not, as it knows what it dropped
void AutoMap_OnItemPutIntoAlcove(int item, int x, int y)
{
  if (!PartyLevelIsLoaded()) return;
  AutoMapFloorItemTag tag = AutoMapLevelReader_FloorItemTag(item);
  if (tag.record == AUTOMAP_NO_RECORD) return;
  recorder.PutIntoAlcove(x, y, tag.record, tag.descIndex);
}

// For the visibility tests: the numbers each draw of the last frame was judged by, taken while
// the frame is on screen, as DrawViewport clears it after
struct DrawMeasurement
{
  AutoMapDrawKey key;
  // Contrasting pixels for a decoration, pixels standing out for the rest
  int counted;
  int drawn;
  int largestPatch;
  // A decoration's mean difference from the wall it covers, hundredths of a step, and its patch
  // of tinted pixels a step off the wall; -1 for the rest
  int darkening;
  int tintPatch;
};
static std::vector<DrawMeasurement> measurementsOfFrame;

static void KeepMeasurement(const AutoMapDrawKey &key, int counted, int drawn, int largestPatch,
                            int darkening = -1, int tintPatch = -1)
{
  if (!visibilityTestIsPending) return;
  DrawMeasurement measurement = {key, counted, drawn, largestPatch, darkening, tintPatch};
  measurementsOfFrame.push_back(measurement);
}

// A decoration counts as seen when the frame shows enough of it in the light on screen, the
// debug light included; a floor item likewise is remembered where it lies
static void LearnFromViewportDraws(void)
{
  MeasuringTimer timer;
  AutoMapMeasure_OnViewportFinished();
  if (AutoMapMeasure_IsRunning() || !PartyLevelIsLoaded()) return;
  int shown = AutoMap_ShownPalette(d.CurrentPalette);
  if (shown < 0 || shown > 5) return;
  const short *palette = d.Palette552[shown].color;
  const std::vector<AutoMapDrawOnScreen> &draws = AutoMapMeasure_DrawsOfFrame();
  measurementsOfFrame.clear();
  // The trace gives global coordinates, as the map's rulers do
  int offsetX = 0;
  int offsetY = 0;
  AutoMapLevelReader_LevelOffset(d.partyLevel, &offsetX, &offsetY);
  std::string trace;
  std::string itemTrace;
  std::string monsterTrace;
  bool hasDecorations = false;
  for (size_t i = 0; i < draws.size(); i++)
  {
    const AutoMapDrawKey &key = draws[i].key;
    char entry[64];
    if (key.kind == AutoMapDrawDecoration)
    {
      AutoMapDrawView view = AutoMapMeasure_ViewOf(key, palette);
      int contrasting = AutoMapSight_ContrastingPixels(view, NULL);
      int kept = (int)std::count(view.isDrawn.begin(), view.isDrawn.end(), true);
      int largestPatch = AutoMapSight_ShapeOfItem(view, NULL).largestPatch;
      int darkening = AutoMapSight_DecorationDarkening(view);
      int tintPatch = AutoMapSight_DecorationTintPatch(view);
      std::snprintf(entry, sizeof(entry), " %d:%d of %d patch %d tint patch %d blot %d", key.drawLocation,
                    contrasting, kept, largestPatch, tintPatch, darkening);
      KeepMeasurement(key, contrasting, kept, largestPatch, darkening, tintPatch);
      trace += entry;
      hasDecorations = true;
      bool isSeen = key.descIndex == AUTOMAP_SCRIBBLES_DRAWN ? AutoMapSight_ScribblesAreSeen(view)
                                                             : AutoMapSight_DecorationIsSeen(view);
      if (!isSeen) continue;
      recorder.MarkDrawnDecoration(key.drawLocation);
    }
    else if (key.kind == AutoMapDrawItem)
    {
      AutoMapItemShape shape = AutoMapSight_ShapeOfItem(AutoMapMeasure_ViewOf(key, palette), NULL);
      std::snprintf(entry, sizeof(entry), " %u@%d,%d:%d of %d patch %d", (unsigned)key.record,
                    key.x + offsetX, key.y + offsetY, shape.visible, shape.drawn, shape.largestPatch);
      itemTrace += entry;
      KeepMeasurement(key, shape.visible, shape.drawn, shape.largestPatch);
      if (shape.largestPatch < AUTOMAP_SEEN_ITEM_PATCH) continue;
      // Beyond the light only that something lies there shows, not what
      AutoMapRememberedItem seen = {key.record, key.descIndex, {d.partyLevel, key.x, key.y},
                                    !recorder.IsLitInFrame(key.x, key.y)};
      recorder.Items().See(seen);
    }
    else if (key.kind == AutoMapDrawAlcoveItem)
    {
      // Judged like a floor item, against the alcove it lies in: a scroll that shows in the
      // dark beyond the light's reach still shows what the alcove holds
      AutoMapItemShape shape = AutoMapSight_ShapeOfItem(AutoMapMeasure_ViewOf(key, palette), NULL);
      std::snprintf(entry, sizeof(entry), " alcove %u@%d,%d:%d of %d patch %d", (unsigned)key.record,
                    key.x + offsetX, key.y + offsetY, shape.visible, shape.drawn, shape.largestPatch);
      itemTrace += entry;
      KeepMeasurement(key, shape.visible, shape.drawn, shape.largestPatch);
      if (shape.largestPatch >= AUTOMAP_SEEN_ITEM_PATCH)
      {
        recorder.SeeAlcoveItem(key.x, key.y, key.record, key.descIndex);
      }
    }
    else if (key.kind == AutoMapDrawMonster)
    {
      // A monster is told by the pixels that stand out like an item's, but needs no patch: two
      // eyes in the dark give it away
      AutoMapItemShape shape = AutoMapSight_ShapeOfItem(AutoMapMeasure_ViewOf(key, palette), NULL);
      std::snprintf(entry, sizeof(entry), " %u pos %d:%d of %d patch %d", (unsigned)key.record,
                    monsterPositionsOfFrame[key.record], shape.visible, shape.drawn,
                    shape.largestPatch);
      monsterTrace += entry;
      KeepMeasurement(key, shape.visible, shape.drawn, shape.largestPatch);
      if (shape.visible >= AUTOMAP_SEEN_MONSTER_PIXELS) recorder.SeeMonster(key.record);
    }
    else if (key.kind == AutoMapDrawFloorDecoration)
    {
      // Judged like a wall decoration, against the floor it lies on: a puddle around the corner
      // whose picture the nearer wall hides is not noticed although a strip of its floor shows
      AutoMapDrawView view = AutoMapMeasure_ViewOf(key, palette);
      int contrasting = AutoMapSight_ContrastingPixels(view, NULL);
      int kept = (int)std::count(view.isDrawn.begin(), view.isDrawn.end(), true);
      int largestPatch = AutoMapSight_ShapeOfItem(view, NULL).largestPatch;
      int darkening = AutoMapSight_DecorationDarkening(view);
      int tintPatch = AutoMapSight_DecorationTintPatch(view);
      std::snprintf(entry, sizeof(entry), " floor@%d,%d:%d of %d patch %d", key.x + offsetX,
                    key.y + offsetY, contrasting, kept, largestPatch);
      itemTrace += entry;
      KeepMeasurement(key, contrasting, kept, largestPatch, darkening, tintPatch);
      if (AutoMapSight_DecorationIsSeen(view)) recorder.SeeFloorPicture(key.x, key.y);
    }
    else if (key.kind == AutoMapDrawDoor)
    {
      // Judged by its pixels that stand out against what it covers, touching or not: the bars
      // of a portcullis two ahead show in the dark beyond the light's reach as a grid of small
      // patches, a wooden door there is black
      AutoMapItemShape shape = AutoMapSight_ShapeOfItem(AutoMapMeasure_ViewOf(key, palette), NULL);
      std::snprintf(entry, sizeof(entry), " door@%d,%d:%d of %d patch %d", key.x + offsetX,
                    key.y + offsetY, shape.visible, shape.drawn, shape.largestPatch);
      itemTrace += entry;
      KeepMeasurement(key, shape.visible, shape.drawn, shape.largestPatch);
      if (AutoMapSight_DoorIsSeen(shape)) recorder.SeeDoor(key.x, key.y);
    }
  }
  // With map_timing set: each new view's decorations as drawLocation:contrasting pixels "of" the
  // pixels on screen; on lines of their own, so they can be watched alone, its items as
  // record@x,y:pixels standing out "of" the pixels on screen and the largest patch, and its
  // monsters as record, position byte:pixels standing out "of" the pixels on screen and the
  // largest patch. Each line with a picture.
  // Numbered pictures in the working directory, to compare the count with the screen; the time
  // of the start keeps a later game from writing over them
  static int pictureNumber = 0;
  static long started = (long)std::time(NULL);
  char path[64];
  static std::string lastTrace;
  static std::string lastItemTrace;
  static std::string lastMonsterTrace;
  char where[64];
  std::snprintf(where, sizeof(where), "L%d global (%d,%d) facing %d palette %d:", d.partyLevel,
                d.partyX + offsetX, d.partyY + offsetY, d.partyFacing, shown);
  trace = where + trace;
  if (!itemTrace.empty()) itemTrace = where + itemTrace;
  if (!monsterTrace.empty()) monsterTrace = where + monsterTrace;
  if (AutoMapWindow_TimingReportsOn() && trace != lastTrace)
  {
    if (hasDecorations)
    {
      std::snprintf(path, sizeof(path), "automap-decorations-%ld-%03d.png", started, ++pictureNumber);
      bool written = AutoMapMeasure_WritePicture(path, palette, AutoMapDrawDecoration);
      printf("Automap decorations: %s -> %s\n", trace.c_str(), written ? path : "picture not written");
    }
    else
    {
      printf("Automap decorations: %s\n", trace.c_str());
    }
    lastTrace = trace;
  }
  if (AutoMapWindow_TimingReportsOn() && itemTrace != lastItemTrace)
  {
    if (!itemTrace.empty())
    {
      std::snprintf(path, sizeof(path), "automap-items-%ld-%03d.png", started, ++pictureNumber);
      bool written = AutoMapMeasure_WritePicture(path, palette, AutoMapDrawItem);
      printf("Automap items: %s -> %s\n", itemTrace.c_str(), written ? path : "picture not written");
    }
    lastItemTrace = itemTrace;
  }
  if (AutoMapWindow_TimingReportsOn() && monsterTrace != lastMonsterTrace)
  {
    if (!monsterTrace.empty())
    {
      std::snprintf(path, sizeof(path), "automap-monsters-%ld-%03d.png", started, ++pictureNumber);
      bool written = AutoMapMeasure_WritePicture(path, palette, AutoMapDrawMonster);
      printf("Automap monsters: %s -> %s\n", monsterTrace.c_str(), written ? path : "picture not written");
    }
    lastMonsterTrace = monsterTrace;
  }
}

void AutoMap_OnViewportFinished(void)
{
  LearnFromViewportDraws();
  if (!AutoMapWindow_TimingReportsOn())
  {
    measuringCounts = 0;
    return;
  }
  int itemDraws = 0;
  int monsterDraws = 0;
  const std::vector<AutoMapDrawOnScreen> &draws = AutoMapMeasure_DrawsOfFrame();
  for (size_t i = 0; i < draws.size(); i++)
  {
    if (draws[i].key.kind == AutoMapDrawItem) itemDraws++;
    if (draws[i].key.kind == AutoMapDrawMonster) monsterDraws++;
  }
  // One line per change of the rounded value, so a steady view does not flood the console
  static long lastMicroseconds = -1;
  long microseconds = (long)((double)measuringCounts * 1000000.0 / (double)SDL_GetPerformanceFrequency() + 0.5);
  measuringCounts = 0;
  if (microseconds == lastMicroseconds) return;
  lastMicroseconds = microseconds;
  printf("Automap: measuring took %ld us this frame (%d item draws, %d monster draws)\n", microseconds,
         itemDraws, monsterDraws);
}

void AutoMap_OnWallActuatorFired(int actuator, bool itemMatched)
{
  if (!PartyLevelIsLoaded()) return;
  // Only the matching item tells what the actuator wanted: a click or a wrong item can fire
  // it too (a normally closed lock, a constant pressure one) and tells nothing
  if (itemMatched) LearnRecord(AutoMapFactLockRequirement, actuator);
  LearnActuatorAndWatchItsLinks(actuator);
  // The party touched what it used, so the face counts as seen however dark the wall was
  // drawn (TouchWallF1 touches the wall ahead, on the face towards the party)
  recorder.UseFaceAhead(d.partyX + d.DeltaX[d.partyFacing], d.partyY + d.DeltaY[d.partyFacing]);
}

// A pad tells it fired when it clicks or is a plate the 3D view draws; an alcove is in plain
// sight. An invisible silent pad tells nothing until its effect is seen (LearnWatchedLinks).
static bool FiringIsNoticed(int actuator, int x, int y)
{
  if (((GetCellFlags(x, y) >> 5) & 7) == roomSTONE) return true;
  RN record;
  record.ConstructFromInteger(actuator);
  DB3 *pad = GetRecordAddressDB3(record);
  return pad->audible() != 0 || pad->wallGraphicOrdinal() != 0;
}

void AutoMap_OnFloorActuatorFired(int actuator, int x, int y)
{
  if (!PartyLevelIsLoaded() || !recorder.CanTell(x, y)) return;
  if (FiringIsNoticed(actuator, x, y))
  {
    LearnActuatorAndWatchItsLinks(actuator);
    return;
  }
  AutoMapRecordId id = AutoMapLevelReader_RecordId(actuator);
  if (id == AUTOMAP_NO_RECORD) return;
  recorder.LinkWatch().Start(id, AutoMapLevelReader_ReadLevel(d.partyLevel), d.Time);
}

void AutoMap_OnWallClickedSilently(int x, int y)
{
  if (!PartyLevelIsLoaded()) return;
  LearnCell(AutoMapFactFalseWall, d.partyLevel, x, y);
}

// The party falls with the levels loaded one after the other, so the level is its own
void AutoMap_OnFellThroughPit(int level, int x, int y, bool isParty)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  if (!isParty && (level != d.partyLevel || !PartyLevelIsLoaded() || !recorder.CanTell(x, y))) return;
  LearnCell(AutoMapFactPit, level, x, y);
  // The pit's level is still the loaded one here (MoveObject loads the next level only after
  // this hook), and the party's next tick looks at the level below: the remembered look of a
  // concealed pit would keep the closed state of its first sighting until seen again
  if (isParty) recorder.SeeFallenThroughPit(level, x, y);
}

// Like the pit: a chain of pits and teleporters loads each level on the way before the
// party's level follows, so the level is its own
void AutoMap_OnPartyTeleported(int level, int x, int y)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  LearnCell(AutoMapFactTeleporter, level, x, y);
}

void AutoMap_OnGeneratorFired(int actuator, int x, int y)
{
  if (!PartyLevelIsLoaded() || !recorder.CanTell(x, y)) return;
  LearnRecord(AutoMapFactActuator, actuator);
}

// The missile leaves the wall through the floor cell in front of the side
void AutoMap_OnLauncherFired(int actuator, int x, int y, int side)
{
  if (!PartyLevelIsLoaded()) return;
  if (!recorder.CanTell(x + d.DeltaX[side & 3], y + d.DeltaY[side & 3])) return;
  LearnRecord(AutoMapFactActuator, actuator);
}

void AutoMap_OnPortraitViewed(int x, int y, int side)
{
  if (!PartyLevelIsLoaded()) return;
  recorder.LookIntoPortrait(x, y, side);
}

void AutoMap_OnChestOpened(int chest)
{
  if (!PartyLevelIsLoaded()) return;
  LearnRecord(AutoMapFactChestOpened, chest);
}

void AutoMap_OnScrollRead(int scroll)
{
  if (!PartyLevelIsLoaded()) return;
  LearnRecord(AutoMapFactScrollRead, scroll);
}

static bool IsFalseWall(int x, int y)
{
  return ((GetCellFlags(x, y) >> 5) & 7) == roomFALSEWALL;
}

// A false wall the party stands in is passable; one seen opening or closing is no wall
static void LearnFalseWallsOfThisTick(void)
{
  if (IsFalseWall(d.partyX, d.partyY)) LearnCell(AutoMapFactFalseWall, d.partyLevel, d.partyX, d.partyY);
  std::vector<AutoMapCell> inSight = recorder.CellsInSightNow();
  for (size_t i = 0; i < inSight.size(); i++)
  {
    const AutoMapCell &cell = inSight[i];
    if (!IsFalseWall(cell.x, cell.y)) continue;
    bool isOpen = (GetCellFlags(cell.x, cell.y) & 4) != 0;
    if (recorder.FalseWallWatch().Observe(cell, isOpen)) Learn(AutoMapFact_OfCell(AutoMapFactFalseWall, cell));
  }
}

// With map_debug, one line per learned link, to find out where a link the party never saw
// comes from: source and target in global coordinates, and which sight rule saw the change
static void ReportLearnedLink(const AutoMapLevelSnapshot &snapshot, const AutoMapFact &fact,
                              int startedAt)
{
  int sourceX = 0;
  int sourceY = 0;
  bool sourceIsFound = false;
  for (size_t i = 0; i < snapshot.links.size() && !sourceIsFound; i++)
  {
    if (snapshot.links[i].source != fact.record) continue;
    sourceX = snapshot.links[i].srcX + snapshot.offsetX;
    sourceY = snapshot.links[i].srcY + snapshot.offsetY;
    sourceIsFound = true;
  }
  char source[32];
  if (sourceIsFound) std::snprintf(source, sizeof(source), "(%d, %d)", sourceX, sourceY);
  else std::snprintf(source, sizeof(source), "(?, ?)");
  int targetX = fact.cell.x + snapshot.boundaryMarginX + snapshot.offsetX;
  int targetY = fact.cell.y + snapshot.boundaryMarginY + snapshot.offsetY;
  const char *seenBy = "frame";
  if (fact.cell.x == d.partyX && fact.cell.y == d.partyY) seenBy = "party cell";
  else if (fact.cell.x == d.partyX + d.DeltaX[d.partyFacing]
           && fact.cell.y == d.partyY + d.DeltaY[d.partyFacing])
  {
    seenBy = "cell ahead";
  }
  printf("Automap learned link: source %u %s -> L%d (%d, %d), watch started %d, learned %d, "
         "seen by %s\n", (unsigned)fact.record, source, fact.cell.level, targetX, targetY,
         startedAt, (int)d.Time, seenBy);
}

static void LearnWatchedLinks(void)
{
  if (!recorder.LinkWatch().IsWatching()) return;
  AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(d.partyLevel);
  std::vector<int> startTicks;
  // What the 3D view shows this tick, not the monsters' line of sight, and whether a
  // concealed pit is known: its toggling is only seen once it is
  std::vector<AutoMapFact> facts = recorder.LinkWatch().Observe(
      snapshot, recorder.SeenThisTick(), &recorder.Knowledge(), d.Time, &startTicks);
  for (size_t i = 0; i < facts.size(); i++)
  {
    if (AutoMapWindow_DebugIsOn()) ReportLearnedLink(snapshot, facts[i], startTicks[i]);
    Learn(facts[i]);
    // A change seen to follow a silent pad gives the pad away: the arrow needs its source
    Learn(AutoMapFact_OfRecord(AutoMapFactActuator, facts[i].record));
  }
}

static void CarryOutDebugRequest(void);

static bool ViewOfThePartyIsHidden(void)
{
  return d.SelectedCharacterOrdinal != 0 || d.PartySleeping != 0;
}

static bool monstersFrozen = false;
static const int LONGEST_FREEZE = 255;

void AutoMap_OnEngineTick(void)
{
  // The game's own Freeze Life, kept from running out: the main loop counts it down after
  // this hook, so it never reaches zero while the debug switch is on
  if (monstersFrozen) d.freezeLifeTimer = LONGEST_FREEZE;
  if (!PartyLevelIsLoaded()) return;
  CarryOutDebugRequest();
  // Mid-way to another level the party already stands on the new level's coordinates
  // while partyLevel still names the old one: wait for the main loop to finish the move
  if (d.newPartyLevel != -1 || !PartyLevelIsLoaded()) return;
  HeroStats_OnPartyOnLevel();
  if (visibilityTestIsPending) RunVisibilityTest();
  recorder.MarkPartyCell();
  if (ViewOfThePartyIsHidden()) recorder.SeeOnlyThePartyCell();
  recorder.SeeCellLooks();
  LearnFalseWallsOfThisTick();
  LearnWatchedLinks();
  // The level is only read on a tick: a window opened while the game is frozen must not show
  // what the level looked like when it was hidden.
  if (!AutoMapWindow_IsShown())
  {
    ShowNoLevel();
    return;
  }
  if (!AutoMapWindow_WantsNewDrawList()) return;
  int viewedLevel = followParty ? d.partyLevel : lockedLevel;
  int levelCount = recorder.Explored().LevelCount();
  if (viewedLevel < 0 || (levelCount > 0 && viewedLevel >= levelCount)) viewedLevel = d.partyLevel;
  // A new level always rebuilds at once; the live data follows at a few hertz
  bool levelChanged = viewedLevel != lastBuiltLevel;
  if (!levelChanged && SDL_GetTicks() - lastRebuildTicks < SMALLEST_REBUILD_GAP) return;
  lastRebuildTicks = SDL_GetTicks();
  lastBuiltLevel = viewedLevel;
  if (followParty) lockedLevel = d.partyLevel;
  bool timingOn = AutoMapWindow_TimingReportsOn();
  Uint64 rebuildStart = SDL_GetPerformanceCounter();
  lastSnapshot = AutoMapLevelReader_ReadLevel(viewedLevel);
  Uint64 drawStart = SDL_GetPerformanceCounter();
  ShowLastSnapshot();
  if (timingOn)
  {
    double frequency = (double)SDL_GetPerformanceFrequency();
    double readMs = (drawStart - rebuildStart) * 1000000.0 / frequency / 1000.0;
    double drawMs =
        (SDL_GetPerformanceCounter() - drawStart) * 1000000.0 / frequency / 1000.0;
    printf("Automap timing: rebuild level %d took %.1f ms (read %.1f, draw %.1f)\n",
           viewedLevel, readMs + drawMs, readMs, drawMs);
  }
}

// ---------------------------------------------------------------------------------------------
// Debug placement

enum DebugRequestKind
{
  DebugRequestTeleport,
  DebugRequestSpawn,
  DebugRequestLevelUp,
  DebugRequestMeasure
};

struct DebugRequest
{
  bool isPending;
  DebugRequestKind kind;
  int level;
  int x;
  int y;
  int monsterType;
  int champion;
  int skill;
  int count;
  int positions;
  int facing;
};

static DebugRequest debugRequest = {false, DebugRequestTeleport, 0, 0, 0, 0, 0, 0, 1, 255, -1};

AutoMapPlacement AutoMap_PlacementAt(int level, int x, int y)
{
  return AutoMapScene_Placement(AutoMapLevelReader_ReadLevel(level), x, y);
}

void AutoMap_RequestTeleport(int level, int x, int y)
{
  DebugRequest request = {true, DebugRequestTeleport, level, x, y, 0, 0, 0, 1, 255, -1};
  debugRequest = request;
}

void AutoMap_RequestSpawn(int level, int x, int y, int monsterType, int count, int positions, int facing)
{
  DebugRequest request = {true, DebugRequestSpawn, level, x, y, monsterType, 0, 0, count, positions, facing};
  debugRequest = request;
}

static bool ChampionCanGainLevel(int champion, int skill)
{
  if (champion < 0 || champion >= d.NumCharacter || skill < SKILL_Fighter || skill > SKILL_Wizard)
  {
    return false;
  }
  if (d.PartySleeping != 0 || d.hero[champion].HP() == 0) return false;
  return AutoMapScene_ExperienceToNextLevel(d.hero[champion].skills92[skill].experience) > 0;
}

std::vector<AutoMapChampionLevels> AutoMap_ChampionLevels(void)
{
  std::vector<AutoMapChampionLevels> champions;
  for (int champion = 0; champion < d.NumCharacter; champion++)
  {
    const CHARDESC &hero = d.hero[champion];
    AutoMapChampionLevels levels;
    levels.name.assign(hero.name, strnlen(hero.name, sizeof(hero.name)));
    for (int skill = SKILL_Fighter; skill <= SKILL_Wizard; skill++)
    {
      levels.level[skill] = AutoMapScene_SkillLevel(hero.skills92[skill].experience);
      levels.canGainLevel[skill] = ChampionCanGainLevel(champion, skill);
    }
    champions.push_back(levels);
  }
  return champions;
}

void AutoMap_RequestLevelUp(int champion, int skill)
{
  DebugRequest request = {true, DebugRequestLevelUp, 0, 0, 0, 0, champion, skill, 1, 255, -1};
  debugRequest = request;
}

void AutoMap_RequestDecorationMeasure(void)
{
  DebugRequest request = {true, DebugRequestMeasure, 0, 0, 0, 0, 0, 0, 1, 255, -1};
  debugRequest = request;
}

static void MeasureDraws(void)
{
  char wallsPath[64];
  char thingsPath[64];
  std::snprintf(wallsPath, sizeof(wallsPath), "automap-walls-L%d.csv", d.partyLevel);
  std::snprintf(thingsPath, sizeof(thingsPath), "automap-things-L%d.csv", d.partyLevel);
  int thingsWritten = 0;
  int written = AutoMapMeasure_SweepPartyLevel(wallsPath, thingsPath, &thingsWritten);
  if (written < 0)
  {
    printf("Automap: could not write %s and %s\n", wallsPath, thingsPath);
    return;
  }
  printf("Automap: wrote %d wall decoration draws to %s and %d item draws to %s\n", written,
         wallsPath, thingsWritten, thingsPath);
}

// The TeleportParty DSA operation: the party wiggles on its own cell over a pretend
// teleporter. A move to another level finishes on the next tick of the main loop.
static void TeleportParty(int level, int x, int y)
{
  DB1 pretendTeleporter;
  pretendTeleporter.what(2); // Party and objects
  pretendTeleporter.destLevel(level);
  pretendTeleporter.destX(x);
  pretendTeleporter.destY(y);
  pretendTeleporter.rotation(0);
  pretendTeleporter.facingMode(0); // Turned by the rotation: kept facing
  pretendTeleporter.audible(0);
  MoveObject(RNnul, d.partyX, d.partyY, d.partyX, d.partyY, &pretendTeleporter, NULL);
  if (d.LoadedLevel != d.partyLevel) LoadLevel(d.partyLevel);
}

// A group of count with the level's own health multiplier, like a generator, facing as asked.
// CreateMonster centres one monster; on the party's level the group is active by then, so a
// spot of its own is set where the engine keeps it (SetMonsterPositionByte). CreateMonster
// was seen to return RNnul for a group it placed, so the group is looked up on its cell,
// which was free before.
static void SpawnMonster(int level, int x, int y, int monsterType, int count, int positions, int facing)
{
  LoadLevel(level);
  CreateMonster(monsterType, d.pCurLevelDesc->experienceMultiplier(), count - 1,
                facing < 0 ? STRandom0_3() : facing, x, y, false, false);
  RN group = FindFirstMonster(x, y);
  if (group == RNeof) group = RNnul;
  if (group != RNnul && count == 1 && positions != 255)
  {
    SetMonsterPositionByte(GetRecordAddressDB4(group), sw(positions), sw(level), false);
  }
  if (group != RNnul && AutoMapWindow_TimingReportsOn())
  {
    DB4 *spawned = GetRecordAddressDB4(group);
    printf("Automap spawn: L%d (%d,%d) %d of type %d asked at %d facing %d: positions %d facings %d"
           " group index %d\n", level, x, y, count, monsterType, positions, facing,
           MonsterPositionByte(spawned, sw(level), false) & 255, MonsterFacingByte(spawned, sw(level), false) & 255,
           spawned->groupIndex());
  }
  LoadLevel(d.partyLevel);
}

// AddToSkill adds the experience as a 16-bit number: bigger steps go in parts, and only
// the last part reaches the next level and calls LevelUp
static void GainLevel(int champion, int skill)
{
  int missing = AutoMapScene_ExperienceToNextLevel(d.hero[champion].skills92[skill].experience);
  while (missing > 0)
  {
    int part = std::min(missing, 0xffff);
    AddToSkill(champion, skill, part);
    missing -= part;
  }
}

static void CarryOutDebugRequest(void)
{
  if (!debugRequest.isPending) return;
  DebugRequest request = debugRequest;
  debugRequest.isPending = false;
  if (request.kind == DebugRequestMeasure)
  {
    MeasureDraws();
    return;
  }
  if (request.kind == DebugRequestLevelUp)
  {
    if (ChampionCanGainLevel(request.champion, request.skill)) GainLevel(request.champion, request.skill);
    return;
  }
  AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(request.level);
  if (AutoMapScene_Placement(snapshot, request.x, request.y) != AutoMapPlacementFree) return;
  int x = request.x - snapshot.boundaryMarginX;
  int y = request.y - snapshot.boundaryMarginY;
  if (request.kind == DebugRequestSpawn)
  {
    SpawnMonster(request.level, x, y, request.monsterType, request.count, request.positions,
                 request.facing);
  }
  else TeleportParty(request.level, x, y);
}

// ---------------------------------------------------------------------------------------------
// The visibility tests (csb --visibility-test): every case is one frame of the 3D view, drawn by
// the engine from where the case puts the party, and the map's verdicts are compared with the
// case file's. No game time passes between the cases.


static AutoMapVisibilitySection visibilitySection;
// Loading a save takes about a second and a case a few milliseconds
static const unsigned VISIBILITY_TEST_SECONDS = 60;

// The program again, for the cases of one section of the file. Its exit code; -1 when it could
// not be started or did not exit by itself.
static int RunSectionInChild(const char *program, const char *casePath, int section)
{
  char number[16];
  std::snprintf(number, sizeof(number), "%d", section);
  std::fflush(stdout);
#ifdef MINGW
  // _spawnvp joins the arguments with spaces, so those holding one are quoted
  std::string quotedProgram = std::string("\"") + program + "\"";
  std::string quotedPath = std::string("\"") + casePath + "\"";
  const char *args[] = {quotedProgram.c_str(), "--visibility-section", number, "--visibility-test",
                        quotedPath.c_str(), NULL};
  intptr_t status = _spawnvp(_P_WAIT, program, args);
  return status < 0 ? -1 : (int)status;
#else
  pid_t child = fork();
  if (child < 0) return -1;
  if (child == 0)
  {
    const char *args[] = {program, "--visibility-section", number, "--visibility-test", casePath, NULL};
    execvp(program, (char *const *)args);
    _exit(127);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) return -1;
  return WEXITSTATUS(status);
#endif
}

// Each save is loaded by a program of its own, the way a single save is: nothing of one save's
// dungeon stays for the next
static void RunSectionsInChildren(const char *program, const char *casePath,
                                  const AutoMapVisibilityCases &cases)
{
  std::string failedSaves;
  int failedCount = 0;
  for (size_t section = 0; section < cases.sections.size(); section++)
  {
    const AutoMapVisibilitySection &part = cases.sections[section];
    printf("visibility: line %d: save %s\n", part.lineNumber, part.save.c_str());
    int status = RunSectionInChild(program, casePath, (int)section);
    if (status == 0) continue;
    if (status != 1) printf("visibility: save %s did not finish its cases\n", part.save.c_str());
    failedCount++;
    failedSaves += " " + part.save;
  }
  if (failedCount == 0) printf("visibility: the checks of all %d saves passed\n", (int)cases.sections.size());
  else printf("visibility: %d of %d saves failed:%s\n", failedCount, (int)cases.sections.size(), failedSaves.c_str());
  std::fflush(stdout);
  std::_Exit(failedCount == 0 ? 0 : 1);
}

const char *AutoMap_PrepareVisibilityTest(const char *program, const char *casePath, int section)
{
  std::ifstream file(casePath, std::ios::binary);
  if (!file)
  {
    printf("visibility: cannot read %s\n", casePath);
    return NULL;
  }
  std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string error;
  AutoMapVisibilityCases cases;
  if (!AutoMapVisibilityCases_Parse(text, &cases, &error))
  {
    printf("visibility: %s: %s\n", casePath, error.c_str());
    return NULL;
  }
  if (section < 0 && cases.sections.size() > 1) RunSectionsInChildren(program, casePath, cases);
  if (section < 0) section = 0;
  if (section >= (int)cases.sections.size())
  {
    printf("visibility: %s has no section %d\n", casePath, section);
    return NULL;
  }
  visibilitySection = cases.sections[section];
  // Without its file the engine would wait in a dialog no window shows
  FILE *save = std::fopen(visibilitySection.save.c_str(), "rb");
  if (save == NULL)
  {
    printf("visibility: line %d: no saved game %s in the working directory\n",
           visibilitySection.lineNumber, visibilitySection.save.c_str());
    return NULL;
  }
  std::fclose(save);
#ifndef MINGW
  // A run that hangs ends rather than holding up the saves after it
  alarm(VISIBILITY_TEST_SECONDS);
#endif
  visibilityTestIsPending = true;
  return visibilitySection.save.c_str();
}

bool AutoMap_VisibilityTestIsPending(void)
{
  return visibilityTestIsPending;
}

// Level coordinates of the view cell cellNumber of the frame drawn from the party's place
static void PositionOfViewCell(int cellNumber, int *x, int *y)
{
  AutoMapSightOffset offset = AutoMapSight_OffsetOfCell(cellNumber);
  int right = (d.partyFacing + 1) & 3;
  *x = d.partyX + offset.stepsForward * d.DeltaX[d.partyFacing] + offset.stepsRight * d.DeltaX[right];
  *y = d.partyY + offset.stepsForward * d.DeltaY[d.partyFacing] + offset.stepsRight * d.DeltaY[right];
}

static AutoMapRecordId MonsterGroupAt(int x, int y)
{
  if (x < 0 || y < 0 || x >= d.width || y >= d.height) return AUTOMAP_NO_RECORD;
  RN group = FindFirstMonster(x, y);
  if (group == RNeof || group == RNnul) return AUTOMAP_NO_RECORD;
  return AutoMapLevelReader_RecordId(group.ConvertToInteger());
}

static bool IsSameName(const std::string &left, const std::string &right)
{
  if (left.size() != right.size()) return false;
  for (size_t i = 0; i < left.size(); i++)
  {
    if (std::tolower((unsigned char)left[i]) != std::tolower((unsigned char)right[i])) return false;
  }
  return true;
}

// The records of the items on the check's tile it names: all of them without a name or corner
// The items in the alcoves of the wall (x, y) of the party's level, in level coordinates; items in
// a chest there are left out, the chest stands for them
static std::vector<AutoMapAlcoveEntry> AlcoveItemsAt(int x, int y)
{
  std::vector<AutoMapAlcoveEntry> items;
  AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(d.partyLevel);
  int sx = x + snapshot.boundaryMarginX;
  int sy = y + snapshot.boundaryMarginY;
  if (sx < 0 || sy < 0 || sx >= snapshot.width || sy >= snapshot.height) return items;
  const std::vector<AutoMapAlcoveEntry> &held = snapshot.cells[(size_t)(sy * snapshot.width + sx)].alcoveContents;
  for (size_t i = 0; i < held.size(); i++)
  {
    if (held[i].inChest == AUTOMAP_NO_RECORD) items.push_back(held[i]);
  }
  return items;
}

static std::string AlcoveItemName(const AutoMapAlcoveEntry &entry)
{
  const char *name = AutoMapLevelReader_ItemName(entry.item.nameIndex);
  return name == NULL ? std::string() : std::string(name);
}

static std::vector<AutoMapRecordId> ItemRecordsOf(const AutoMapVisibilityCheck &check, int offsetX, int offsetY)
{
  std::vector<AutoMapRecordId> records;
  std::vector<AutoMapFloorItemPlace> places =
      AutoMapLevelReader_FloorItemPlacesAt(check.x - offsetX, check.y - offsetY);
  for (size_t i = 0; i < places.size(); i++)
  {
    if (check.corner >= 0 && places[i].corner != check.corner) continue;
    if (!check.itemName.empty() && !IsSameName(places[i].name, check.itemName)) continue;
    records.push_back(places[i].record);
  }
  return records;
}

// The thing a draw of the last frame shows, as a check in global coordinates: an item by its name,
// and its corner when another item on the tile has the same name; an item in an alcove as the
// alcove, by the face the party looks at. False for what the case file does not name: a monster
// not on a cell of the frame.
static bool ThingOfDraw(const AutoMapDrawKey &key, int offsetX, int offsetY, AutoMapVisibilityCheck *thing)
{
  thing->side = -1;
  thing->corner = -1;
  thing->itemName.clear();
  int x = key.x;
  int y = key.y;
  if (key.kind == AutoMapDrawDecoration)
  {
    int cellNumber = 0;
    int summaryIndex = 0;
    if (!AutoMapSight_DecorationPlacement(key.drawLocation, &cellNumber, &summaryIndex)) return false;
    thing->thing = AutoMapVisibilityDecoration;
    thing->side = AutoMapSight_SideOfSummaryIndex(d.partyFacing, summaryIndex);
    PositionOfViewCell(cellNumber, &x, &y);
  }
  else if (key.kind == AutoMapDrawItem)
  {
    thing->thing = AutoMapVisibilityItem;
    std::vector<AutoMapFloorItemPlace> places = AutoMapLevelReader_FloorItemPlacesAt(x, y);
    for (size_t i = 0; i < places.size(); i++)
    {
      if (places[i].record != key.record) continue;
      thing->itemName = places[i].name;
      for (size_t k = 0; k < places.size(); k++)
      {
        if (k != i && IsSameName(places[k].name, places[i].name)) thing->corner = places[i].corner;
      }
    }
  }
  else if (key.kind == AutoMapDrawAlcoveItem)
  {
    // The face SeeAlcoveItem marks: the one turned to the party; the item by its name
    thing->thing = AutoMapVisibilityAlcove;
    thing->side = AutoMapSight_SideOfSummaryIndex(d.partyFacing, 1);
    std::vector<AutoMapAlcoveEntry> held = AlcoveItemsAt(x, y);
    for (size_t i = 0; i < held.size(); i++)
    {
      if (held[i].record == key.record) thing->itemName = AlcoveItemName(held[i]);
    }
  }
  else if (key.kind == AutoMapDrawDoor) thing->thing = AutoMapVisibilityDoor;
  else if (key.kind == AutoMapDrawFloorDecoration) thing->thing = AutoMapVisibilityFloor;
  else if (key.kind == AutoMapDrawMonster)
  {
    thing->thing = AutoMapVisibilityMonster;
    bool isFound = false;
    for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT && !isFound; cell++)
    {
      PositionOfViewCell(cell, &x, &y);
      isFound = MonsterGroupAt(x, y) == key.record;
    }
    if (!isFound) return false;
  }
  else return false;
  thing->x = x + offsetX;
  thing->y = y + offsetY;
  return true;
}

static bool IsSameThing(const AutoMapVisibilityCheck &left, const AutoMapVisibilityCheck &right)
{
  return left.thing == right.thing && left.x == right.x && left.y == right.y && left.side == right.side
         && left.corner == right.corner && IsSameName(left.itemName, right.itemName);
}

// What the map made of the thing from the frame just drawn. False, with the problem, when the
// check names no single item: nothing on the tile matches it, or more than one item does.
static bool VerdictOf(const AutoMapVisibilityCheck &check, int offsetX, int offsetY,
                      AutoMapVisibilityVerdict *verdict, std::string *problem)
{
  AutoMapCell cell = {d.partyLevel, check.x - offsetX, check.y - offsetY};
  *verdict = AutoMapVisibilityNotSeen;
  if (check.thing == AutoMapVisibilityDecoration)
  {
    if (recorder.Explored().FaceIsSeen(cell, check.side)) *verdict = AutoMapVisibilitySeen;
    else if (recorder.Explored().FaceIsGlimpsed(cell, check.side)) *verdict = AutoMapVisibilityGlimpsed;
    return true;
  }
  if (check.thing == AutoMapVisibilityTile)
  {
    if (recorder.Explored().IsExplored(cell)) *verdict = AutoMapVisibilitySeen;
    return true;
  }
  if (check.thing == AutoMapVisibilityPortrait)
  {
    if (recorder.Explored().TextIsRead(cell, check.side)) *verdict = AutoMapVisibilitySeen;
    return true;
  }
  if (check.thing == AutoMapVisibilityFloor)
  {
    if (recorder.Explored().FloorIsSeen(cell)) *verdict = AutoMapVisibilitySeen;
    return true;
  }
  if (check.thing == AutoMapVisibilityAlcove)
  {
    // Without a name the alcove is seen when anything in it is; looked into as a whole, all is
    if (recorder.Explored().AlcoveIsLookedInto(cell, check.side)) *verdict = AutoMapVisibilitySeen;
    std::vector<AutoMapAlcoveEntry> held = AlcoveItemsAt(cell.x, cell.y);
    std::vector<AutoMapRememberedItem> remembered = recorder.Items().ItemsOn(cell);
    bool isNamed = false;
    for (size_t i = 0; i < held.size(); i++)
    {
      if (held[i].side != check.side) continue;
      if (!check.itemName.empty() && !IsSameName(AlcoveItemName(held[i]), check.itemName)) continue;
      isNamed = true;
      for (size_t r = 0; r < remembered.size(); r++)
      {
        if (remembered[r].record == held[i].record) *verdict = AutoMapVisibilitySeen;
      }
    }
    if (!check.itemName.empty() && !isNamed)
    {
      *problem = "no such item in the alcove";
      return false;
    }
    return true;
  }
  if (check.thing == AutoMapVisibilityItem)
  {
    std::vector<AutoMapRecordId> records = ItemRecordsOf(check, offsetX, offsetY);
    bool namesOne = check.corner >= 0 || !check.itemName.empty();
    if (namesOne && records.size() != 1)
    {
      char text[64];
      if (records.empty()) std::snprintf(text, sizeof(text), "no such item on the tile");
      else std::snprintf(text, sizeof(text), "%d items on the tile match", (int)records.size());
      *problem = text;
      return false;
    }
    std::vector<AutoMapRememberedItem> items = recorder.Items().ItemsOn(cell);
    for (size_t i = 0; i < items.size(); i++)
    {
      if (std::find(records.begin(), records.end(), items[i].record) == records.end()) continue;
      *verdict = std::max(*verdict, items[i].isGlimpsed ? AutoMapVisibilityGlimpsed : AutoMapVisibilitySeen);
    }
    return true;
  }
  if (check.thing == AutoMapVisibilityMonster)
  {
    AutoMapRecordId group = MonsterGroupAt(cell.x, cell.y);
    const std::vector<AutoMapRecordId> &seen = recorder.MonstersSeenNow();
    if (group != AUTOMAP_NO_RECORD && std::find(seen.begin(), seen.end(), group) != seen.end())
    {
      *verdict = AutoMapVisibilitySeen;
    }
    return true;
  }
  AutoMapCellLook look;
  if (recorder.Cells().LookOf(cell, &look)) *verdict = AutoMapVisibilitySeen;
  return true;
}

// The numbers the scoring judged the thing by in the frame, as the map_timing trace prints them.
// Empty for a tile: the line of sight and the light's reach decide it, no pixels.
static std::string MeasurementsOf(const AutoMapVisibilityCheck &check, int offsetX, int offsetY)
{
  if (check.thing == AutoMapVisibilityTile) return "";
  std::vector<AutoMapRecordId> itemRecords;
  if (check.thing == AutoMapVisibilityItem) itemRecords = ItemRecordsOf(check, offsetX, offsetY);
  std::string text;
  for (size_t i = 0; i < measurementsOfFrame.size(); i++)
  {
    const DrawMeasurement &measurement = measurementsOfFrame[i];
    AutoMapVisibilityCheck thing;
    if (!ThingOfDraw(measurement.key, offsetX, offsetY, &thing)) continue;
    if (check.thing == AutoMapVisibilityItem)
    {
      // The items the check names, however the survey would name each
      bool isNamed = std::find(itemRecords.begin(), itemRecords.end(), measurement.key.record) != itemRecords.end();
      if (thing.thing != AutoMapVisibilityItem || thing.x != check.x || thing.y != check.y || !isNamed) continue;
    }
    else if (check.thing == AutoMapVisibilityAlcove && check.itemName.empty())
    {
      // An unnamed alcove check takes the numbers of every item in it
      if (thing.thing != check.thing || thing.x != check.x || thing.y != check.y || thing.side != check.side) continue;
    }
    else if (!IsSameThing(thing, check)) continue;
    char entry[96];
    std::snprintf(entry, sizeof(entry), "%s%d %s of %d, patch %d", text.empty() ? "" : "; ",
                  measurement.counted,
                  check.thing == AutoMapVisibilityDecoration || check.thing == AutoMapVisibilityFloor
                      ? "contrasting" : "standing out",
                  measurement.drawn, measurement.largestPatch);
    text += entry;
    if (measurement.darkening >= 0)
    {
      std::snprintf(entry, sizeof(entry), ", tint patch %d, blot %d", measurement.tintPatch,
                    measurement.darkening);
      text += entry;
    }
  }
  return text.empty() ? "not measured" : text;
}

// Puts the party where the case says and draws its frame. The party is set down, not moved:
// nothing under it fires, so every case sees the dungeon as saved.
static void DrawVisibilityCase(const AutoMapVisibilityCase &visibilityCase, int offsetX, int offsetY)
{
  recorder.ForgetDungeon(AutoMapLevelReader_DungeonLevelSizes());
  if (d.partyLevel != visibilityCase.level) LoadPartyLevel(visibilityCase.level);
  d.partyX = sw(visibilityCase.x - offsetX);
  d.partyY = sw(visibilityCase.y - offsetY);
  d.partyFacing = sw(visibilityCase.facing);
  AutoMap_SetLightOverride(visibilityCase.light);
  DrawViewport(d.partyFacing, d.partyX, d.partyY);
}

static void RunVisibilityTest(void)
{
  int passed = 0;
  int total = 0;
  int levelCount = (int)AutoMapLevelReader_DungeonLevelSizes().size();
  std::map<int, AutoMapLevelSnapshot> snapshots;
  {
    int offsetX = 0;
    int offsetY = 0;
    AutoMapLevelReader_LevelOffset(d.partyLevel, &offsetX, &offsetY);
    printf("visibility: %s has the party at L%d (%d,%d) facing %s\n", visibilitySection.save.c_str(),
           d.partyLevel, d.partyX + offsetX, d.partyY + offsetY,
           AutoMapVisibilityCases_SideName(d.partyFacing));
  }
  for (size_t c = 0; c < visibilitySection.cases.size(); c++)
  {
    const AutoMapVisibilityCase &visibilityCase = visibilitySection.cases[c];
    if (visibilityCase.level >= levelCount)
    {
      printf("visibility: line %d: the dungeon has no level %d\n", visibilityCase.lineNumber,
             visibilityCase.level);
      total += std::max((int)visibilityCase.checks.size(), 1);
      continue;
    }
    int offsetX = 0;
    int offsetY = 0;
    AutoMapLevelReader_LevelOffset(visibilityCase.level, &offsetX, &offsetY);
    if (snapshots.find(visibilityCase.level) == snapshots.end())
    {
      snapshots[visibilityCase.level] = AutoMapLevelReader_ReadLevel(visibilityCase.level);
    }
    const AutoMapLevelSnapshot &snapshot = snapshots[visibilityCase.level];
    // The party's cell as the map numbers it: the snapshot's place plus its offset
    int snapshotX = visibilityCase.x - snapshot.offsetX;
    int snapshotY = visibilityCase.y - snapshot.offsetY;
    bool isInSnapshot = snapshotX >= 0 && snapshotY >= 0 && snapshotX < snapshot.width
                        && snapshotY < snapshot.height;
    const AutoMapCellRecord *cell =
        isInSnapshot ? &snapshot.cells[(size_t)snapshotY * snapshot.width + snapshotX] : NULL;
    // Inside rock the 3D view draws what the party could never see
    if (cell == NULL || cell->isOutsideLevel || cell->kind == AutoMapCellWall
        || cell->kind == AutoMapCellFalseWall)
    {
      printf("visibility: line %d: the party cannot stand on L%d (%d,%d)\n", visibilityCase.lineNumber,
             visibilityCase.level, visibilityCase.x, visibilityCase.y);
      total += std::max((int)visibilityCase.checks.size(), 1);
      continue;
    }
    DrawVisibilityCase(visibilityCase, offsetX, offsetY);
    // Monsters seen are those of the frame; the looks need the tick's refresh
    recorder.SeeCellLooks();
    if (visibilityCase.checks.empty())
    {
      // A survey: every thing the frame drew, once, as a check line to paste
      printf("visibility: line %d: survey of at L%d (%d,%d) facing %s light %d\n",
             visibilityCase.lineNumber, visibilityCase.level, visibilityCase.x, visibilityCase.y,
             AutoMapVisibilityCases_SideName(visibilityCase.facing), visibilityCase.light);
      std::vector<AutoMapVisibilityCheck> things;
      const std::vector<AutoMapDrawOnScreen> &draws = AutoMapMeasure_DrawsOfFrame();
      for (size_t i = 0; i < draws.size(); i++)
      {
        AutoMapVisibilityCheck thing;
        if (!ThingOfDraw(draws[i].key, offsetX, offsetY, &thing)) continue;
        bool isKnown = false;
        for (size_t k = 0; k < things.size() && !isKnown; k++) isKnown = IsSameThing(things[k], thing);
        if (!isKnown) things.push_back(thing);
      }
      // Every tile of the frame but the party's own, which is always seen; the rim's rock too
      for (int cell = 0; cell < AUTOMAP_SIGHT_CELL_COUNT; cell++)
      {
        if (cell == AUTOMAP_SIGHT_PARTY_CELL) continue;
        int x = 0;
        int y = 0;
        PositionOfViewCell(cell, &x, &y);
        if (x < -1 || y < -1 || x > d.width || y > d.height) continue;
        AutoMapVisibilityCheck tile = {visibilityCase.lineNumber, AutoMapVisibilityTile, x + offsetX,
                                       y + offsetY, -1, -1, "", AutoMapVisibilityNotSeen};
        things.push_back(tile);
      }
      for (size_t k = 0; k < things.size(); k++)
      {
        AutoMapVisibilityVerdict verdict = AutoMapVisibilityNotSeen;
        std::string problem;
        bool isJudged = VerdictOf(things[k], offsetX, offsetY, &verdict, &problem);
        std::string line = AutoMapVisibilityCases_CheckLine(things[k], verdict);
        std::string measurements = MeasurementsOf(things[k], offsetX, offsetY);
        if (measurements.empty() && isJudged) printf("  %s\n", line.c_str());
        else printf("  %-40s # %s%s%s\n", line.c_str(), measurements.c_str(), isJudged ? "" : "; ",
                    problem.c_str());
      }
      continue;
    }
    for (size_t i = 0; i < visibilityCase.checks.size(); i++)
    {
      const AutoMapVisibilityCheck &check = visibilityCase.checks[i];
      total++;
      AutoMapVisibilityVerdict verdict = AutoMapVisibilityNotSeen;
      std::string problem;
      if (!VerdictOf(check, offsetX, offsetY, &verdict, &problem))
      {
        printf("visibility: line %d: %s: %s\n", check.lineNumber,
               AutoMapVisibilityCases_CheckLine(check, check.expected).c_str(), problem.c_str());
        continue;
      }
      if (verdict == check.expected)
      {
        passed++;
        continue;
      }
      std::string measurements = MeasurementsOf(check, offsetX, offsetY);
      if (!measurements.empty()) measurements = " (" + measurements + ")";
      printf("visibility: line %d: expected %s, got %s%s\n", check.lineNumber,
             AutoMapVisibilityCases_CheckLine(check, check.expected).c_str(),
             AutoMapVisibilityCases_VerdictName(verdict), measurements.c_str());
    }
  }
  printf("visibility: %s: %d of %d checks passed\n", visibilitySection.save.c_str(), passed, total);
  // Nothing of the run is saved: not the game, not the map file, not csb.cfg
  std::fflush(stdout);
  std::_Exit(passed == total ? 0 : 1);
}

int AutoMap_Vision(void)
{
  return visionSetting;
}

// The level is not read again here: the menu can be used while the engine is in the middle
// of something. The records of the last tick are filtered anew.
static void FilterLastSnapshotAnew(void)
{
  if (lastSnapshot.level != AUTOMAP_NO_LEVEL) ShowLastSnapshot();
}

// ---------------------------------------------------------------------------------------------
// The damage the game showed. It counts wherever the group is: the number was on screen, even
// when the group then walks out of sight.

void AutoMap_OnMonsterDamageShown(int group, int member, int damage)
{
  recorder.Damage().Credit(AutoMapLevelReader_RecordId(group), member, damage);
  FilterLastSnapshotAnew();
}

void AutoMap_OnGroupDamageShown(int group, int damage)
{
  RN record;
  record.ConstructFromInteger(group);
  if (record == RNeof || record == RNnul) return;
  int memberCount = GetRecordAddressDB4(record)->numMonM1() + 1;
  for (int member = 0; member < memberCount; member++)
  {
    recorder.Damage().Credit(AutoMapLevelReader_RecordId(group), member, damage);
  }
  FilterLastSnapshotAnew();
}

void AutoMap_OnMonsterMemberRemoved(int group, int member)
{
  recorder.Damage().RemoveMember(AutoMapLevelReader_RecordId(group), member);
  FilterLastSnapshotAnew();
}

// Records are reused: a generator's new group must not show the damage of the group that died
void AutoMap_OnMonsterRecordTaken(int group)
{
  recorder.Damage().ForgetGroup(AutoMapLevelReader_RecordId(group));
}

void AutoMap_SetVision(int vision)
{
  visionSetting = ValidVisionSetting(vision);
  FilterLastSnapshotAnew();
  AutoMapWindow_RefreshSearch();
}

int AutoMap_SpoilerLevel(void)
{
  return spoilerSetting;
}

void AutoMap_SetSpoilerLevel(int level)
{
  spoilerSetting = ValidSpoilerSetting(level);
  FilterLastSnapshotAnew();
  AutoMapWindow_RefreshSearch();
}

// ---------------------------------------------------------------------------------------------
// Debug forgetting and light

static const int LIGHT_OF_THE_PARTY = -1;
static const int DARKEST_PALETTE = 5;
static int lightOverride = LIGHT_OF_THE_PARTY;

static void ShowForgottenMap(void)
{
  FilterLastSnapshotAnew();
  AutoMapWindow_RefreshSearch();
}

// The remembered items and looks go with the cells: they are what the party saw there. None of
// the three forgets here touches the player's notes (see AutoMap_ForgetPlayerNotes).
void AutoMap_ForgetExploredLevel(int level)
{
  recorder.Explored().ForgetLevel(level);
  recorder.Items().ForgetLevel(level);
  recorder.Cells().ForgetLevel(level);
  ShowForgottenMap();
}

void AutoMap_ForgetExploredDungeon(void)
{
  for (int level = 0; level < recorder.Explored().LevelCount(); level++)
  {
    recorder.Explored().ForgetLevel(level);
    recorder.Items().ForgetLevel(level);
  }
  recorder.Cells().Forget();
  ShowForgottenMap();
}

// The watches go too: a watch started before would teach a link again right away
void AutoMap_ForgetLearned(void)
{
  recorder.Knowledge().Forget();
  recorder.Damage().Forget();
  recorder.LinkWatch().Clear();
  recorder.FalseWallWatch().Clear();
  ShowForgottenMap();
}

// ---------------------------------------------------------------------------------------------
// The player's notes. The three forgets above never touch them: they are the player's, not the
// map's, and only FORGET MARKS clears them. A note is stored as given, never checked against
// the dungeon.

// Only a cell of the level itself is kept: the .map file holds no other, and a load would refuse
// the whole file over one (the window offers no note on the rim rock either). This is the level's
// shape, nothing the dungeon would have to tell.
static bool CellTakesNotes(const AutoMapCell &cell)
{
  if (cell.level < 0 || cell.level >= recorder.Explored().LevelCount()) return false;
  AutoMapLevelSize size = recorder.Explored().SizeOfLevel(cell.level);
  return cell.x >= 0 && cell.x < size.width && cell.y >= 0 && cell.y < size.height;
}

void AutoMap_SetPlayerPit(int level, int x, int y, int mark)
{
  AutoMapCell cell = {level, x, y};
  if (!CellTakesNotes(cell)) return;
  if (mark < 0) recorder.Notes().ClearPit(cell);
  else recorder.Notes().SetPit(cell, mark != 0);
  FilterLastSnapshotAnew();
}

void AutoMap_TogglePlayerLink(int level, int srcX, int srcY, int side, int targetLevel, int dstX,
                              int dstY)
{
  AutoMapLinkSource source = {{level, srcX, srcY}, side};
  AutoMapCell target = {targetLevel, dstX, dstY};
  // A link stays on one level, and never links a cell to itself
  if (targetLevel != level || !CellTakesNotes(source.cell) || !CellTakesNotes(target)) return;
  if (side < -1 || side > AutoMapWest) return;
  if (srcX == dstX && srcY == dstY) return;
  recorder.Notes().ToggleLinkTarget(source, target);
  FilterLastSnapshotAnew();
}

void AutoMap_RemovePlayerLinksOf(int level, int x, int y)
{
  AutoMapCell cell = {level, x, y};
  if (!CellTakesNotes(cell)) return;
  recorder.Notes().RemoveLinksOf(cell);
  FilterLastSnapshotAnew();
}

void AutoMap_ForgetPlayerNotes(void)
{
  recorder.Notes().Forget();
  FilterLastSnapshotAnew();
}

int AutoMap_LightOverride(void)
{
  return lightOverride;
}

void AutoMap_SetLightOverride(int palette)
{
  if (palette < LIGHT_OF_THE_PARTY || palette > DARKEST_PALETTE) palette = LIGHT_OF_THE_PARTY;
  lightOverride = palette;
  // The vertical blank copies the viewport palette only when asked to, as after a light change
  d.Word11778 = 1;
}

int AutoMap_RealPalette(void)
{
  return d.CurrentPalette;
}

int AutoMap_ShownPalette(int realPalette)
{
  return lightOverride == LIGHT_OF_THE_PARTY ? realPalette : lightOverride;
}

bool AutoMap_MonstersAreFrozen(void)
{
  return monstersFrozen;
}

// Switching off ends the freeze at once, a Freeze Life cast before included
void AutoMap_SetMonstersFrozen(bool isFrozen)
{
  monstersFrozen = isFrozen;
  d.freezeLifeTimer = isFrozen ? LONGEST_FREEZE : 0;
}

bool AutoMap_ExploredIsHighlighted(void)
{
  return exploredIsHighlighted;
}

void AutoMap_SetExploredHighlight(bool isOn)
{
  exploredIsHighlighted = isOn;
  FilterLastSnapshotAnew();
}

bool AutoMap_FollowsParty(void)
{
  return followParty;
}

void AutoMap_FollowParty(void)
{
  followParty = true;
  AutoMapWindow_RecenterView();
  // The tick picks the party's level up again; nothing is read here because the menu
  // can be used while the engine is in the middle of something
}

int AutoMap_ShownLevel(void)
{
  if (followParty) return d.partyLevel;
  return lockedLevel;
}

// Dungeon-wide item search: one filtered draw list per level, searched with
// the engine-free rule, merged by match quality. Unexplored cells never match
// (no draw-list cell), monsters match only on the party's level where the fog
// lets them through, or on any explored cell with Explored, live.
std::vector<AutoMapSearchHit> AutoMap_SearchDungeon(const std::string &query)
{
  std::vector<AutoMapSearchHit> all;
  if (query.empty()) return all;
  std::vector<AutoMapLevelSize> levelSizes = AutoMapLevelReader_DungeonLevelSizes();
  int levelCount = (int)levelSizes.size();
  if (levelCount <= 0) levelCount = recorder.Explored().LevelCount();
  static const AutoMapVisibleCells nothingVisibleNow;
  static const std::vector<AutoMapRecordId> noMonstersSeenNow;
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(level);
    if (snapshot.width <= 0 || snapshot.height <= 0) continue;
    const AutoMapVisibleCells *visibleNow = &recorder.VisibleNow();
    if (level != d.partyLevel) visibleNow = &nothingVisibleNow;
    const std::vector<AutoMapRecordId> *monstersSeenNow = &recorder.MonstersSeenNow();
    if (level != d.partyLevel) monstersSeenNow = &noMonstersSeenNow;
    AutoMapFog fog = {visionSetting != VISION_WHOLE_LEVEL_SETTING, &recorder.Explored(), visibleNow,
                      exploredIsHighlighted,
                      SpoilersOfSetting(), &recorder.Knowledge(), &recorder.Items(), monstersSeenNow,
                      &recorder.Cells(), &recorder.Notes(), &recorder.Damage(),
                      visionSetting == VISION_LIVE_SETTING};
    AutoMapScene_RememberExploredLooks(snapshot, recorder.Explored(), &recorder.Cells());
    AutoMapDrawList drawList = AutoMapScene_BuildDrawList(snapshot, fog);
    std::vector<AutoMapSearchHit> hits =
        AutoMapScene_SearchItems(drawList, query, AutoMapLevelReader_ItemName,
                                 AutoMapLevelReader_ItemIsFood);
    all.insert(all.end(), hits.begin(), hits.end());
  }
  AutoMapScene_SortSearchHits(&all, query);
  return all;
}

int AutoMap_LevelCount(void)
{
  return recorder.Explored().LevelCount();
}

void AutoMap_ShowLevel(int level)
{
  int levelCount = recorder.Explored().LevelCount();
  if (levelCount > 0) level = std::max(0, std::min(level, levelCount - 1));
  else if (level < 0) level = 0;
  lockedLevel = level;
  followParty = false;
}

void AutoMap_ReadSettings(void)
{
  // A csb.cfg from before the three modes has only map_fog: on is the fog of war, off the
  // whole level
  int vision = csb_get_config_int("settings", "map_vision", -1);
  if (vision < 0)
  {
    vision = csb_get_config_int("settings", "map_fog", 1) != 0 ? VISION_FOG_SETTING
                                                               : VISION_WHOLE_LEVEL_SETTING;
  }
  visionSetting = ValidVisionSetting(vision);
  exploredIsHighlighted = csb_get_config_int("settings", "map_highlight", 0) != 0;
  spoilerSetting = ValidSpoilerSetting(
      csb_get_config_int("settings", "map_spoilers", SPOILERS_LEARNED_SETTING));
  followParty = csb_get_config_int("settings", "map_follow", 1) != 0;
  lockedLevel = csb_get_config_int("settings", "map_level", 0);
  AutoMapWindow_ReadSettings();
}

void AutoMap_WriteSettings(void)
{
  csb_set_config_int("settings", "map_vision", visionSetting);
  // Kept for an older build, which then opens with the nearest mode
  csb_set_config_int("settings", "map_fog", visionSetting != VISION_WHOLE_LEVEL_SETTING ? 1 : 0);
  csb_set_config_int("settings", "map_highlight", exploredIsHighlighted ? 1 : 0);
  csb_set_config_int("settings", "map_spoilers", spoilerSetting);
  csb_set_config_int("settings", "map_follow", followParty ? 1 : 0);
  csb_set_config_int("settings", "map_level", lockedLevel);
  AutoMapWindow_WriteSettings();
}

// ---------------------------------------------------------------------------------------------
// Saved games

// The engine looks for a file in several folders. The .map file must sit beside the file the
// engine actually found, so the path is taken from the engine's own table of open files.
static SavedGameFile FindSavedGame(const char *name)
{
  SavedGameFile save;
  save.signature.first = 0;
  save.signature.second = 0;
  if (name == NULL) return save;
  i16 file = OPEN(name, "rb");
  if (file < 0) return save;
  ui32 first = 0, second = 0;
  Signature(file, &first, &second);
  save.signature.first = first;
  save.signature.second = second;
  save.path = GETFILENAME(file);
  CLOSE(file);
  return save;
}

static std::string MapFileOf(const std::string &savePath)
{
  return savePath + MAP_FILE_SUFFIX;
}

// A save without a .map file is normal, so a missing source is not an error
static void MoveMapFile(const std::string &fromSavePath, const std::string &toSavePath)
{
  if (fromSavePath.empty() || toSavePath.empty()) return;
  std::remove(MapFileOf(toSavePath).c_str());
  std::rename(MapFileOf(fromSavePath).c_str(), MapFileOf(toSavePath).c_str());
}

void AutoMap_BeforeSaveIsRotated(void)
{
  pathOfSaveBeforeRotation = FindSavedGame(d.SaveGameFilename).path;
}

void AutoMap_AfterSaveIsRotated(void)
{
  MoveMapFile(pathOfSaveBeforeRotation, FindSavedGame(d.Pointer22980).path);
  pathOfSaveBeforeRotation.clear();
}

static bool WriteMapFile(const SavedGameFile &save)
{
  if (save.path.empty()) return false;
  HeroStats_BeforeGameSaved();
  std::ofstream out(MapFileOf(save.path).c_str(), std::ios::binary | std::ios::trunc);
  AutoMapFile(save.signature).Write(out, recorder.Explored(), recorder.Knowledge(), recorder.Items(),
                                        recorder.Cells(), recorder.Notes(), recorder.Damage(), HeroStats_Store(),
                                        HeroStats_Origins());
  out.close();
  return !out.fail();
}

void AutoMap_OnGameSaved(void)
{
  if (WriteMapFile(FindSavedGame(d.SaveGameFilename)))
  {
    HeroStats_OnGameSaved();
    return;
  }
  UI_MessageBox("The map of explored cells could not be saved.\n"
                "The saved game itself is complete.",
                "Map", MESSAGE_OK);
}

void AutoMap_BeforeBackupIsRestored(void)
{
  pathOfBackupBeforeRestore = FindSavedGame(d.Pointer22980).path;
}

static void RememberItemsOfExploredCells(void)
{
  std::vector<AutoMapLevelSize> levelSizes = AutoMapLevelReader_DungeonLevelSizes();
  int levelCount = (int)levelSizes.size();
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapScene_RememberExploredItems(AutoMapLevelReader_ReadLevel(level), recorder.Explored(),
                                       &recorder.Items());
  }
}

// An older map file kept no looks: every explored cell takes the one it has now, before any
// change out of sight could slip into a first impression
static void RememberLooksOfExploredCells(void)
{
  std::vector<AutoMapLevelSize> levelSizes = AutoMapLevelReader_DungeonLevelSizes();
  int levelCount = (int)levelSizes.size();
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapScene_RememberExploredLooks(AutoMapLevelReader_ReadLevel(level), recorder.Explored(),
                                       &recorder.Cells());
  }
}

// False when no map file was read, so that the hero statistics know it
static bool ReadMapFile(const SavedGameFile &save)
{
  if (save.path.empty()) return false;
  std::string mapPath = MapFileOf(save.path);
  std::ifstream in(mapPath.c_str(), std::ios::binary);
  if (!in.is_open()) return false;
  bool hadItemMemory = false;
  bool hadCellMemory = false;
  HeroStats statsFromFile;
  HeroStatsOrigins originsFromFile;
  AutoMapFileResult result = AutoMapFile(save.signature).Read(
    in, &recorder.Explored(), &recorder.Knowledge(), &recorder.Items(), &hadItemMemory,
    &recorder.Cells(), &hadCellMemory, &recorder.Notes(), &recorder.Damage(), &statsFromFile,
    &originsFromFile);
  if (result == AutoMapFileRead)
  {
    HeroStats_OnMapFileRead(statsFromFile, originsFromFile);
    // An older map file showed every live item on an explored cell: keep showing those
    if (!hadItemMemory) RememberItemsOfExploredCells();
    if (!hadCellMemory) RememberLooksOfExploredCells();
    return true;
  }
  printf("Automap: ignoring %s: %s\n", mapPath.c_str(), AutoMapFile_Describe(result));
  return false;
}

// With map_timing set, list every lock actuator sitting on a door cell once per
// dungeon load: level with local and global coordinates, actuator type and side.
// Finding a test door by hovering the whole dungeon would take ages; this answers
// it in one terminal readout. Same machinery as the per-tick rebuild, read-only.
static void ReportDoorLocksOfDungeon(void)
{
  if (!AutoMapWindow_TimingReportsOn()) return;
  std::vector<AutoMapLevelSize> levelSizes = AutoMapLevelReader_DungeonLevelSizes();
  int levelCount = (int)levelSizes.size();
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadLevel(level);
    if (snapshot.width <= 0) continue;
    for (size_t i = 0; i < snapshot.cells.size(); i++)
    {
      const AutoMapCellRecord &cell = snapshot.cells[i];
      if (cell.kind != AutoMapCellDoor || cell.wallLocks.empty()) continue;
      int x = (int)(i % (size_t)snapshot.width);
      int y = (int)(i / (size_t)snapshot.width);
      for (size_t k = 0; k < cell.wallLocks.size(); k++)
      {
        const AutoMapWallLock &lock = cell.wallLocks[k];
        printf("Automap: door lock type %d side %d%s at level %d local (%d,%d) global (%d,%d)\n",
               lock.actuatorType, lock.side, lock.required ? " NEEDS" : " plain", level, x, y,
               x + snapshot.offsetX, y + snapshot.offsetY);
      }
    }
  }
}

void AutoMap_OnDungeonLoaded(void)
{
  // A new dungeon can reuse a level number with other wall pictures
  cachedLevel = -1;
  AutoMapLevelReader_ForgetDungeon();
  recorder.ForgetDungeon(AutoMapLevelReader_DungeonLevelSizes());
  ReportDoorLocksOfDungeon();
  ShowNoLevel();
  HeroStats_OnDungeonLoaded();
  std::string pathOfRestoredBackup = pathOfBackupBeforeRestore;
  pathOfBackupBeforeRestore.clear();
  if (d.gameState != GAMESTATE_ResumeSavedGame) return;
  SavedGameFile save = FindSavedGame(d.SaveGameFilename);
  MoveMapFile(pathOfRestoredBackup, save.path);
  if (!ReadMapFile(save)) HeroStats_OnNoMapFile();
}
