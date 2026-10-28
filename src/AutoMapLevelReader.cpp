// AutoMapLevelReader.cpp : reads dungeon levels the way the 3D view perceives them.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "AutoMapLevelReader.h"
// TimingReportsOn only; the reader never touches the window
#include "AutoMapWindow.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"
#include "AutoMapHooks.h"

static const i32 CUSTOM_WALL_GRAPHIC = 0x10000;
static const i32 CUSTOM_FLOOR_GRAPHIC = 0x4000;
static const i32 FOOTPRINTS = 0x8000;
// Each wall feature has a side view and a front view, each floor feature six views by distance
static const int FRONT_VIEW_OF_WALL_FEATURE = 1;
static const int NEAREST_VIEW_OF_FLOOR_FEATURE = 5;
static const int LEVEL_FEATURE_TABLE_SIZE = 16;
static const int PIT_PICTURE = 54;
static const int CONCEALED_PIT_PICTURE = 60;
static const int FIRST_ITEM_PICTURE = 360;
static const int FIRST_MONSTER_PICTURE = 446;
// The 3D view draws missiles with a negative graphic class through this picture family
static const int SPELL_MISSILE_PICTURE_BASE = 316;
static const int SPELL_MISSILE_PICTURE_COUNT = 14;
// ... and lingering clouds through this one
static const int CLOUD_PICTURE_BASE = 351;
// The one type-0 decoration the map keeps as a plain grey bar; every other type-0
// decoration (empty torch holders, alcoves, fountains, the two Lord portraits) shows
// its original picture
static const int MANACLES_WALL_DECORATION = 36;
static const int GRAPHIC_CLASS_COUNT = 85;
static const int WALL_SIDES_IN_ONE_SUMMARY = 3;

// SummarizeRoomInfo reports a wall decoration number the level does not have with a dialog,
// once per game. A scan of the whole level must neither show that dialog nor use it up.
class IllegalDecorationWarningSilencer
{
public:
  IllegalDecorationWarningSilencer(void) : m_flagsOfTheGame(d.newFlags)
  {
    d.newFlags |= IllegalWallDecorationWarning;
  }
  ~IllegalDecorationWarningSilencer(void)
  {
    d.newFlags = m_flagsOfTheGame;
  }

private:
  i16 m_flagsOfTheGame;
};

static SUMMARIZEROOMDATA Summarize(int x, int y, AutoMapDirection viewerFacing)
{
  SUMMARIZEROOMDATA summary;
  summary.x = x;
  summary.y = y;
  SummarizeRoomInfo(&summary, viewerFacing);
  return summary;
}

static int WallFeaturePicture(i32 ordinalOnLevel)
{
  bool isStandardFeature = ordinalOnLevel > 0
                        && (ordinalOnLevel & CUSTOM_WALL_GRAPHIC) == 0
                        && ordinalOnLevel <= d.NumWallDecoration
                        && ordinalOnLevel <= LEVEL_FEATURE_TABLE_SIZE;
  if (!isStandardFeature) return AUTOMAP_NO_PICTURE;
  return d.WallDecorationsForLevel[ordinalOnLevel - 1][0] + FRONT_VIEW_OF_WALL_FEATURE;
}

static int FloorFeaturePicture(i32 ordinalOnLevel)
{
  ordinalOnLevel &= ~FOOTPRINTS;
  bool isStandardFeature = ordinalOnLevel > 0
                        && (ordinalOnLevel & CUSTOM_FLOOR_GRAPHIC) == 0
                        && ordinalOnLevel <= d.pCurLevelDesc->NumFloorDecorations()
                        && ordinalOnLevel <= LEVEL_FEATURE_TABLE_SIZE;
  if (!isStandardFeature) return AUTOMAP_NO_PICTURE;
  return d.LoadedFloorDecorations[ordinalOnLevel - 1][0] + NEAREST_VIEW_OF_FLOOR_FEATURE;
}

// The shown level's global decoration number for this wall ordinal, or -1 when the
// ordinal is not a standard feature (custom graphic or out of range). Same guard as
// the pictures. The reader rebuilds the shown level's own table, so this is that
// level's mapping, not the party level's stale one.
static int WallDecorationNumber(i32 ordinalOnLevel)
{
  bool isStandardFeature = ordinalOnLevel > 0
                        && (ordinalOnLevel & CUSTOM_WALL_GRAPHIC) == 0
                        && ordinalOnLevel <= d.NumWallDecoration
                        && ordinalOnLevel <= LEVEL_FEATURE_TABLE_SIZE;
  if (!isStandardFeature) return -1;
  return d.WallDecorationTOC[ordinalOnLevel - 1];
}

// Whether an ordinal on the shown level names one of its three alcove graphics:
// the level's own table maps the ordinal to a global decoration number, which must
// be one of the alcove numbers (Data.h Byte4038: 1, 2, 3 = Square, VI Altar,
// Ornate). The 3D view draws the items embedded in a wall only on such faces
// (Viewport Alcove-gated contents pass), so the map must too. The level's own table
// is read rather than IsAlcove(), whose AlcoveGraphicIndices only hold the shown
// level's alcoves while BuildTablesForLoadedLevel's tables are in place.
static bool SideShowsAlcove(i32 ordinalOnLevel)
{
  int decorationNumber = WallDecorationNumber(ordinalOnLevel);
  if (decorationNumber < 0) return false;
  for (int i = 0; i < 3; i++)
  {
    if (decorationNumber == d.Byte4038[i]) return true;
  }
  return false;
}

// An interactive wall feature the 3D view still opens or drinks from with no live
// actuator behind it: an alcove to reach into, or a water fountain (the engine marks
// the latter by comparing the wall decoration against Word4040, Code390e, and the
// party drinks on the click in Mouse.cpp). Such a face keeps its icon and contents.
static bool SideShowsUsableFeature(i32 ordinalOnLevel)
{
  int decorationNumber = WallDecorationNumber(ordinalOnLevel);
  if (decorationNumber < 0) return false;
  for (int i = 0; i < 3; i++)
  {
    if (decorationNumber == d.Byte4038[i]) return true;
  }
  return d.Word4040[0] != 0 && decorationNumber == d.Word4040[0];
}

// What the face is for, for the search: the level's water fountain (Word4040, as Code390e
// tells it), or the Vi altar, the second of the alcove numbers (Byte4038: Square, VI Altar,
// Ornate)
static AutoMapWallFeature WallFeatureOf(i32 ordinalOnLevel)
{
  int decorationNumber = WallDecorationNumber(ordinalOnLevel);
  if (decorationNumber < 0) return AutoMapWallFeatureNone;
  if (d.Word4040[0] != 0 && decorationNumber == d.Word4040[0]) return AutoMapWallFeatureFountain;
  if (decorationNumber == d.Byte4038[1]) return AutoMapWallFeatureViAltar;
  return AutoMapWallFeatureNone;
}

// The one type-0 decoration the map greys out as pure dressing instead of drawing its
// picture: the manacles. Every other wall graphic a type-0 actuator names (the Lord
// portraits, empty torch holders, unknown decorations) keeps its icon.
static bool SideIsPlainDressing(i32 ordinalOnLevel)
{
  return WallDecorationNumber(ordinalOnLevel) == MANACLES_WALL_DECORATION;
}

// Whether the 3D view shows anything for this actuator, the way SummarizeRoomInfo decides:
// a DSA only with a custom graphic, a portrait never when its graphic is hidden, anything
// else exactly when it names a wall graphic
static bool ActuatorShowsGraphic(DB3 *actuator)
{
  int type = actuator->actuatorType();
  if (type == 47)
  {
    int levelIndex = actuator->DSAselector();
    return DSALevelIndex[d.LoadedLevel][levelIndex] == 255;
  }
  if (type == 127 && (actuator->State() & PORTRAIT_HideGraphic) != 0) return false;
  return actuator->wallGraphicOrdinal() != 0;
}

// Type 0 is the engine's "do nothing" actuator: Mouse.cpp skips it when the party
// touches a wall and the stone-room timer dispatch (Timer.cpp) ignores it, so it can
// neither fire nor receive. Such a record is a wall decoration, not machinery.
static bool ActuatorIsInert(DB3 *actuator)
{
  return actuator != NULL && actuator->actuatorType() == 0;
}

AutoMapRecordId AutoMapLevelReader_RecordId(int recordInteger)
{
  RN record;
  record.ConstructFromInteger(recordInteger);
  if (record == RNeof || record == RNnul) return AUTOMAP_NO_RECORD;
  if (record.dbType() == dbUNKNOWN) return AUTOMAP_NO_RECORD;
  return ((AutoMapRecordId)record.dbNum() << 16) | record.idx();
}

static AutoMapRecordId RecordIdOf(RN record)
{
  return AutoMapLevelReader_RecordId(record.ConvertToInteger());
}

static void ClassifyWallSide(int ordinal, DB3 *actuator, AutoMapWallSide *side)
{
  side->functional = false;
  side->hidden = false;
  if (actuator == NULL)
  {
    // Random dressing, unless it is a usable feature the party can interact with.
    // Same per-level table check as SideShowsAlcove: IsAlcove would read the party
    // level's indices and misjudge any other shown level.
    if (SideShowsUsableFeature(ordinal)) side->functional = true;
    return;
  }
  if (ActuatorIsInert(actuator))
  {
    // A decoration that happens to be stored as an actuator. Only the manacles is
    // pure dressing the map greys to a quiet tick; every other type-0 decoration
    // keeps its original picture and writing, so the Lord portraits, torch holders
    // and unknown wall graphics stay visible.
    side->functional = !SideIsPlainDressing(ordinal);
    return;
  }
  if (ActuatorShowsGraphic(actuator)) side->functional = true;
  else side->hidden = true;
}

// The machinery on each side of a wall cell, by AutoMapDirection. A type-0
// decoration may share its side with a real actuator; keep the real one so the
// map still shows the torch holder or switch, not just the decoration.
static void WallActuators(int x, int y, DB3 *actuators[AUTOMAP_SIDE_COUNT],
                          std::vector<AutoMapRecordId> records[AUTOMAP_SIDE_COUNT])
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) actuators[side] = NULL;
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    if (object.dbType() != dbACTUATOR) continue;
    int side = object.pos();
    if (side < 0 || side >= AUTOMAP_SIDE_COUNT) continue;
    DB3 *actuator = GetRecordAddressDB3(object);
    records[side].push_back(RecordIdOf(object));
    DB3 *&slot = actuators[side];
    if (slot == NULL || ActuatorIsInert(slot) || !ActuatorIsInert(actuator)) slot = actuator;
  }
}

// Decoded excerpt of a DB2 text record, "" when undecodable. Wall texts use
// type 1 like the engine's step-on message, scrolls 0x8002 like DisplayScroll.
static std::string DecodedExcerpt(DB2 *text, i16 typeOfText)
{
  char raw[1000];
  if (DecodeText(raw, text, typeOfText, 990) == -1) return "";
  const char *translated = TranslateLanguage(raw);
  std::string full = translated == NULL ? raw : translated;
  std::string excerpt = AutoMapScene_MakeExcerpt(full);
  if (excerpt.empty()) return "";
  return excerpt;
}

static std::string WallTextExcerpt(RN textRecord)
{
  if (textRecord == RNeof || textRecord == RNnul) return "";
  if (textRecord.dbType() != dbTEXT) return "";
  return DecodedExcerpt(GetRecordAddressDB2(textRecord), 1);
}

static std::string ScrollTextExcerpt(RN scrollObject)
{
  DB7 *scroll = GetRecordAddressDB7(scrollObject);
  if (scroll == NULL) return "";
  RN textRecord = scroll->text();
  if (textRecord == RNeof || textRecord == RNnul) return "";
  if (textRecord.dbType() != dbTEXT) return "";
  std::string excerpt = DecodedExcerpt(GetRecordAddressDB2(textRecord), (i16)0x8002);
  if (excerpt.empty()) return "";
  return excerpt;
}

// A summary lists the sides to the right of, facing and to the left of the viewer; the far
// side is missing
static void ReadWallSides(const SUMMARIZEROOMDATA &summary, AutoMapDirection viewerFacing,
                          DB3 *actuators[AUTOMAP_SIDE_COUNT], AutoMapCellRecord *record)
{
  for (int i = 0; i < WALL_SIDES_IN_ONE_SUMMARY; i++)
  {
    int side = (viewerFacing + i + 1) % AUTOMAP_SIDE_COUNT;
    AutoMapWallSide &wallSide = record->wallSides[side];
    wallSide.picture = WallFeaturePicture(summary.decorations[i]);
    wallSide.hasWriting = summary.text[i] != RNeof;
    wallSide.text = WallTextExcerpt(summary.text[i]);
    wallSide.isAlcove = SideShowsAlcove(summary.decorations[i]);
    wallSide.feature = WallFeatureOf(summary.decorations[i]);
    ClassifyWallSide(summary.decorations[i], actuators[side], &wallSide);
  }
}

// Champion portrait on one side of a wall cell (actuator type 127, e.g. the
// Hall of Champions on DM L0): the champion index 0..511 the 3D view draws,
// AUTOMAP_NO_PICTURE when none or hidden. Mirrors Codea59a.cpp: the portrait
// shows unless PORTRAIT_HidePortrait is set, independent of the frame graphic
// (PORTRAIT_HideGraphic) and of the alcove gate below, which this never touches.
static int PortraitOnSide(int x, int y, int side)
{
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    if (object.dbType() != dbACTUATOR) continue;
    if (object.pos() != side) continue;
    DB3 *actuator = GetRecordAddressDB3(object);
    if (actuator == NULL) continue;
    if (actuator->actuatorType() != 127) continue;
    if ((actuator->State() & PORTRAIT_HidePortrait) != 0) continue;
    return actuator->value() & 511;
  }
  return AUTOMAP_NO_PICTURE;
}

static void ReadWall(int x, int y, const SUMMARIZEROOMDATA &seenFromSouth, AutoMapCellRecord *record)
{
  DB3 *actuators[AUTOMAP_SIDE_COUNT];
  std::vector<AutoMapRecordId> actuatorRecords[AUTOMAP_SIDE_COUNT];
  WallActuators(x, y, actuators, actuatorRecords);
  record->kind = AutoMapCellWall;
  ReadWallSides(seenFromSouth, AutoMapNorth, actuators, record);
  ReadWallSides(Summarize(x, y, AutoMapSouth), AutoMapSouth, actuators, record);
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    record->wallSides[side].portrait = PortraitOnSide(x, y, side);
    record->wallSides[side].actuators = actuatorRecords[side];
  }
}

// A damaged dungeon can hold the unused values 6 and 7
static AutoMapDoorState DoorStateOf(int engineValue)
{
  if (engineValue > AutoMapDoorDestroyed) return AutoMapDoorDestroyed;
  return (AutoMapDoorState)engineValue;
}

static void ReadDoor(int x, int y, const SUMMARIZEROOMDATA &seenFromSouth, AutoMapCellRecord *record)
{
  record->kind = AutoMapCellDoor;
  record->runsNorthSouth = seenFromSouth.roomType == roomDOORFACING;
  // Only a viewer who looks along a door is told how far it is open
  SUMMARIZEROOMDATA seenAlongDoor =
      record->runsNorthSouth ? seenFromSouth : Summarize(x, y, AutoMapEast);
  record->doorState = DoorStateOf(seenAlongDoor.decorations[0]);
  DB0 *door = FirstObjectRecordAddressDB0(x, y);
  if (door != NULL)
  {
    record->doorHasSwitch = door->doorSwitch() != 0;
    record->doorBreakableWithAxe = door->canOpenWithAxe();
    record->doorBreakableWithFireball = door->canOpenWithFireball();
    record->doorBreakForce = d.DoorTOC[door->doorType()] & 0xff;
  }
}



// What SummarizeRoomInfo reports and what it becomes on the map:
//
//   roomSTONE                              wall with the features and writing of its four sides
//   roomOPEN, also an open false wall     floor, with its floor decoration
//     (a concealed closed pit reports as roomOPEN but the raw type brings it
//      back as a pit; a normal closed pit stays floor until it opens)
//   roomPIT, decorations[0] set            concealed pit
//   roomPIT, decorations[0] clear          pit
//   roomTELEPORTER, decorations[0] set     teleporter, visible and firing
//   roomTELEPORTER, decorations[0] clear   an invisible teleporter that fires, a dormant one
//                                          that fires nothing, or a plain floor when the cell
//                                          holds no teleporter. Firing follows the active bit
//                                          of the cell flags, showing follows decorations[0].
//                                          A teleporter targeting its own cell is a spinner
//                                          (live while the active bit is set, else dormant).
//   roomSTAIRFACING / roomSTAIREDGE        stairs running north-south / east-west;
//                                          decorations[0] set means up
//   roomDOORFACING / roomDOOREDGE          door running north-south / east-west;
//                                          decorations[0] of the facing view is its position
//
// The summary hides three things on purpose and the raw cell flags bring the
// hidden ones back: a false wall is reported as stone, a concealed closed pit
// as open floor, and an invisible teleporter as floor.

// The room type the dungeon data holds, before SummarizeRoomInfo hides false walls
// as stone for the 3D view
static ROOMTYPE RawRoomType(int x, int y)
{
  return (ROOMTYPE)((GetCellFlags(x, y) >> 5) & 0x7);
}



// A teleporter that keeps its victim on its own cell spins it instead of moving it
static bool TeleporterSpinsInPlace(DB1 *teleporter, int level, int x, int y)
{
  return teleporter->destLevel() == level && teleporter->destX() == x && teleporter->destY() == y;
}

static void NoteTeleporterTarget(DB1 *teleporter, AutoMapCellRecord *record)
{
  if (teleporter == NULL) return;
  record->hasTeleporterTarget = true;
  record->teleporterDestLevel = teleporter->destLevel();
  record->teleporterDestX = teleporter->destX();
  record->teleporterDestY = teleporter->destY();
}

// The level's offset in global coordinates; declared here, defined below.
static void LevelOffset(int level, int *offsetX, int *offsetY);

static void ResolveTeleporter(int x, int y, int level, const SUMMARIZEROOMDATA &seenFromSouth,
                               AutoMapCellRecord *record)
{
  DB1 *teleporter = FirstObjectRecordAddressDB1(x, y);
  if (teleporter != NULL && TeleporterSpinsInPlace(teleporter, level, x, y))
  {
    // The rotation is an exit facing in the engine's 0=north numbering when the facing
    // mode is absolute; a relative spin leaves the exit facing unknown. The circular
    // arrow already shows the self-target, so no destination is recorded. The
    // rotation itself is always kept so the tooltip can name the relative turn.
    record->spinnerRotation = teleporter->rotation();
    if (teleporter->facingMode() != 0) record->spinnerFacing = teleporter->rotation();
    record->spinnerIsVisible = seenFromSouth.decorations[0] != 0;
    // A spinner is a teleporter, so the same room-active bit 0x08 that gates firing
    // (Code11f52.cpp, ProcessTT_TELEPORTER toggles it) decides whether it still spins:
    // clear means an actuator switched it off, and it reads as a dormant spinner.
    record->kind = (GetCellFlags(x, y) & 8) != 0 ? AutoMapCellSpinner
                                                 : AutoMapCellDormantSpinner;
  }
  else if (seenFromSouth.decorations[0] != 0)
  {
    record->kind = AutoMapCellTeleporter;
    NoteTeleporterTarget(teleporter, record);
    if (teleporter != NULL)
    {
      record->teleporterScope = teleporter->what();
      record->teleporterRotation = teleporter->rotation();
      record->teleporterFacingMode = teleporter->facingMode();
    }
  }
  else if (teleporter != NULL && (GetCellFlags(x, y) & 8) != 0)
  {
    record->kind = AutoMapCellInvisibleTeleporter;
    NoteTeleporterTarget(teleporter, record);
    record->teleporterScope = teleporter->what();
    record->teleporterRotation = teleporter->rotation();
    record->teleporterFacingMode = teleporter->facingMode();
  }
  else if (teleporter != NULL)
  {
    // A dormant teleporter fires nothing, but its record still describes what
    // it would do if reactivated: the tooltip names it as a hypothetical. The
    // target shares the firing visibility rule, so the destination leaks
    // nothing the live arrow would hide; the arrow itself stays firing-only
    // (the window gates it on the cell kind).
    record->kind = AutoMapCellDormantTeleporter;
    NoteTeleporterTarget(teleporter, record);
    record->teleporterScope = teleporter->what();
    record->teleporterRotation = teleporter->rotation();
    record->teleporterFacingMode = teleporter->facingMode();
  }
  else record->kind = AutoMapCellFloor;
  if (record->hasTeleporterTarget)
  {
    // The destination in the dungeon's global coordinates, the frame the
    // rulers and the tooltip use. The destination level has its own offset.
    int offsetX = 0, offsetY = 0;
    LevelOffset(record->teleporterDestLevel, &offsetX, &offsetY);
    record->teleporterDestGlobalX = record->teleporterDestX + offsetX;
    record->teleporterDestGlobalY = record->teleporterDestY + offsetY;
  }
}

static AutoMapCellRecord ReadCell(int x, int y, int level)
{
  AutoMapCellRecord record;
  SUMMARIZEROOMDATA seenFromSouth = Summarize(x, y, AutoMapNorth);
  // A false wall looks like stone or like an open passage; the raw type tells which
  // illusion a stone-looking or open-looking cell is. A closed pit looks like open
  // floor the same way (Codea59a.cpp reports roomOPEN when the open bit 0x08 is
  // clear); the raw type brings a concealed one back so invisible pits still
  // draw as pits, while a normal switchable pit stays floor until it opens.
  bool rawFalseWall = RawRoomType(x, y) == roomFALSEWALL;
  bool rawPit = RawRoomType(x, y) == roomPIT;
  switch (seenFromSouth.roomType)
  {
  case roomSTONE:
    ReadWall(x, y, seenFromSouth, &record);
    record.kind = rawFalseWall ? AutoMapCellFalseWall : AutoMapCellWall;
    // Stone in the 3D view, but the party walks through it (Mouse.cpp's wall sound and
    // Timer.cpp's move test read the same bit)
    record.falseWallIsPassable = rawFalseWall && (GetCellFlags(x, y) & 1) != 0;
    return record;
  case roomPIT:
    record.kind = AutoMapCellPit;
    record.floorPicture = seenFromSouth.decorations[0] != 0 ? CONCEALED_PIT_PICTURE : PIT_PICTURE;
    // Open pit: the obscure bit 0x04 (decorations[0]) gives the concealed look
    // that reads as floor in 3D, hence hidden on the map without its marker.
    record.pitIsOpen = true;
    record.pitIsHidden = seenFromSouth.decorations[0] != 0;
    break;
  case roomTELEPORTER:
    record.kind = AutoMapCellFloor;
    record.floorPicture = FloorFeaturePicture(seenFromSouth.decorations[2]);
    ResolveTeleporter(x, y, level, seenFromSouth, &record);
    break;
  case roomSTAIRFACING:
  case roomSTAIREDGE:
    record.kind = AutoMapCellStairs;
    record.runsNorthSouth = seenFromSouth.roomType == roomSTAIRFACING;
    record.stairsLeadUp = seenFromSouth.decorations[0] != 0;
    break;
  case roomDOORFACING:
  case roomDOOREDGE:
    ReadDoor(x, y, seenFromSouth, &record);
    break;
  default:
    if (rawPit && (GetCellFlags(x, y) & 4) != 0)
    {
      // Concealed closed pit (e.g. 11,30,46-49): the summary reports roomOPEN
      // with a floor decoration, but the cell hides a pit that reads as floor
      // even when open. Draw it as a pit so the map never hides it.
      // Closed pits always look like floor in 3D, hence always hidden.
      record.kind = AutoMapCellPit;
      record.floorPicture = CONCEALED_PIT_PICTURE;
      record.pitIsOpen = false;
      record.pitIsHidden = true;
    }
    else
    {
      // Plain floor, or a normal switchable pit while closed (e.g.
      // 11,35,31-11,36,32): it looks and acts like floor until something
      // opens it, so the map shows floor. The obscure bit 0x04 above is
      // static dungeon data, orthogonal to the open bit 0x08.
      record.kind = AutoMapCellFloor;
      record.floorPicture = FloorFeaturePicture(seenFromSouth.decorations[2]);
      // Still a pit to whoever opens it: All names it and outlines it
      record.pitIsClosedSwitchable = rawPit;
    }
    break;
  }
  if (rawFalseWall) record.kind = AutoMapCellOpenFalseWall;
  return record;
}

static AutoMapMonsterKind KindOfType(int monsterType)
{
  MONSTERDESC *type = &d.MonsterDescriptor[monsterType];
  ITEM12 *pictures = &d.Item6414[type->uByte0];
  AutoMapMonsterKind kind;
  kind.frontPicture = FIRST_MONSTER_PICTURE + pictures->ITEM12_word0;
  kind.hasSidePicture = type->word4.HasSideGraphic();
  kind.hasBackPicture = type->word4.HasBackGraphic();
  kind.transparentColour = pictures->uByte10 & 15;
  return kind;
}

static AutoMapMonsterKind KindOf(DB4 *group)
{
  return KindOfType(group->monsterType());
}

// Which unit a live generator creates: the low five bits of its value are the monster
// type CreateMonster uses. Spent generators (and anything unresolvable) keep the flag off.
static const int GENERATOR_MONSTER_TYPES = 27;

// Statistics.cpp: honours custom dungeon names from the expool, falls back to
// "Name(graphic)". Declared here so the reader can resolve the display name;
// the scene only carries the resulting string.
void GetMonsterName(int monsterType, int graphic, char *name);

// The default "Name(graphic)" suffix carries no meaning on the map (the
// variant is already drawn); custom dungeon names come without one and are
// kept whole.
static std::string StripGraphicSuffix(const char *raw)
{
  std::string name = raw == NULL ? "" : raw;
  while (!name.empty() && name[name.size() - 1] == ' ') name.erase(name.size() - 1);
  if (name.empty() || name[name.size() - 1] != ')') return name;
  size_t open = name.rfind('(');
  if (open == std::string::npos || open + 1 >= name.size() - 1) return name;
  for (size_t i = open + 1; i < name.size() - 1; i++)
  {
    if (name[i] < '0' || name[i] > '9') return name;
  }
  name.erase(open);
  while (!name.empty() && name[name.size() - 1] == ' ') name.erase(name.size() - 1);
  return name;
}

static void ResolveGeneratedMonster(DB3 *actuator, AutoMapFloorActuator *pad, int level)
{
  if (pad->kind != AutoMapActuatorGenerator) return;
  int monsterType = actuator->value() & 0x1f;
  if (monsterType < 0 || monsterType >= GENERATOR_MONSTER_TYPES) return;
  pad->hasGeneratedMonster = true;
  pad->generatedKind = KindOfType(monsterType);
  char raw[80];
  GetMonsterName(monsterType, (actuator->value() >> 5) & 3, raw);
  pad->generatedMonsterName = StripGraphicSuffix(raw);
  // How many units one firing creates (Timer.cpp): the delay, unless bit 3
  // asks for 1..(delay & 7) at random. Shown only when exact and above one.
  int delay = actuator->delay();
  if ((delay & 0x8) == 0 && delay > 1) pad->generatedCount = delay;
  // Spawn health preview (Monster.cpp CreateMonster, Timer.cpp generator firing).
  // The level multiplier comes from the shown level, not the party level, so
  // cross-level snapshots read their own dungeon.dat value. Missing descriptors
  // (e.g. mid-load) leave the health line off rather than guessing.
  int levelMultiplier = -1;
  if (d.dungeonDatIndex != NULL && d.pLevelDescriptors != NULL
      && level >= 0 && level < d.dungeonDatIndex->NumLevel())
  {
    levelMultiplier = d.pLevelDescriptors[level].experienceMultiplier();
  }
  int healthMultiplier = actuator->hitPointMultiplier();
  int minHealth = -1;
  int maxHealth = -1;
  if (!AutoMapScene_GeneratorHealthRange(levelMultiplier, healthMultiplier,
                                         d.MonsterDescriptor[monsterType].baseHealth09,
                                         &minHealth, &maxHealth))
  {
    return;
  }
  pad->hasGeneratorHealth = true;
  pad->levelMultiplier = levelMultiplier;
  pad->healthMultiplier = healthMultiplier;
  pad->minHealth = minHealth;
  pad->maxHealth = maxHealth;
}

// The members of a group on the party's level turn one by one; the engine keeps their
// facings in its table of active groups. Groups on other levels are asleep: every member
// looks the way the group does.
static AutoMapDirection FacingOfMember(DB4 *group, int member, int shownLevel)
{
  if (shownLevel != d.partyLevel) return (AutoMapDirection)group->facing();
  if (group->groupIndex() >= d.MaxITEM16) return (AutoMapDirection)group->facing();
  return (AutoMapDirection)TwoBitNibble(d.Item16[group->groupIndex()].facings(), member);
}

// Start of a monster type's death-residue list in d.MonsterDroppings, mirroring
// CreateAndDropObject: 0-terminated, 0x8000 entries drop only half the time.
static i16 *ResidueListStart(MONSTERTYPE monsterType)
{
  switch (monsterType)
  {
  case mon_Skeleton: return &d.MonsterDroppings[37];
  case mon_StoneGolem: return &d.MonsterDroppings[35];
  case mon_Trolin: return &d.MonsterDroppings[33];
  case mon_AnimatedArmour: return &d.MonsterDroppings[26];
  case mon_RockPile: return &d.MonsterDroppings[21];
  case mon_PainRat: return &d.MonsterDroppings[18];
  case mon_Screamer: return &d.MonsterDroppings[15];
  case mon_Worm: return &d.MonsterDroppings[11];
  case mon_Dragon: return &d.MonsterDroppings[0];
  default: return NULL;
  }
}

// Inventory tiles mirror GetIconGraphic: the engine keeps 16x16 tiles in basic
// graphics 42..48, one graphic per 32 object types, split by d.Word612.
static const int FIRST_ICON_GRAPHIC = 42;
static const int ICON_GRAPHICS = 7;
static const int ICON_TILE_PIXELS = 16;

// Inventory tile for an object type, mirroring GetIconGraphic. False when the
// type names no tile.
static bool IconForType(int objType, int *graphic, int *tile)
{
  int group = 0;
  for (; group < ICON_GRAPHICS; group++)
  {
    if (d.Word612[group] > objType) break;
  }
  group--;
  if (group < 0 || group >= ICON_GRAPHICS) return false;
  *graphic = FIRST_ICON_GRAPHIC + group;
  *tile = objType - d.Word612[group];
  return true;
}

// One item with everything the tooltip needs: the world sprite, the inventory
// tile and the name. Unknown when the descriptor has no graphic class.
static AutoMapItem ItemForDescriptor(int descIndex)
{
  AutoMapItem item;
  int descriptors = (int)(sizeof(d.ObjDesc) / sizeof(d.ObjDesc[0]));
  if (descIndex < 0 || descIndex >= descriptors) return item;
  i32 graphicClass = d.ObjDesc[descIndex].GraphicClass();
  if (graphicClass < 0 || graphicClass >= GRAPHIC_CLASS_COUNT) return item;
  item.worldPicture = FIRST_ITEM_PICTURE + (ui8)d.s5604[graphicClass].objectGraphicIndex;
  int objType = d.ObjDesc[descIndex].objectType();
  int names = (int)(sizeof(d.ObjectNames) / sizeof(d.ObjectNames[0]));
  if (objType < 0 || objType >= names) return item;
  item.nameIndex = objType;
  int graphic = AUTOMAP_NO_PICTURE;
  int tile = 0;
  if (!IconForType(objType, &graphic, &tile)) return item;
  item.hasIcon = true;
  item.iconGraphic = graphic;
  item.iconTile = tile;
  return item;
}

static AutoMapItem ItemForRN(RN object)
{
  AutoMapItem item = ItemForDescriptor(object.DescIndex());
  if (item.worldPicture == AUTOMAP_NO_PICTURE) return item;
  // The display type, the way the inventory draws the item: charged weapons and
  // tools (Fury, Bolt Blade, Eye Of Time, ...) show their charged icon and name
  // while they hold charges. The world sprite above stays the basic one, which is
  // what the 3D view draws.
  int displayType = object.NameIndex();
  int names = (int)(sizeof(d.ObjectNames) / sizeof(d.ObjectNames[0]));
  if (displayType < 0 || displayType >= names) return item;
  item.nameIndex = displayType;
  int graphic = AUTOMAP_NO_PICTURE;
  int tile = 0;
  if (!IconForType(displayType, &graphic, &tile))
  {
    item.hasIcon = false;
    return item;
  }
  item.hasIcon = true;
  item.iconGraphic = graphic;
  item.iconTile = tile;
  return item;
}

// An object description's item: ItemForRN without the live record, so the display type is the
// description's own (a charged weapon shows its uncharged name). Built once per dungeon.
static AutoMapItem ItemForDescIndex(int descIndex)
{
  return ItemForDescriptor(descIndex);
}

// d.ObjDesc belongs to the dungeon, so the table is dropped when another one is loaded
static std::map<int, AutoMapItem> itemsOfDescriptions;
static bool itemsOfDescriptionsAreFilled = false;

void AutoMapLevelReader_ForgetDungeon(void)
{
  itemsOfDescriptions.clear();
  itemsOfDescriptionsAreFilled = false;
}

static const std::map<int, AutoMapItem> &ItemsOfAllDescriptions(void)
{
  if (!itemsOfDescriptionsAreFilled)
  {
    int descriptors = (int)(sizeof(d.ObjDesc) / sizeof(d.ObjDesc[0]));
    for (int descIndex = 0; descIndex < descriptors; descIndex++)
    {
      AutoMapItem item = ItemForDescIndex(descIndex);
      if (item.worldPicture != AUTOMAP_NO_PICTURE) itemsOfDescriptions[descIndex] = item;
    }
    itemsOfDescriptionsAreFilled = true;
  }
  return itemsOfDescriptions;
}

static AutoMapFloorItemTag FloorItemTagOf(RN object)
{
  AutoMapFloorItemTag tag = {RecordIdOf(object), object.DescIndex()};
  return tag;
}

AutoMapFloorItemTag AutoMapLevelReader_FloorItemTag(int object)
{
  RN record;
  record.ConstructFromInteger(object);
  return FloorItemTagOf(record);
}

// The loaded level's floor items on (x, y), the ones ReadObjectsAt would list: not in a wall,
// weapons to misc, with a picture
std::vector<AutoMapFloorItemTag> AutoMapLevelReader_FloorItemsAt(int x, int y)
{
  std::vector<AutoMapFloorItemTag> tags;
  AutoMapCellRecord cell = ReadCell(x, y, d.LoadedLevel);
  if (cell.kind == AutoMapCellWall || cell.kind == AutoMapCellFalseWall) return tags;
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    DBTYPE type = object.dbType();
    if (type < dbWEAPON || type > dbMISC) continue;
    if (ItemForRN(object).worldPicture == AUTOMAP_NO_PICTURE) continue;
    tags.push_back(FloorItemTagOf(object));
  }
  return tags;
}

std::vector<AutoMapFloorItemPlace> AutoMapLevelReader_FloorItemPlacesAt(int x, int y)
{
  std::vector<AutoMapFloorItemPlace> places;
  AutoMapCellRecord cell = ReadCell(x, y, d.LoadedLevel);
  if (cell.kind == AutoMapCellWall || cell.kind == AutoMapCellFalseWall) return places;
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    DBTYPE type = object.dbType();
    if (type < dbWEAPON || type > dbMISC) continue;
    AutoMapItem item = ItemForRN(object);
    if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    AutoMapFloorItemPlace place = {RecordIdOf(object), AutoMapLevelReader_ItemName(item.nameIndex),
                                   object.pos()};
    places.push_back(place);
  }
  return places;
}

// One cell of the loaded level, so the per-tick refresh never reads the whole level
AutoMapCellLook AutoMapLevelReader_CellLookAt(int x, int y)
{
  return AutoMapScene_LookOf(ReadCell(x, y, d.LoadedLevel));
}

// The descriptor a death-residue entry names: same value encoding as
// CreateAndDropObject. -1 when the entry names nothing.
static int ResidueDescriptor(i16 entry)
{
  int value = entry & 0x7fff;
  int descriptor = -1;
  if (value >= 127) descriptor = objDI_Compass + (value - 127);
  else if (value >= 69) descriptor = objDI_Cape + (value - 69);
  else if (value >= 23) descriptor = objDI_EyeOfTime + (value - 23);
  else return -1;
  int descriptors = (int)(sizeof(d.ObjDesc) / sizeof(d.ObjDesc[0]));
  if (descriptor < 0 || descriptor >= descriptors) return -1;
  return descriptor;
}

// The translated name of an object type for the tooltip footer. "" when unknown.
const char *AutoMapLevelReader_ItemName(int nameIndex)
{
  int names = (int)(sizeof(d.ObjectNames) / sizeof(d.ObjectNames[0]));
  if (nameIndex < 0 || nameIndex >= names) return "";
  const char *name = d.ObjectNames[nameIndex];
  if (name == NULL) return "";
  return TranslateLanguage(name);
}

bool AutoMapLevelReader_ItemIsFood(int nameIndex)
{
  return nameIndex >= objNI_Apple && nameIndex <= objNI_DragonSteak;
}

static void ReadMonster(RN monster, int x, int y, AutoMapLevelSnapshot *snapshot)
{
  DB4 *group = GetRecordAddressDB4(monster);
  if (group->invisible()) return;
  AutoMapMonsterRecord record;
  record.x = x;
  record.y = y;
  record.kind = KindOf(group);
  record.record = RecordIdOf(monster);
  record.memberCount = group->numMonM1() + 1;
  for (int member = 0; member < AUTOMAP_LARGEST_GROUP; member++)
  {
    record.memberFacings[member] = FacingOfMember(group, member, snapshot->level);
  }
  // Current hit points per member in group order. Unused slots read 0xffff;
  // only the members the group actually has are reported.
  for (int member = 0; member < record.memberCount && member < AUTOMAP_LARGEST_GROUP; member++)
  {
    if (group->hitPoints6[member] == 0xffff) continue;
    record.memberHealth.push_back(group->hitPoints6[member]);
  }
  // Carried items, like chest contents: one level, capped against looped chains.
  int guard = 0;
  for (RN held = group->possession(); held != RNeof && guard < 32;
       held = GetDBRecordLink(held), guard++)
  {
    DBTYPE heldType = held.dbType();
    if (heldType < dbWEAPON || heldType > dbMISC) continue;
    AutoMapItem item = ItemForRN(held);
    if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    record.possessionPictures.push_back(item);
    if (heldType == dbSCROLL) record.possessionTexts.push_back(ScrollTextExcerpt(held));
    else record.possessionTexts.push_back("");
  }
  // Death-residue preview: every member death rolls the table once (single deaths
  // call CreateAndDropObject directly, the final one via DropMonsterRemains), so a
  // group of N drops N table passes. Carried possessions above stay group-level.
  if (d.MonsterDescriptor[group->monsterType()].leavesDroppings())
  {
    std::vector<AutoMapItem> once;
    std::vector<AutoMapItem> chanceOnce;
    i16 *entry = ResidueListStart(group->monsterType());
    for (int scanned = 0; entry != NULL && *entry != 0 && scanned < 40; entry++, scanned++)
    {
      int descriptor = ResidueDescriptor(*entry);
      if (descriptor < 0) continue;
      AutoMapItem item = ItemForDescriptor(descriptor);
      if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
      if ((*entry & 0x8000) != 0) chanceOnce.push_back(item);
      else once.push_back(item);
    }
    int members = group->numMonM1() + 1;
    for (int member = 0; member < members; member++)
    {
      record.residuePictures.insert(record.residuePictures.end(), once.begin(), once.end());
      record.chanceResiduePictures.insert(record.chanceResiduePictures.end(),
                                          chanceOnce.begin(), chanceOnce.end());
    }
  }
  snapshot->monsters.push_back(record);
}

// The item the 3D view draws for the flying object, the way DrawMissiles works it
// out: an ordinary graphic class is an item picture, a negative one is a spell
// picture. Spell fire also reports its remaining range, which is what the 3D view
// sizes it by.
static void MissileLooks(RN missile, AutoMapItem *item, bool *sizeShowsRange, int *rangeRemaining)
{
  DB14 *record = GetRecordAddressDB14(missile);
  int graphicClass = GetGraphicClass(record->flyingObject());
  *sizeShowsRange = graphicClass < 0;
  *rangeRemaining = *sizeShowsRange ? record->rangeRemaining() : 0;
  if (graphicClass >= 0)
  {
    *item = ItemForRN(record->flyingObject());
    return;
  }
  item->worldPicture = AUTOMAP_NO_PICTURE;
  int index = -graphicClass - 1;
  if (index < 0 || index >= SPELL_MISSILE_PICTURE_COUNT) return;
  item->worldPicture = SPELL_MISSILE_PICTURE_BASE + d.struct5688[index].uByte0;
}



// The picture family the 3D view draws for a cloud of this type and density
static int CloudPicture(DB15 *cloud)
{
  int type = cloud->cloudType();
  int base;
  if (type == 0 || type == 2 || type == 101) base = 0;
  else if (type == 6 || type == 7) base = 2;
  else if (type == 40) base = 3;
  else base = 1;
  int index = base * 3;
  int density = cloud->value() >> 5;
  if (density != 0)
  {
    index++;
    if (density > 3) index++;
  }
  return CLOUD_PICTURE_BASE + index;
}



// Wall switches live for the wall-tier phase; here only the floor contraptions: pads of any
// operator type, dormant ones that cannot fire, generators (spent ones included), and the
// remaining machinery (counters, DSAs) as a generic cog
static bool FloorActuatorKind(int actuatorType, AutoMapActuatorKind *kind)
{
  switch (actuatorType)
  {
  case 1:
  case 2:
  case 3:
  case 4:
  case 7:
  case 8:
  case 9:
    *kind = AutoMapActuatorPad;
    return true;
  case 0:
    *kind = AutoMapActuatorDormantPad;
    return true;
  case 6:
    *kind = AutoMapActuatorGenerator;
    return true;
  case 65:
    *kind = AutoMapActuatorSpentGenerator;
    return true;
  case 47:
  case 48:
    *kind = AutoMapActuatorCog;
    return true;
  default:
    return false;
  }
}

static void LinkCell(AutoMapLevelSnapshot *snapshot, int srcX, int srcY, RN object, DB3 *actuator,
                     const AutoMapCellRecord *record);

// Who operates the pad, from the actuator type and value the way PressurePadOrAlcove reads
// them. Only live pads (kind Pad) carry an operator; every other contraption keeps Anything.
static void ResolvePadOperator(DB3 *actuator, AutoMapFloorActuator *pad);

// The record flags every relay entry carries, whatever its kind. andOrEdge is
// an extended-features field, so it reads 0 when those are off; localActionType
// asserts unless the record is local-action only.
static void FillRelayFlags(DB3 *actuator, AutoMapRelay *relay)
{
  relay->signal = (AutoMapSignal)(actuator->action() & 3);
  relay->normallyClosed = actuator->normallyClosed();
  relay->onceOnly = actuator->onceOnly() != 0;
  relay->delay = actuator->delay();
  relay->edgeMode = ExtendedFeaturesVersion != '@' ? (int)actuator->andOrEdge() : 0;
  relay->localActionType =
      actuator->localActionOnly() != 0 ? (int)actuator->localActionType() : -1;
}
static AutoMapItem ItemForObjectType(int basicObjectType);

// The item the 3D view would draw for an object of this basic type: the first
// descriptor with that type lends its identity, the way ReadObjectsAt resolves
// live items. Unknown when no descriptor or no graphic class matches.
static AutoMapItem ItemForObjectType(int basicObjectType)
{
  int descriptors = (int)(sizeof(d.ObjDesc) / sizeof(d.ObjDesc[0]));
  for (int i = 0; i < descriptors; i++)
  {
    if (d.ObjDesc[i].objectType() != basicObjectType) continue;
    AutoMapItem item = ItemForDescriptor(i);
    if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    return item;
  }
  return AutoMapItem();
}

static void ResolvePadOperator(DB3 *actuator, AutoMapFloorActuator *pad)
{
  if (pad->kind != AutoMapActuatorPad) return;
  int value = actuator->value();
  switch (actuator->actuatorType())
  {
  case 2: pad->op = AutoMapPadPartyOrMonster; break;
  case 3:
    if (value == 0) pad->op = AutoMapPadParty;
    else
    {
      pad->op = AutoMapPadPartyFacing;
      // The engine compares partyFacing + 1 against the value, in 0=north numbering
      pad->requiredFacing = (value >= 1 && value <= AUTOMAP_SIDE_COUNT) ? value - 1 : -1;
    }
    break;
  case 4:
    pad->op = AutoMapPadObject;
    pad->object = ItemForObjectType(value);
    break;
  case 7: pad->op = AutoMapPadMonster; break;
  case 8:
    pad->op = AutoMapPadPossession;
    pad->object = ItemForObjectType(value);
    break;
  case 9: pad->op = AutoMapPadParty; break;
  default: pad->op = AutoMapPadAnything; break;
  }
}

// The object a wall actuator wants in the party's hand, the way Mouse.cpp tests it:
// locks (3 = stays in hand, 4/11 = taken, 16 = swapped for the drawable object,
// 17 = one-shot) and torch holders (13) compare the basic object type in hand
// against the value. A stone alcove (2) compares the object placed in it against
// the value too (Code11f52.cpp PressurePadOrAlcove), so its value names the
// required object even though the same actuator also reacts to an empty-hand
// click (Mouse.cpp TouchWall). Pushbuttons (1) and any-object testers (12) name
// no object. Keyholes always read as needs; holders only when they operate
// something, plain holders are mere containers.
static void ResolveWallLock(RN actuatorRecord, DB3 *actuator, int side, bool hasAction, AutoMapCellRecord *record)
{
  int type = actuator->actuatorType();
  // A type 2 names an object only where an alcove actually holds one: the same
  // type doubles as an empty-hand switch (small switch, dent, ...) with a value
  // that names no niche item, and those must not read as NEEDS.
  if (type == 2
      && (record->kind != AutoMapCellWall || side < 0 || side >= AUTOMAP_SIDE_COUNT
          || !record->wallSides[side].isAlcove))
  {
    return;
  }
  if (type != 2 && type != 3 && type != 4 && type != 11 && type != 16 && type != 17
      && type != 13) return;
  AutoMapItem object = ItemForObjectType(actuator->value());
  if (object.worldPicture == AUTOMAP_NO_PICTURE) return;
  AutoMapWallLock lock;
  lock.side = side;
  lock.object = object;
  lock.actuatorType = type;
  lock.required = (type != 13) || hasAction;
  lock.actuator = RecordIdOf(actuatorRecord);
  record->wallLocks.push_back(lock);
}

// Whether a same-side actuator later in the chain keeps a last-only lock from
// operating: Mouse.cpp TouchWall skips types 11, 16 and 17 while actuators
// remain behind them on the same position (remainingActuators), stopping at
// the first monster-or-later record like the engine does. Such a lock reads
// as nothing until the blocker is gone; the per-tick rebuild then surfaces
// the next step (e.g. the gem wall on DM L13 names the firestaff only after
// the zokathra lock is used). A lock behind something permanent stays hidden,
// truthfully: it can never fire.
static bool SideHasLaterActuator(RN lock, int side)
{
  for (RN later = GetDBRecordLink(lock); later != RNeof; later = GetDBRecordLink(later))
  {
    if (later == RNnul) break;
    DBTYPE laterType = later.dbType();
    if (laterType >= dbMONSTER) break;
    if (laterType == dbACTUATOR && later.pos() == side) return true;
  }
  return false;
}

// Whether an item holder sits on this side of the cell: a torch holder
// (actuator type 13) or a champion portrait (actuator type 127). Their goods
// read as alcove goods although neither is an alcove: holders keep their
// torches, portraits hold their starting gear like a niche although the 3D
// view shows just the portrait.
static bool SideHasItemHolder(int x, int y, int side)
{
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    if (object.dbType() != dbACTUATOR) continue;
    if (object.pos() != side) continue;
    DB3 *actuator = GetRecordAddressDB3(object);
    if (actuator == NULL) continue;
    int holderType = actuator->actuatorType();
    if (holderType == 13 || holderType == 127) return true;
  }
  return false;
}

// Whether a wall lock sits on this side of the cell: a keyhole, coin slot or
// other item-accepting lock (the ResolveWallLock types except the holders,
// which SideHasItemHolder covers). Its inserted goods read as alcove goods
// although the face is no alcove: they are really there and the engine can hand
// them out, e.g. the copper coins in the coin slot on CSB L10 (36,36), which turns
// into an alcove holding them once a coin goes in. The 3D view draws goods only on
// an alcove face (Viewport.cpp, the Alcove step), so the map filter hides them
// below All until then. Plain lockless faces stay hidden, so embedded machinery
// never reads as goods.
static bool SideHasWallLock(int x, int y, int side)
{
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    if (object.dbType() != dbACTUATOR) continue;
    if (object.pos() != side) continue;
    DB3 *actuator = GetRecordAddressDB3(object);
    if (actuator == NULL) continue;
    int lockType = actuator->actuatorType();
    if (lockType == 3 || lockType == 4 || lockType == 11
        || lockType == 16 || lockType == 17) return true;
  }
  return false;
}

// Whether a local-action type-5 gate on this side rotates the actuator list when
// it fires (localActionType 1 or 2 -> RotateActuatorList, Code11f52.cpp). DM L5's
// four-alcove gate at (18,24) uses this to move a disabled graphic actuator
// behind itself and expose the Iron Key on the same side; that face is neither
// an alcove nor a lock, but the goods are still meant to be found. Keeping this
// to type 5 avoids exposing wall goods on every unrelated local actuator (torch
// holders, plaques, counters) whose word6 happens to read like a rotate action.
static bool SideRotatesActuatorList(int x, int y, int side)
{
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    if (object.dbType() != dbACTUATOR) continue;
    if (object.pos() != side) continue;
    DB3 *actuator = GetRecordAddressDB3(object);
    if (actuator == NULL) continue;
    if (actuator->actuatorType() != 5) continue;
    if (actuator->localActionOnly() == 0) continue;
    int localAction = actuator->localActionType();
    if (localAction == 1 || localAction == 2) return true;
  }
  return false;
}

// One walk over the objects of a cell feeds items, monsters, missiles, clouds, pads and
// the link set. A rebuild scans every cell on every tick, so each extra walk costs.
static void ReadObjectsAt(int x, int y, AutoMapCellRecord *record, AutoMapLevelSnapshot *snapshot)
{
  // Walls hold alcove goods instead of floor goods; doors hold no pads. Missiles,
  // clouds, monsters and links are collected everywhere: the engine never puts
  // them in walls, and the scan is free.
  bool isWall = record->kind == AutoMapCellWall || record->kind == AutoMapCellFalseWall;
  bool isDoor = record->kind == AutoMapCellDoor;
  bool takesFloorGoods = !isWall;
  bool takesPads = takesFloorGoods && !isDoor;
  bool monsterTaken = false;
  for (RN object = FindFirstObject(x, y); object != RNeof; object = GetDBRecordLink(object))
  {
    DBTYPE type = object.dbType();
    if (type >= dbWEAPON && type <= dbMISC)
    {
      if (!takesFloorGoods)
      {
        // Alcove contents: items resting in a wall niche, tagged with the side
        // they face (object position). Chests flatten one level deep like above,
        // so the tooltip shows what is inside without chest nesting.
        if (!isWall) continue;
        int side = object.pos();
        if (side < 0 || side >= AUTOMAP_SIDE_COUNT) continue;
        // Only usable faces read wall-embedded items as alcove goods: alcove
        // faces, item holders and lock faces. Anything sitting in a plain
        // lockless wall is hidden machinery (like the firestaff inside the
        // gem wall on DM L13) and must not read as goods that are not there.
        // Item holders keep their goods: torch holders their torches,
        // portraits their gear. Neither is an alcove. Lock faces keep theirs
        // too: coins, keys and exchange goods sit in their slots, though the
        // 3D view does not draw them (e.g. CSB L10 (36,36)). A face whose local actuator rotates the
        // actuator list keeps its goods as revealable: DM L5's (18,24) gate
        // moves the covering graphic aside and exposes the Iron Key.
        if (!record->wallSides[side].isAlcove && !SideHasItemHolder(x, y, side)
            && !SideHasWallLock(x, y, side)
            && !SideRotatesActuatorList(x, y, side)) continue;
        AutoMapItem item = ItemForRN(object);
        if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
        AutoMapAlcoveEntry entry;
        entry.item = item;
        entry.side = side;
        if (type == dbSCROLL)
        {
          entry.text = ScrollTextExcerpt(object);
          entry.scroll = RecordIdOf(object);
        }
        record->alcoveContents.push_back(entry);
        if (type == dbCHEST)
        {
          DB9 *chest = GetRecordAddressDB9(object);
          int guard = 0;
          for (RN held = chest->contents(); held != RNeof && guard < 32;
               held = GetDBRecordLink(held), guard++)
          {
            DBTYPE heldType = held.dbType();
            if (heldType < dbWEAPON || heldType > dbMISC) continue;
            AutoMapItem heldItem = ItemForRN(held);
            if (heldItem.worldPicture == AUTOMAP_NO_PICTURE) continue;
            AutoMapAlcoveEntry heldEntry;
            heldEntry.item = heldItem;
            heldEntry.side = side;
            heldEntry.inChest = RecordIdOf(object);
            if (heldType == dbSCROLL)
            {
              heldEntry.text = ScrollTextExcerpt(held);
              heldEntry.scroll = RecordIdOf(held);
            }
            record->alcoveContents.push_back(heldEntry);
          }
        }
        continue;
      }
      AutoMapItem item = ItemForRN(object);
      if (item.worldPicture == AUTOMAP_NO_PICTURE) continue;
      record->itemPictures.push_back(item);
      record->itemTags.push_back(FloorItemTagOf(object));
      if (type == dbSCROLL)
      {
        std::string excerpt = ScrollTextExcerpt(object);
        if (!excerpt.empty())
        {
          record->scrollTexts.push_back(excerpt);
          record->scrollRecords.push_back(RecordIdOf(object));
        }
      }
      if (type == dbCHEST)
      {
        // One level deep, capped: the 3D chest view shows 8 slots, and a looped
        // chain must never hang the per-tick rebuild.
        DB9 *chest = GetRecordAddressDB9(object);
        AutoMapChestContents contents;
        contents.item = record->itemPictures.size() - 1;
        contents.chest = RecordIdOf(object);
        int guard = 0;
        for (RN held = chest->contents(); held != RNeof && guard < 32;
             held = GetDBRecordLink(held), guard++)
        {
          DBTYPE heldType = held.dbType();
          if (heldType < dbWEAPON || heldType > dbMISC) continue;
          AutoMapItem heldItem = ItemForRN(held);
          if (heldItem.worldPicture == AUTOMAP_NO_PICTURE) continue;
          contents.pictures.push_back(heldItem);
          if (heldType == dbSCROLL)
          {
            contents.pictureTexts.push_back(ScrollTextExcerpt(held));
            contents.pictureScrolls.push_back(RecordIdOf(held));
          }
          else
          {
            contents.pictureTexts.push_back("");
            contents.pictureScrolls.push_back(AUTOMAP_NO_RECORD);
          }
        }
        if (!contents.pictures.empty()) record->chestContents.push_back(contents);
      }
    }
    else if (type == dbMONSTER)
    {
      // One group per cell at most, like FindFirstMonster assumes
      if (monsterTaken) continue;
      monsterTaken = true;
      ReadMonster(object, x, y, snapshot);
    }
    else if (type == dbMISSILE)
    {
      AutoMapMissileRecord missile;
      missile.x = x;
      missile.y = y;
      MissileLooks(object, &missile.item, &missile.sizeShowsRange, &missile.rangeRemaining);
      snapshot->missiles.push_back(missile);
    }
    else if (type == dbCLOUD)
    {
      AutoMapCloudRecord cloud;
      cloud.x = x;
      cloud.y = y;
      cloud.picture = CloudPicture(GetRecordAddressDB15(object));
      snapshot->clouds.push_back(cloud);
    }
    else if (type == dbACTUATOR)
    {
      DB3 *actuator = GetRecordAddressDB3(object);
      if (actuator->localActionOnly() == 0) LinkCell(snapshot, x, y, object, actuator, record);
      // Locks resolve on walls and doors alike: the position names the face they
      // sit on. Alcove goods stay wall-only; doors keep their floor goods.
      if (isWall || isDoor)
      {
        // Counters, gates and DSAs embedded in stone still count down / fire
        // from here: without this the relay behind a coin slot stays
        // undescribed. Type 6 in stone is a counter too (Timer.cpp treats 6 and
        // 48 alike, CSB.h: type 6 counts in a closed room and generates in an
        // open one); type 5 in stone is an And/Nor gate whose low nibble flips
        // per incoming message and fires on matching the high nibble. Walls are
        // stone while floors keep type 6 as a generator. Doors are skipped:
        // their counters are inert (ProcessTT_DOOR walks no actuators). Every
        // record keeps its own entry, unfolded; the per-tick rebuild keeps the
        // remainders live.
        if (record->kind == AutoMapCellWall)
        {
          int relayType = actuator->actuatorType();
          AutoMapRelay relay;
          bool isRelay = true;
          if (relayType == 48 || relayType == 6)
          {
            relay.kind = AutoMapRelayCounter;
            relay.value = actuator->value();
          }
          else if (relayType == 5)
          {
            relay.kind = AutoMapRelayGate;
            relay.value = actuator->value();
          }
          else if (relayType == 47)
          {
            relay.kind = AutoMapRelayDSA;
          }
          else
          {
            isRelay = false;
          }
          if (isRelay)
          {
            FillRelayFlags(actuator, &relay);
            relay.record = RecordIdOf(object);
            record->wallRelays.push_back(relay);
          }
        }
        // Missile launchers only operate in stone (Timer.cpp stone-room
        // handling calls TriggerMissileLauncher for types 7-10/14/15 whose
        // side matches the message): floor copies are inert and stay silent.
        // The local-action flag is irrelevant here, launching answers timer
        // messages either way.
        if (record->kind == AutoMapCellWall)
        {
          int launcherType = actuator->actuatorType();
          if (launcherType == 7 || launcherType == 8 || launcherType == 9
              || launcherType == 10 || launcherType == 14 || launcherType == 15)
          {
            AutoMapLauncher launcher;
            launcher.actuatorType = launcherType;
            launcher.value = actuator->value();
            launcher.actuator = RecordIdOf(object);
            int side = object.pos();
            if (side >= 0 && side < AUTOMAP_SIDE_COUNT) launcher.side = side;
            record->wallLaunchers.push_back(launcher);
          }
        }
        int side = object.pos();
        if (side >= 0 && side < AUTOMAP_SIDE_COUNT)
        {
          // An action means a remote target in bounds, the way LinkCell badges it:
          // holders with one operate something, holders without just hold things.
          // targetX/targetY assert a target exists, so the local check comes first.
          bool hasAction = actuator->localActionOnly() == 0
              && actuator->targetX() >= 0 && actuator->targetX() < snapshot->width
              && actuator->targetY() >= 0 && actuator->targetY() < snapshot->height;
          int lockType = actuator->actuatorType();
          bool lastOnly = lockType == 11 || lockType == 16 || lockType == 17;
          if (!lastOnly || !SideHasLaterActuator(object, side))
          {
            ResolveWallLock(object, actuator, side, hasAction, record);
          }
        }
        continue;
      }
      if (!takesPads) continue;
      AutoMapActuatorKind kind;
      if (!FloorActuatorKind(actuator->actuatorType(), &kind)) continue;
      AutoMapFloorActuator pad(x, y, kind);
      pad.record = RecordIdOf(object);
      if (kind == AutoMapActuatorCog)
      {
        if (actuator->actuatorType() == 48)
        {
          pad.counterValue = actuator->value();
          pad.relay.kind = AutoMapRelayCounter;
          pad.relay.value = pad.counterValue;
        }
        else
        {
          pad.relay.kind = AutoMapRelayDSA;
        }
        FillRelayFlags(actuator, &pad.relay);
        pad.relay.record = pad.record;
      }
      ResolvePadOperator(actuator, &pad);
      ResolveGeneratedMonster(actuator, &pad, snapshot->level);
      snapshot->actuators.push_back(pad);
    }
  }
}

// Every cell an actuator points at: the badge set for action-linked walls, plus the directed
// edge behind it for hover arrows. Targets name no level and always sit on the loaded one.
// A wall-cell actuator names the side it sits on (object position); anything else uses the
// cell centre.
static void LinkCell(AutoMapLevelSnapshot *snapshot, int srcX, int srcY, RN object, DB3 *actuator,
                     const AutoMapCellRecord *record)
{
  if (ActuatorIsInert(actuator)) return;
  int targetX = actuator->targetX();
  int targetY = actuator->targetY();
  if (targetX < 0 || targetX >= snapshot->width || targetY < 0 || targetY >= snapshot->height)
  {
    return;
  }
  bool badgeKnown = false;
  for (size_t i = 0; i < snapshot->linkedCells.size(); i++)
  {
    if (snapshot->linkedCells[i].x == targetX && snapshot->linkedCells[i].y == targetY)
    {
      badgeKnown = true;
      break;
    }
  }
  if (!badgeKnown)
  {
    AutoMapCell target = {snapshot->level, targetX, targetY};
    snapshot->linkedCells.push_back(target);
  }
  bool srcIsWall = (record->kind == AutoMapCellWall || record->kind == AutoMapCellFalseWall);
  int side = object.pos();
  if (!srcIsWall || side < 0 || side >= AUTOMAP_SIDE_COUNT)
  {
    srcIsWall = false;
    side = -1;
  }
  AutoMapSignal signal = (AutoMapSignal)(actuator->action() & 3);
  bool onceOnly = actuator->onceOnly() != 0;
  int delay = actuator->delay();
  // A counter source only fires when its count reaches zero (Timer.cpp
  // OperateCounterActuator); type 6 counts in stone and generates in the open,
  // so only stone-embedded 6s read as counters, the way the wall tally above does.
  // A type-5 gate source fires on matching its bit pattern (high nibble), which
  // rides along for the tooltip. targetPos names the bit for gate targets (and
  // the wall part for stone targets).
  bool fromCounter = actuator->actuatorType() == 48
      || (srcIsWall && actuator->actuatorType() == 6);
  int gatePattern = -1;
  if (srcIsWall && actuator->actuatorType() == 5)
  {
    gatePattern = (actuator->value() >> 4) & 15;
  }
  int targetPos = actuator->targetPos();
  for (size_t i = 0; i < snapshot->links.size(); i++)
  {
    const AutoMapLink &known = snapshot->links[i];
    if (known.srcX == srcX && known.srcY == srcY && known.dstX == targetX && known.dstY == targetY
        && known.srcIsWall == srcIsWall && known.srcSide == side && known.signal == signal
        && known.onceOnly == onceOnly && known.delay == delay
        && known.fromCounter == fromCounter
        // Two relays of one cell can send identical links; each fires on its own, so
        // the press walk needs both sources.
        && known.source == RecordIdOf(object)
        && known.targetPos == targetPos && known.gatePattern == gatePattern)
    {
      return;
    }
  }
  AutoMapLink edge = {srcX, srcY, targetX, targetY, srcIsWall, side};
  edge.signal = signal;
  edge.onceOnly = onceOnly;
  edge.delay = delay;
  edge.fromCounter = fromCounter;
  edge.targetPos = targetPos;
  edge.gatePattern = gatePattern;
  edge.source = RecordIdOf(object);
  edge.sourceNormallyClosed = actuator->normallyClosed();
  snapshot->links.push_back(edge);
}

// The colour schemes of the loaded level, the way ReadGraphicsForLevel works them out
// from its monster types. The engine repaints palette entries 9 and 10 with them.
static void ColourSchemesOfLoadedLevel(int *scheme9, int *scheme10)
{
  int s9 = AUTOMAP_DEFAULT_SCHEME9;
  int s10 = AUTOMAP_DEFAULT_SCHEME10;
  for (int i = 0; i < d.pCurLevelDesc->numMonsterType(); i++)
  {
    ui8 monsterType = (ui8)d.EndOfCELLFLAGS[i];
    int low = d.Item6414[monsterType].uByte11 & 15;
    int high = (d.Item6414[monsterType].uByte11 >> 4) & 15;
    if (low != 0) s9 = low - 1;
    if (high != 0) s10 = high - 1;
  }
  *scheme9 = s9;
  *scheme10 = s10;
}

static double TicksToMs(Uint64 ticks)
{
  return ticks * 1000000.0 / SDL_GetPerformanceFrequency() / 1000.0;
}

static void ReportReadSplit(Uint64 cellTicks, Uint64 objectTicks)
{
  if (!AutoMapWindow_TimingReportsOn()) return;
  printf("Automap timing: level read split cells %.1f ms objects %.1f ms\n",
         TicksToMs(cellTicks), TicksToMs(objectTicks));
}

std::vector<AutoMapLevelSize> AutoMapLevelReader_DungeonLevelSizes(void)
{
  std::vector<AutoMapLevelSize> sizes;
  if (d.dungeonDatIndex == NULL || d.pLevelDescriptors == NULL) return sizes;
  for (int level = 0; level < d.dungeonDatIndex->NumLevel(); level++)
  {
    AutoMapLevelSize size = {d.pLevelDescriptors[level].LastColumn() + 1,
                             d.pLevelDescriptors[level].LastRow() + 1};
    sizes.push_back(size);
  }
  return sizes;
}

// The level's offset in global coordinates. The descriptors can be unavailable while a saved
// game loads; then the level reads as if it started at the origin.
static void LevelOffset(int level, int *offsetX, int *offsetY)
{
  *offsetX = 0;
  *offsetY = 0;
  if (d.dungeonDatIndex == NULL || d.pLevelDescriptors == NULL) return;
  if (level < 0 || level >= d.dungeonDatIndex->NumLevel()) return;
  *offsetX = d.pLevelDescriptors[level].offsetX;
  *offsetY = d.pLevelDescriptors[level].offsetY;
}

void AutoMapLevelReader_LevelOffset(int level, int *offsetX, int *offsetY)
{
  LevelOffset(level, offsetX, offsetY);
}

// The list sits just past the level's cell flags (MonsterAllowedOnLevel, Code11f52.cpp)
std::vector<AutoMapMonsterChoice> AutoMapLevelReader_MonsterTypesOfLevel(int level)
{
  std::vector<AutoMapMonsterChoice> choices;
  if (d.dungeonDatIndex == NULL || d.pLevelDescriptors == NULL) return choices;
  if (d.pppdPointer10450 == NULL) return choices;
  if (level < 0 || level >= d.dungeonDatIndex->NumLevel()) return choices;
  LEVELDESC *descriptor = &d.pLevelDescriptors[level];
  const CELLFLAG *list =
      &d.pppdPointer10450[level][descriptor->LastColumn()][descriptor->LastRow() + 1];
  for (int i = 0; i < descriptor->numMonsterType(); i++)
  {
    int monsterType = (ui8)list[i];
    if (monsterType >= GENERATOR_MONSTER_TYPES) continue;
    char raw[80];
    GetMonsterName(monsterType, 0, raw);
    AutoMapMonsterChoice choice;
    choice.type = monsterType;
    choice.name = StripGraphicSuffix(raw);
    choice.horizontalSize = d.MonsterDescriptor[monsterType].horizontalSize();
    choices.push_back(choice);
  }
  return choices;
}

// Debug inspector for "what is this tile" questions: set CSB_AUTOMAP_INSPECT to a
// level ("5") or a level plus local cell ("5,12,7") and, while the map reads that
// level, every actuator there prints once with its engine fields, the way AsciiDump
// DumpDB3 names them: room type (0 = stone, 1 = open), wall side, actuator type,
// value, action, target or local action, once-only, delay, facing. Both local and
// global coordinates print so a hovered global cell is greppable. Change the value
// to dump again (live counter values tick down otherwise). Every face of the level the
// reader takes for an alcove prints too, once on the party's level and once from another.
// Nothing prints without the variable; no game behaviour changes.
static void MaybeInspectSnapshot(const AutoMapLevelSnapshot &snapshot)
{
  const char *spec = std::getenv("CSB_AUTOMAP_INSPECT");
  if (spec == NULL || spec[0] == '\0') return;
  int wantLevel = -1, wantX = -1, wantY = -1;
  int fields = std::sscanf(spec, "%d,%d,%d", &wantLevel, &wantX, &wantY);
  if (fields < 1 || wantLevel != snapshot.level) return;
  static std::string inspected;
  char key[64];
  std::snprintf(key, sizeof(key), "%s@%d party %d", spec, snapshot.level,
                snapshot.partyIsOnShownLevel ? 1 : 0);
  if (inspected == key) return;
  inspected = key;
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      if (fields >= 3 && (x != wantX || y != wantY)) continue;
      for (RN object = FindFirstObject(x, y); object != RNeof;
           object = GetDBRecordLink(object))
      {
        if (object.dbType() != dbACTUATOR) continue;
        DB3 *actuator = GetRecordAddressDB3(object);
        int roomType = (GetCellFlags(x, y) >> 5) & 7;
        if (actuator->localActionOnly() != 0)
        {
          std::printf("Automap inspect: L%d local (%d,%d) global (%d,%d) room=%d pos=%d "
                      "typ=%d val=%d localAction=%d OO=%d DL=%d %s\n",
                      snapshot.level, x, y, x + snapshot.offsetX, y + snapshot.offsetY,
                      roomType, object.pos(), actuator->actuatorType(), actuator->value(),
                      actuator->localActionType(), actuator->onceOnly(), actuator->delay(),
                      actuator->normallyClosed() ? "NC" : "NO");
        }
        else
        {
          std::printf("Automap inspect: L%d local (%d,%d) global (%d,%d) room=%d pos=%d "
                      "typ=%d val=%d act=%d target=(%d,%d,%d) OO=%d DL=%d %s\n",
                      snapshot.level, x, y, x + snapshot.offsetX, y + snapshot.offsetY,
                      roomType, object.pos(), actuator->actuatorType(), actuator->value(),
                      (int)actuator->action(), actuator->targetX(), actuator->targetY(),
                      actuator->targetPos(), actuator->onceOnly(), actuator->delay(),
                      actuator->normallyClosed() ? "NC" : "NO");
        }
      }
    }
  }
}

// The alcove part of the inspector, after the boundary rock is added: local coordinates count
// from the expanded grid
static void MaybeInspectAlcoves(const AutoMapLevelSnapshot &snapshot)
{
  const char *spec = std::getenv("CSB_AUTOMAP_INSPECT");
  if (spec == NULL || spec[0] == '\0') return;
  int wantLevel = -1;
  if (std::sscanf(spec, "%d", &wantLevel) < 1 || wantLevel != snapshot.level) return;
  static std::string inspected;
  char key[64];
  std::snprintf(key, sizeof(key), "%s@%d party %d", spec, snapshot.level,
                snapshot.partyIsOnShownLevel ? 1 : 0);
  if (inspected == key) return;
  inspected = key;
  // Every face the reader takes for an alcove, with the level's decoration table, to compare a
  // read on the party's level with one from another level
  std::string table;
  for (int i = 0; i < d.NumWallDecoration && i < LEVEL_FEATURE_TABLE_SIZE; i++)
  {
    char number[8];
    std::snprintf(number, sizeof(number), " %d", d.WallDecorationTOC[i]);
    table += number;
  }
  std::printf("Automap alcoves: L%d party on level %d, loaded %d, %dx%d, offset (%d,%d), "
              "%d wall decorations:%s, %d random\n",
              snapshot.level, snapshot.partyIsOnShownLevel ? 1 : 0, d.LoadedLevel, d.width,
              d.height, snapshot.offsetX, snapshot.offsetY, d.NumWallDecoration, table.c_str(),
              d.pCurLevelDesc->NumRandomWallDecoration());
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      const AutoMapCellRecord &cell = snapshot.cells[(size_t)y * snapshot.width + x];
      for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
      {
        const AutoMapWallSide &face = cell.wallSides[side];
        if (!face.isAlcove) continue;
        int goods = 0;
        for (size_t i = 0; i < cell.alcoveContents.size(); i++)
        {
          if (cell.alcoveContents[i].side == side) goods++;
        }
        std::printf("Automap alcoves:   global (%d,%d) local (%d,%d) kind %d side %d picture %d "
                    "functional %d actuators %d goods %d outside %d\n", x + snapshot.offsetX,
                    y + snapshot.offsetY, x, y, (int)cell.kind, side, face.picture,
                    face.functional ? 1 : 0, (int)face.actuators.size(), goods,
                    cell.isOutsideLevel ? 1 : 0);
      }
    }
  }
}

// The engine surrounds every level with solid rock: looking past a floor cell on the edge,
// GetCellFlags fabricates a stone cell whose random decoration the 3D view draws there
// (CSBCode GetCellFlags, e.g. 4 for the cell west of the west edge). The reader walked only
// the level's own cells, so those boundary walls never became cells and their features were
// lost (DM L4's west-edge water fountain at local (0,16), say). The fabrication only happens
// next to an open or pit edge cell (GetCellFlags returns 0 otherwise, since the solid edge
// cell already stands for the rock), so add a ring column or row only on a side that has one
// and leave the solid sides alone. The ring holds the fabricated walls, the real cells shift
// inward, and the offsets step back so the global coordinates the rulers and tooltips show do
// not move. Any level cell whose feature coordinates name this level shift with it.
static void ExpandSnapshotWithBoundaryRock(AutoMapLevelSnapshot *snapshot)
{
  if (snapshot->width <= 0 || snapshot->height <= 0) return;
  int oldWidth = snapshot->width;
  int oldHeight = snapshot->height;
  bool needWest = false, needEast = false, needNorth = false, needSouth = false;
  for (int y = 0; y < oldHeight; y++)
  {
    ROOMTYPE west = RawRoomType(0, y);
    if (west == roomOPEN || west == roomPIT) needWest = true;
    ROOMTYPE east = RawRoomType(oldWidth - 1, y);
    if (east == roomOPEN || east == roomPIT) needEast = true;
  }
  for (int x = 0; x < oldWidth; x++)
  {
    ROOMTYPE north = RawRoomType(x, 0);
    if (north == roomOPEN || north == roomPIT) needNorth = true;
    ROOMTYPE south = RawRoomType(x, oldHeight - 1);
    if (south == roomOPEN || south == roomPIT) needSouth = true;
  }
  int marginLeft = needWest ? 1 : 0;
  int marginRight = needEast ? 1 : 0;
  int marginTop = needNorth ? 1 : 0;
  int marginBottom = needSouth ? 1 : 0;
  if (marginLeft == 0 && marginRight == 0 && marginTop == 0 && marginBottom == 0) return;
  std::vector<AutoMapCellRecord> oldCells;
  oldCells.swap(snapshot->cells);
  int newWidth = oldWidth + marginLeft + marginRight;
  int newHeight = oldHeight + marginTop + marginBottom;
  snapshot->cells.assign((size_t)newWidth * newHeight, AutoMapCellRecord());
  for (int gy = 0; gy < newHeight; gy++)
  {
    int localY = gy - marginTop;
    for (int gx = 0; gx < newWidth; gx++)
    {
      int localX = gx - marginLeft;
      AutoMapCellRecord &record = snapshot->cells[(size_t)gy * newWidth + gx];
      if (localX >= 0 && localX < oldWidth && localY >= 0 && localY < oldHeight)
      {
        record = oldCells[(size_t)localY * oldWidth + localX];
      }
      else
      {
        record = ReadCell(localX, localY, snapshot->level);
        record.isOutsideLevel = true;
      }
    }
  }
  snapshot->width = newWidth;
  snapshot->height = newHeight;
  snapshot->offsetX -= marginLeft;
  snapshot->offsetY -= marginTop;
  snapshot->boundaryMarginX = marginLeft;
  snapshot->boundaryMarginY = marginTop;
  snapshot->party.x += marginLeft;
  snapshot->party.y += marginTop;
  for (size_t i = 0; i < snapshot->monsters.size(); i++)
  {
    snapshot->monsters[i].x += marginLeft;
    snapshot->monsters[i].y += marginTop;
  }
  for (size_t i = 0; i < snapshot->missiles.size(); i++)
  {
    snapshot->missiles[i].x += marginLeft;
    snapshot->missiles[i].y += marginTop;
  }
  for (size_t i = 0; i < snapshot->clouds.size(); i++)
  {
    snapshot->clouds[i].x += marginLeft;
    snapshot->clouds[i].y += marginTop;
  }
  for (size_t i = 0; i < snapshot->actuators.size(); i++)
  {
    snapshot->actuators[i].x += marginLeft;
    snapshot->actuators[i].y += marginTop;
  }
  for (size_t i = 0; i < snapshot->linkedCells.size(); i++)
  {
    snapshot->linkedCells[i].x += marginLeft;
    snapshot->linkedCells[i].y += marginTop;
  }
  for (size_t i = 0; i < snapshot->links.size(); i++)
  {
    snapshot->links[i].srcX += marginLeft;
    snapshot->links[i].srcY += marginTop;
    snapshot->links[i].dstX += marginLeft;
    snapshot->links[i].dstY += marginTop;
  }
  for (size_t i = 0; i < snapshot->cells.size(); i++)
  {
    AutoMapCellRecord &record = snapshot->cells[i];
    if (record.hasTeleporterTarget && record.teleporterDestLevel == snapshot->level)
    {
      // Same-level destinations name a cell of this level, so they shift with the grid.
      // Cross-level ones stay in the destination level's own coordinates. The global
      // destination is already stored and does not move.
      record.teleporterDestX += marginLeft;
      record.teleporterDestY += marginTop;
    }
  }
}

AutoMapLevelSnapshot AutoMapLevelReader_ReadPartyLevel(void)
{
  IllegalDecorationWarningSilencer silencer;
  AutoMapLevelSnapshot snapshot;
  snapshot.level = d.partyLevel;
  snapshot.width = d.width;
  snapshot.height = d.height;
  LevelOffset(snapshot.level, &snapshot.offsetX, &snapshot.offsetY);
  snapshot.party.x = d.partyX;
  snapshot.party.y = d.partyY;
  snapshot.party.facing = (AutoMapDirection)d.partyFacing;
  snapshot.partyIsOnShownLevel = true;
  ColourSchemesOfLoadedLevel(&snapshot.paletteScheme9, &snapshot.paletteScheme10);
  Uint64 cellTicks = 0, objectTicks = 0;
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      Uint64 cellStart = SDL_GetPerformanceCounter();
      snapshot.cells.push_back(ReadCell(x, y, snapshot.level));
      cellTicks += SDL_GetPerformanceCounter() - cellStart;
      Uint64 objectStart = SDL_GetPerformanceCounter();
      ReadObjectsAt(x, y, &snapshot.cells.back(), &snapshot);
      objectTicks += SDL_GetPerformanceCounter() - objectStart;
    }
  }
  ReportReadSplit(cellTicks, objectTicks);
  snapshot.itemOfDescIndex = ItemsOfAllDescriptions();
  MaybeInspectSnapshot(snapshot);
  ExpandSnapshotWithBoundaryRock(&snapshot);
  MaybeInspectAlcoves(snapshot);
  return snapshot;
}

// Everything LoadLevel leaves behind: the per-level decoration tables that SetPartyLevel
// and ReadGraphicsForLevel build for the party's level. Saved and restored around the
// temporary load so the game keeps drawing its own level afterwards.
struct LevelTables
{
  CELLFLAG *endOfCellFlags;
  i16 numWallDecoration;
  ui8 wallTOC[16];
  i8 floorTOC[16];
  i8 doorTOC[18];
  i16 wallGraphics[16][2];
  i16 floorGraphics[16][2];
  i16 doorTOCCharacteristics[2];
  i16 fountainIndex;
  i16 alcoveIndices[3];
  i16 ornateAlcoveIndex;
};

static void SaveLevelTables(LevelTables *saved)
{
  saved->endOfCellFlags = d.EndOfCELLFLAGS;
  saved->numWallDecoration = d.NumWallDecoration;
  std::memcpy(saved->wallTOC, d.WallDecorationTOC, sizeof(saved->wallTOC));
  std::memcpy(saved->floorTOC, d.FloorDecorationTOC, sizeof(saved->floorTOC));
  std::memcpy(saved->doorTOC, d.DoorDecorationTOC, sizeof(saved->doorTOC));
  std::memcpy(saved->wallGraphics, d.WallDecorationsForLevel, sizeof(saved->wallGraphics));
  std::memcpy(saved->floorGraphics, d.LoadedFloorDecorations, sizeof(saved->floorGraphics));
  saved->doorTOCCharacteristics[0] = d.DoorTOC[0];
  saved->doorTOCCharacteristics[1] = d.DoorTOC[1];
  saved->fountainIndex = d.FountainGraphicIndex[0];
  std::memcpy(saved->alcoveIndices, d.AlcoveGraphicIndices, sizeof(saved->alcoveIndices));
  saved->ornateAlcoveIndex = d.OrnateAlcoveGraphicIndex;
}

static void RestoreLevelTables(const LevelTables *saved)
{
  d.EndOfCELLFLAGS = saved->endOfCellFlags;
  d.NumWallDecoration = saved->numWallDecoration;
  std::memcpy(d.WallDecorationTOC, saved->wallTOC, sizeof(saved->wallTOC));
  std::memcpy(d.FloorDecorationTOC, saved->floorTOC, sizeof(saved->floorTOC));
  std::memcpy(d.DoorDecorationTOC, saved->doorTOC, sizeof(saved->doorTOC));
  std::memcpy(d.WallDecorationsForLevel, saved->wallGraphics, sizeof(saved->wallGraphics));
  std::memcpy(d.LoadedFloorDecorations, saved->floorGraphics, sizeof(saved->floorGraphics));
  d.DoorTOC[0] = saved->doorTOCCharacteristics[0];
  d.DoorTOC[1] = saved->doorTOCCharacteristics[1];
  d.FountainGraphicIndex[0] = saved->fountainIndex;
  std::memcpy(d.AlcoveGraphicIndices, saved->alcoveIndices, sizeof(d.AlcoveGraphicIndices));
  d.OrnateAlcoveGraphicIndex = saved->ornateAlcoveIndex;
}

// The pure part of SetPartyLevel and ReadGraphicsForLevel: which decorations the level uses
// and which pictures they map to. The graphics loading itself is left out.
static void BuildTablesForLoadedLevel(void)
{
  d.EndOfCELLFLAGS = d.LevelCellFlags[d.width - 1] + d.height;
  d.NumWallDecoration = d.pCurLevelDesc->NumWallDecoration();
  i16 monsterTypes = d.pCurLevelDesc->numMonsterType();
  std::memmove(d.WallDecorationTOC, d.EndOfCELLFLAGS + monsterTypes, d.NumWallDecoration);
  d.WallDecorationTOC[d.NumWallDecoration] = 0;
  i16 offset = sw(monsterTypes + d.NumWallDecoration);
  std::memmove(d.FloorDecorationTOC, d.EndOfCELLFLAGS + offset,
               d.pCurLevelDesc->NumFloorDecorations());
  offset = sw(offset + d.pCurLevelDesc->NumFloorDecorations());
  std::memmove(d.DoorDecorationTOC, d.EndOfCELLFLAGS + offset,
               d.pCurLevelDesc->NumDoorDecorations());
  // The alcove and fountain indices as ReadGraphicsForLevel finds them (Code390e.cpp): the engine
  // reads them too, e.g. to strip a random alcove from the rock outside the level
  // (TAG00a4a2), so stale ones from the party's level would put alcoves there
  int alcoves = 0;
  for (int i = 0; i < 3; i++) d.AlcoveGraphicIndices[i] = -1;
  d.FountainGraphicIndex[0] = -1;
  d.OrnateAlcoveGraphicIndex = -1;
  for (int i = 0; i <= d.pCurLevelDesc->NumWallDecoration() && i < LEVEL_FEATURE_TABLE_SIZE; i++)
  {
    i16 decorationIndex = d.WallDecorationTOC[i];
    d.WallDecorationsForLevel[i][0] = sw(121 + 2 * decorationIndex);
    d.WallDecorationsForLevel[i][1] = d.Byte4100[decorationIndex];
    for (int a = 0; a < 3; a++)
    {
      if (decorationIndex != (UI8)d.Byte4038[a]) continue;
      if (alcoves < 3) d.AlcoveGraphicIndices[alcoves++] = sw(i);
      if (decorationIndex == 2) d.OrnateAlcoveGraphicIndex = sw(i);
    }
    if (decorationIndex == d.Word4040[0]) d.FountainGraphicIndex[0] = sw(i);
  }
  for (int i = 0; i < d.pCurLevelDesc->NumFloorDecorations() && i < LEVEL_FEATURE_TABLE_SIZE; i++)
  {
    i16 decorationIndex = (ui8)d.FloorDecorationTOC[i];
    d.LoadedFloorDecorations[i][0] = sw(247 + 6 * decorationIndex);
    d.LoadedFloorDecorations[i][1] = d.Byte4110[decorationIndex];
  }
}

AutoMapLevelSnapshot AutoMapLevelReader_ReadLevel(int level)
{
  if (d.dungeonDatIndex == NULL || d.pLevelDescriptors == NULL) return AutoMapLevelSnapshot();
  if (level < 0 || level >= d.dungeonDatIndex->NumLevel()) return AutoMapLevelSnapshot();
  if (level == d.LoadedLevel && level == d.partyLevel) return AutoMapLevelReader_ReadPartyLevel();
  IllegalDecorationWarningSilencer silencer;
  LevelTables saved;
  SaveLevelTables(&saved);
  i16 savedLoadedLevel = d.LoadedLevel;
  LoadLevel(level);
  BuildTablesForLoadedLevel();
  AutoMapLevelSnapshot snapshot;
  snapshot.level = level;
  snapshot.width = d.width;
  snapshot.height = d.height;
  LevelOffset(snapshot.level, &snapshot.offsetX, &snapshot.offsetY);
  snapshot.party.x = d.partyX;
  snapshot.party.y = d.partyY;
  snapshot.party.facing = (AutoMapDirection)d.partyFacing;
  snapshot.partyIsOnShownLevel = (level == d.partyLevel);
  ColourSchemesOfLoadedLevel(&snapshot.paletteScheme9, &snapshot.paletteScheme10);
  Uint64 otherCellTicks = 0, otherObjectTicks = 0;
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      Uint64 cellStart = SDL_GetPerformanceCounter();
      snapshot.cells.push_back(ReadCell(x, y, snapshot.level));
      otherCellTicks += SDL_GetPerformanceCounter() - cellStart;
      Uint64 objectStart = SDL_GetPerformanceCounter();
      ReadObjectsAt(x, y, &snapshot.cells.back(), &snapshot);
      otherObjectTicks += SDL_GetPerformanceCounter() - objectStart;
    }
  }
  ReportReadSplit(otherCellTicks, otherObjectTicks);
  snapshot.itemOfDescIndex = ItemsOfAllDescriptions();
  // While this level is still loaded: FindFirstObject reads it, not the party level.
  MaybeInspectSnapshot(snapshot);
  ExpandSnapshotWithBoundaryRock(&snapshot);
  MaybeInspectAlcoves(snapshot);
  LoadLevel(savedLoadedLevel);
  RestoreLevelTables(&saved);
  return snapshot;
}
