// AutoMapScene.cpp : turns plain records of a level into the list of things the map window draws.
#include "AutoMapScene.h"
#include "AutoMapSceneInternal.h"
#include "AutoMapCellMemory.h"
#include "AutoMapPlayerNotes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <set>

static const int ZOOM_STEPS_PER_FITTING_SIZE = 4;

static AutoMapWallSide PlainWallSide(void)
{
  AutoMapWallSide side = {AUTOMAP_NO_PICTURE, false, false, false, false, AUTOMAP_NO_PICTURE,
                          std::string(), std::vector<AutoMapRecordId>(), AutoMapWallFeatureNone};
  return side;
}

AutoMapCellRecord::AutoMapCellRecord(void)
  : kind(AutoMapCellWall),
    isOutsideLevel(false),
    spinnerFacing(-1),
    hasTeleporterTarget(false),
    teleporterDestLevel(AUTOMAP_NO_LEVEL),
    teleporterDestX(-1),
    teleporterDestY(-1),
    teleporterDestGlobalX(-1),
    teleporterDestGlobalY(-1),
    teleporterScope(-1),
    teleporterRotation(-1),
    teleporterFacingMode(-1),
    spinnerRotation(-1),
    spinnerIsVisible(false),
    floorPicture(AUTOMAP_NO_PICTURE),
    pitIsHidden(false),
    pitIsOpen(false), pitIsClosedSwitchable(false), falseWallIsPassable(false),
    runsNorthSouth(false),
    doorState(AutoMapDoorOpen),
    doorHasSwitch(false),
    doorBreakableWithAxe(false),
    doorBreakableWithFireball(false),
    doorBreakForce(-1),
    stairsLeadUp(false)
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++) wallSides[side] = PlainWallSide();
}

static AutoMapParty PartyNowhere(void)
{
  AutoMapParty party = {0, 0, AutoMapNorth};
  return party;
}

// The schemes ReadGraphicsForLevel starts from before the monster types of the level
// replace them
const int AUTOMAP_DEFAULT_SCHEME9 = 8;
const int AUTOMAP_DEFAULT_SCHEME10 = 12;

AutoMapLevelSnapshot::AutoMapLevelSnapshot(void)
  : level(AUTOMAP_NO_LEVEL), width(0), height(0), offsetX(0), offsetY(0),
    boundaryMarginX(0), boundaryMarginY(0),
    party(PartyNowhere()),
    partyIsOnShownLevel(true),
    paletteScheme9(AUTOMAP_DEFAULT_SCHEME9), paletteScheme10(AUTOMAP_DEFAULT_SCHEME10)
{
}

AutoMapDrawList::AutoMapDrawList(void)
  : level(AUTOMAP_NO_LEVEL), width(0), height(0), offsetX(0), offsetY(0),
    boundaryMarginX(0), boundaryMarginY(0),
    party(PartyNowhere()),
    partyIsOnShownLevel(true),
    paletteScheme9(AUTOMAP_DEFAULT_SCHEME9), paletteScheme10(AUTOMAP_DEFAULT_SCHEME10),
    spoilers(AutoMapSpoilersAll)
{
}

AutoMapMonsterPicture AutoMapScene_ChooseMonsterPicture(const AutoMapMonsterKind &kind,
                                                        AutoMapDirection facing)
{
  // The engine stores the pictures of a monster type as front, side, back, leaving out the
  // ones the type does not have. Its side picture looks east.
  int sidePicture = kind.frontPicture + 1;
  int backPicture = kind.frontPicture + (kind.hasSidePicture ? 2 : 1);
  AutoMapMonsterPicture choice = {kind.frontPicture, false};
  bool facesSideways = facing == AutoMapEast || facing == AutoMapWest;
  if (facing == AutoMapNorth && kind.hasBackPicture) choice.picture = backPicture;
  if (facesSideways && kind.hasSidePicture)
  {
    choice.picture = sidePicture;
    choice.isMirrored = facing == AutoMapWest;
  }
  return choice;
}

static AutoMapCell CellInFrontOf(const AutoMapCell &wallCell, int side)
{
  static const int STEP_X[AUTOMAP_SIDE_COUNT] = {0, 1, 0, -1};
  static const int STEP_Y[AUTOMAP_SIDE_COUNT] = {-1, 0, 1, 0};
  AutoMapCell neighbour = {wallCell.level, wallCell.x + STEP_X[side], wallCell.y + STEP_Y[side]};
  return neighbour;
}

// Which sides of a wall cell the party has seen. At All a side counts as seen once the floor
// cell in front of it is explored: one explored bit per cell cannot tell which side was seen.
// Below All the 3D view's own record of faces decides, and with the fog off every side counts.
// The 3D view records faces of stone only, so a door keeps the rule of All.
static void SeenSides(const AutoMapCell &wallCell, const AutoMapCellRecord &record,
                      const AutoMapFog &fog, bool seen[AUTOMAP_SIDE_COUNT])
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (!fog.isOn) seen[side] = true;
    else if (fog.spoilers == AutoMapSpoilersAll || record.kind == AutoMapCellDoor)
    {
      seen[side] = fog.explored->IsExplored(CellInFrontOf(wallCell, side));
    }
    else seen[side] = fog.explored->FaceIsSeen(wallCell, side);
  }
}

// Seen from afar, writing is scribbles: the words show once the party has read them. A
// champion's mirror is likewise an empty frame: the face shows once the party stood before it,
// which marks the face read (AutoMap_OnPortraitFaceDrawn). Seen from the side, an alcove shows no
// contents: each item shows once it was noticed in it, all of them once the face was looked into
// as a whole (a clicked portrait, a map from before items were told apart).
// An item in an alcove the 3D view showed so that it was noticed; an item in a chest there goes
// with its chest, whose contents HideItemSecrets keeps hidden until it was opened
static bool AlcoveItemIsNoticed(const AutoMapCell &wallCell, const AutoMapFog &fog,
                                const AutoMapAlcoveEntry &entry)
{
  if (fog.items == NULL) return false;
  AutoMapRecordId drawn = entry.inChest != AUTOMAP_NO_RECORD ? entry.inChest : entry.record;
  if (drawn == AUTOMAP_NO_RECORD) return false;
  std::vector<AutoMapRememberedItem> remembered = fog.items->ItemsOn(wallCell);
  for (size_t i = 0; i < remembered.size(); i++)
  {
    if (remembered[i].record == drawn) return true;
  }
  return false;
}

static void HideTextsNotReadYet(const AutoMapCell &wallCell, const AutoMapFog &fog, AutoMapCellRecord *record)
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (fog.explored->TextIsRead(wallCell, side)) continue;
    record->wallSides[side].text.clear();
    record->wallSides[side].portrait = AUTOMAP_NO_PICTURE;
  }
  std::vector<AutoMapAlcoveEntry> lookedAt;
  for (size_t i = 0; i < record->alcoveContents.size(); i++)
  {
    int side = record->alcoveContents[i].side;
    if (side < 0 || side >= AUTOMAP_SIDE_COUNT || fog.explored->AlcoveIsLookedInto(wallCell, side)
        || AlcoveItemIsNoticed(wallCell, fog, record->alcoveContents[i]))
    {
      lookedAt.push_back(record->alcoveContents[i]);
    }
  }
  record->alcoveContents = lookedAt;
}

// An explored cell only had some strip of it on screen: its floor picture (a puddle, moss, a
// pad) and its stairs show once the 3D view showed enough of them; until then stairs read as
// floor. A pit's picture is the cell itself and stays.
static void HideFloorPictureNotSeenYet(const AutoMapCell &floorCell, const AutoMapFog &fog,
                                       AutoMapCellRecord *record)
{
  if (record->kind == AutoMapCellPit) return;
  if (fog.explored->FloorIsSeen(floorCell)) return;
  record->floorPicture = AUTOMAP_NO_PICTURE;
  if (record->kind == AutoMapCellStairs) record->kind = AutoMapCellFloor;
}

// Whole level shows what the party would see walking past every face. The 3D view draws the
// goods in a wall only on an alcove face (Viewport.cpp, the Alcove step), so coins in a slot
// stay hidden until the slot turns into an alcove (CSB L10 (36,36)). Goods the party looked
// into all the same, like a portrait's gear, stay.
static void HideGoodsNotDrawn(const AutoMapCell &wallCell, const AutoMapFog &fog,
                              AutoMapCellRecord *record)
{
  std::vector<AutoMapAlcoveEntry> drawn;
  for (size_t i = 0; i < record->alcoveContents.size(); i++)
  {
    int side = record->alcoveContents[i].side;
    if (side < 0 || side >= AUTOMAP_SIDE_COUNT || record->wallSides[side].isAlcove
        || fog.explored->AlcoveIsLookedInto(wallCell, side))
    {
      drawn.push_back(record->alcoveContents[i]);
    }
  }
  record->alcoveContents = drawn;
}

static void HideWallSidesNotSeenYet(const bool seen[AUTOMAP_SIDE_COUNT], AutoMapCellRecord *record)
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (!seen[side]) record->wallSides[side] = PlainWallSide();
  }
  // Alcove contents and lock requirements hide with their side, so hovering near
  // secrets leaks nothing. Entries with no valid side survive: there is no side to hide.
  std::vector<AutoMapAlcoveEntry> seenAlcove;
  for (size_t i = 0; i < record->alcoveContents.size(); i++)
  {
    int side = record->alcoveContents[i].side;
    if (side < 0 || side >= AUTOMAP_SIDE_COUNT || seen[side])
    {
      seenAlcove.push_back(record->alcoveContents[i]);
    }
  }
  record->alcoveContents = seenAlcove;
  std::vector<AutoMapWallLock> seenLocks;
  for (size_t i = 0; i < record->wallLocks.size(); i++)
  {
    int side = record->wallLocks[i].side;
    if (side < 0 || side >= AUTOMAP_SIDE_COUNT || seen[side])
    {
      seenLocks.push_back(record->wallLocks[i]);
    }
  }
  record->wallLocks = seenLocks;
}

static AutoMapShownCell ShowCell(const AutoMapCell &position, const AutoMapCellRecord &record)
{
  AutoMapShownCell shown;
  shown.x = position.x;
  shown.y = position.y;
  shown.cell = record;
  shown.allItemPictures = record.itemPictures;
  shown.hasMoreItems = (int)record.itemPictures.size() > AUTOMAP_MOST_ITEMS_SHOWN;
  shown.isHighlighted = false;
  shown.teleporterTargetVisible = false;
  shown.playerPit = -1;
  if (shown.hasMoreItems) shown.cell.itemPictures.resize(AUTOMAP_MOST_ITEMS_SHOWN);
  return shown;
}

bool AutoMapScene_FloorPictureIsFlavour(int floorPicture)
{
  static const int FIRST_ORNAMENT_PICTURE = 247;
  static const int VIEWS_PER_ORNAMENT = 6;
  static const int NEAREST_VIEW = 5;
  static const int STANDARD_ORNAMENTS = 9;
  int offset = floorPicture - FIRST_ORNAMENT_PICTURE - NEAREST_VIEW;
  if (offset < 0 || offset % VIEWS_PER_ORNAMENT != 0) return false;
  int ornament = offset / VIEWS_PER_ORNAMENT;
  if (ornament >= STANDARD_ORNAMENTS) return false;
  return ornament != 1 && ornament != 4 && ornament != 7;
}

AutoMapItemKind AutoMapScene_KindOfItem(const AutoMapItem &item)
{
  static const int FIRST_POTION = 2;
  static const int FIRST_WEAPON = 23;
  static const int FIRST_CLOTHING = 69;
  static const int FIRST_MISC = 127;
  // Name indices, Objects.h: objNI_Apple..objNI_DragonSteak, objNI_FirstKey..objNI_LastKey
  static const int FIRST_FOOD_NAME = 168;
  static const int LAST_FOOD_NAME = 175;
  static const int FIRST_KEY_NAME = 176;
  static const int LAST_KEY_NAME = 191;
  int type = item.descIndex;
  if (type < 0 || type >= FIRST_MISC)
  {
    if (item.nameIndex >= FIRST_FOOD_NAME && item.nameIndex <= LAST_FOOD_NAME) return AutoMapItemFood;
    if (item.nameIndex >= FIRST_KEY_NAME && item.nameIndex <= LAST_KEY_NAME) return AutoMapItemKey;
    return AutoMapItemOther;
  }
  if (type >= FIRST_CLOTHING) return AutoMapItemArmour;
  if (type >= FIRST_WEAPON) return AutoMapItemWeapon;
  if (type >= FIRST_POTION) return AutoMapItemPotion;
  return type == 0 ? AutoMapItemScroll : AutoMapItemChest;
}

std::vector<AutoMapItem> AutoMapScene_ItemsOfKinds(const std::vector<AutoMapItem> &items,
                                                   unsigned kinds)
{
  std::vector<AutoMapItem> kept;
  for (size_t i = 0; i < items.size(); i++)
  {
    if (kinds & (1u << AutoMapScene_KindOfItem(items[i]))) kept.push_back(items[i]);
  }
  return kept;
}

std::vector<AutoMapItemStack> AutoMapScene_GroupItems(const std::vector<AutoMapItem> &items)
{
  std::vector<AutoMapItemStack> stacks;
  for (size_t i = 0; i < items.size(); i++)
  {
    bool counted = false;
    for (size_t s = 0; s < stacks.size(); s++)
    {
      if (stacks[s].item == items[i])
      {
        stacks[s].count++;
        counted = true;
        break;
      }
    }
    if (!counted)
    {
      AutoMapItemStack stack;
      stack.item = items[i];
      stack.count = 1;
      stacks.push_back(stack);
    }
  }
  return stacks;
}

// The fog remembers the level's own cells, but a snapshot grown by the reader's boundary
// margin holds the level shifted inward by that margin. Map a snapshot cell back before
// asking the fog; boundary rock lands outside the level, where the fog says unseen.
static AutoMapCell LevelCellOf(const AutoMapLevelSnapshot &snapshot, int x, int y)
{
  AutoMapCell cell = {snapshot.level, x - snapshot.boundaryMarginX, y - snapshot.boundaryMarginY};
  return cell;
}

// Under fog every cell shown is an explored one: a highlight would mark nothing
static bool IsHighlighted(const AutoMapCell &position, const AutoMapCellRecord &record,
                          const AutoMapFog &fog)
{
  if (fog.isOn || !fog.highlightsExplored) return false;
  // The highlight marks walked floor: wall-like cells stay dark, whatever their kind
  if (record.kind == AutoMapCellWall || record.kind == AutoMapCellFalseWall) return false;
  return fog.explored->IsExplored(position);
}

// Whether the party has found this out. Seen never reads the knowledge, so it may be NULL there.
static bool Known(const AutoMapFog &fog, const AutoMapFact &fact)
{
  return fog.spoilers == AutoMapSpoilersLearned && fog.knowledge != NULL
      && fog.knowledge->Knows(fact);
}

static bool AnyActuatorKnown(const AutoMapFog &fog, const std::vector<AutoMapRecordId> &actuators)
{
  for (size_t i = 0; i < actuators.size(); i++)
  {
    if (Known(fog, AutoMapFact_OfRecord(AutoMapFactActuator, actuators[i]))) return true;
  }
  return false;
}

static void HideWallSecrets(const AutoMapFog &fog, AutoMapCellRecord *record)
{
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    AutoMapWallSide &wall = record->wallSides[side];
    // An actuator nobody has fired looks like dressing; a launcher shows only once it has fired
    wall.functional = wall.functional && AnyActuatorKnown(fog, wall.actuators);
    bool launcherKnown = false;
    for (size_t i = 0; i < record->wallLaunchers.size(); i++)
    {
      const AutoMapLauncher &launcher = record->wallLaunchers[i];
      if (launcher.side == side
          && Known(fog, AutoMapFact_OfRecord(AutoMapFactActuator, launcher.actuator)))
      {
        launcherKnown = true;
      }
    }
    wall.hidden = wall.hidden && launcherKnown;
  }
  std::vector<AutoMapWallLock> knownLocks;
  for (size_t i = 0; i < record->wallLocks.size(); i++)
  {
    if (Known(fog, AutoMapFact_OfRecord(AutoMapFactLockRequirement, record->wallLocks[i].actuator)))
    {
      knownLocks.push_back(record->wallLocks[i]);
    }
  }
  record->wallLocks = knownLocks;
  std::vector<AutoMapLauncher> knownLaunchers;
  for (size_t i = 0; i < record->wallLaunchers.size(); i++)
  {
    if (Known(fog, AutoMapFact_OfRecord(AutoMapFactActuator, record->wallLaunchers[i].actuator)))
    {
      knownLaunchers.push_back(record->wallLaunchers[i]);
    }
  }
  record->wallLaunchers = knownLaunchers;
  // Relay machinery is never shown below All: nothing the party did tells what it counts
  record->wallRelays.clear();
}

static void HideTeleporterSecrets(const AutoMapCell &levelCell, const AutoMapFog &fog,
                                  AutoMapCellRecord *record)
{
  AutoMapCellKind kind = record->kind;
  bool dormant = kind == AutoMapCellDormantTeleporter || kind == AutoMapCellDormantSpinner;
  bool isTeleporter = kind == AutoMapCellTeleporter || kind == AutoMapCellInvisibleTeleporter
      || kind == AutoMapCellSpinner || dormant;
  if (!isTeleporter) return;
  // A dormant one fires nothing, so there is nothing to have used
  bool used = !dormant && Known(fog, AutoMapFact_OfCell(AutoMapFactTeleporter, levelCell));
  // A visible teleporter stays on the map, and so does a visible spinner, drawn as the
  // teleporter it looks like; the others look like floor until they were used
  if (!used && kind != AutoMapCellTeleporter)
  {
    record->kind = record->spinnerIsVisible ? AutoMapCellTeleporter : AutoMapCellFloor;
  }
  // No TAKES line, whatever the party knows
  record->teleporterScope = -1;
  if (used) return;
  record->hasTeleporterTarget = false;
  record->teleporterRotation = -1;
  record->teleporterFacingMode = -1;
  record->spinnerRotation = -1;
  record->spinnerFacing = -1;
}

static bool ScrollRead(const AutoMapFog &fog, AutoMapRecordId scroll)
{
  return scroll != AUTOMAP_NO_RECORD
      && Known(fog, AutoMapFact_OfRecord(AutoMapFactScrollRead, scroll));
}

// Chests show their contents once opened, scrolls their text once read. A chest not opened is
// still a chest on the floor or in its alcove; the cell's own item list stays.
static void HideItemSecrets(const AutoMapFog &fog, AutoMapCellRecord *record)
{
  std::vector<AutoMapChestContents> openedChests;
  for (size_t i = 0; i < record->chestContents.size(); i++)
  {
    AutoMapChestContents contents = record->chestContents[i];
    if (!Known(fog, AutoMapFact_OfRecord(AutoMapFactChestOpened, contents.chest))) continue;
    for (size_t p = 0; p < contents.pictureTexts.size(); p++)
    {
      AutoMapRecordId scroll = p < contents.pictureScrolls.size() ? contents.pictureScrolls[p]
                                                                  : AUTOMAP_NO_RECORD;
      if (!ScrollRead(fog, scroll)) contents.pictureTexts[p].clear();
    }
    openedChests.push_back(contents);
  }
  record->chestContents = openedChests;
  std::vector<AutoMapAlcoveEntry> shownAlcove;
  for (size_t i = 0; i < record->alcoveContents.size(); i++)
  {
    AutoMapAlcoveEntry entry = record->alcoveContents[i];
    if (entry.inChest != AUTOMAP_NO_RECORD
        && !Known(fog, AutoMapFact_OfRecord(AutoMapFactChestOpened, entry.inChest))) continue;
    if (!ScrollRead(fog, entry.scroll)) entry.text.clear();
    shownAlcove.push_back(entry);
  }
  record->alcoveContents = shownAlcove;
  std::vector<std::string> readTexts;
  std::vector<AutoMapRecordId> readRecords;
  for (size_t i = 0; i < record->scrollTexts.size(); i++)
  {
    AutoMapRecordId scroll = i < record->scrollRecords.size() ? record->scrollRecords[i]
                                                              : AUTOMAP_NO_RECORD;
    if (!ScrollRead(fog, scroll)) continue;
    readTexts.push_back(record->scrollTexts[i]);
    readRecords.push_back(scroll);
  }
  record->scrollTexts = readTexts;
  record->scrollRecords = readRecords;
}

// What a cell gives away that the party has not found out, below All; see
// AutoMapScene_BuildDrawList. levelCell is in level coordinates, like the facts.
static void HideCellSecrets(const AutoMapCell &levelCell, const AutoMapFog &fog,
                            AutoMapCellRecord *record)
{
  HideWallSecrets(fog, record);
  // A closed switchable pit is floor to the party until it opens
  record->pitIsClosedSwitchable = false;
  record->falseWallIsPassable = false;
  // Whether a door can be broken is only found out by breaking it
  record->doorBreakableWithAxe = false;
  record->doorBreakableWithFireball = false;
  record->doorBreakForce = -1;
  if (record->kind == AutoMapCellFalseWall
      && !Known(fog, AutoMapFact_OfCell(AutoMapFactFalseWall, levelCell)))
  {
    record->kind = AutoMapCellWall;
  }
  else if (record->kind == AutoMapCellOpenFalseWall
           && !Known(fog, AutoMapFact_OfCell(AutoMapFactFalseWall, levelCell)))
  {
    record->kind = AutoMapCellFloor;
  }
  // A concealed pit reads as floor in the 3D view whether open or closed, so until the party
  // knows it the map draws floor either way: its open state was never seen
  if (record->kind == AutoMapCellPit && record->pitIsHidden
      && !Known(fog, AutoMapFact_OfCell(AutoMapFactPit, levelCell)))
  {
    record->kind = AutoMapCellFloor;
    record->floorPicture = AUTOMAP_NO_PICTURE;
  }
  HideTeleporterSecrets(levelCell, fog, record);
  HideItemSecrets(fog, record);
}

// Below All with the fog on, a cell shows the items given, not the live pile: what the party
// remembers on it, or with Explored, live the items of the pile it has seen. An item still in the pile (same record and description) keeps its live picture,
// chest and scroll text; one that left it is drawn from its description. Chests and scrolls then
// face the same facts as ever in HideItemSecrets.
static void ShowItems(const std::vector<AutoMapRememberedItem> &remembered,
                      const AutoMapLevelSnapshot &snapshot, const AutoMapCellRecord &live,
                      AutoMapCellRecord *record)
{
  record->itemPictures.clear();
  record->itemTags.clear();
  record->chestContents.clear();
  record->scrollTexts.clear();
  record->scrollRecords.clear();
  for (size_t r = 0; r < remembered.size(); r++)
  {
    const AutoMapRememberedItem &item = remembered[r];
    // A glimpse shows as a "?" of its own, not as the item
    if (item.isGlimpsed) continue;
    size_t pile = live.itemTags.size();
    for (size_t i = 0; i < live.itemTags.size() && i < live.itemPictures.size(); i++)
    {
      if (live.itemTags[i].record == item.record && live.itemTags[i].descIndex == item.descIndex)
      {
        pile = i;
        break;
      }
    }
    AutoMapItem picture;
    if (pile < live.itemTags.size())
    {
      picture = live.itemPictures[pile];
    }
    else
    {
      std::map<int, AutoMapItem>::const_iterator found = snapshot.itemOfDescIndex.find(item.descIndex);
      if (found == snapshot.itemOfDescIndex.end()) continue;
      picture = found->second;
    }
    AutoMapFloorItemTag tag = {item.record, item.descIndex};
    record->itemPictures.push_back(picture);
    record->itemTags.push_back(tag);
    if (pile >= live.itemTags.size()) continue;
    for (size_t c = 0; c < live.chestContents.size(); c++)
    {
      if (live.chestContents[c].item != pile) continue;
      AutoMapChestContents contents = live.chestContents[c];
      contents.item = record->itemPictures.size() - 1;
      record->chestContents.push_back(contents);
    }
    for (size_t t = 0; t < live.scrollRecords.size() && t < live.scrollTexts.size(); t++)
    {
      if (live.scrollRecords[t] != item.record) continue;
      record->scrollTexts.push_back(live.scrollTexts[t]);
      record->scrollRecords.push_back(live.scrollRecords[t]);
    }
  }
}

static void RememberItems(const AutoMapCell &levelCell, const AutoMapLevelSnapshot &snapshot,
                          const AutoMapItemMemory &memory, const AutoMapCellRecord &live,
                          AutoMapCellRecord *record)
{
  // Items noticed in an alcove are remembered on its wall cell too: they are no floor items, nor
  // is one that left the alcove unseen. Stone holds no floor items.
  if (live.kind == AutoMapCellWall)
  {
    ShowItems(std::vector<AutoMapRememberedItem>(), snapshot, live, record);
    return;
  }
  std::vector<AutoMapRememberedItem> onTheFloor;
  std::vector<AutoMapRememberedItem> remembered = memory.ItemsOn(levelCell);
  for (size_t r = 0; r < remembered.size(); r++)
  {
    bool isInAnAlcove = false;
    for (size_t a = 0; a < live.alcoveContents.size() && !isInAnAlcove; a++)
    {
      isInAnAlcove = live.alcoveContents[a].record == remembered[r].record;
    }
    if (!isInAnAlcove) onTheFloor.push_back(remembered[r]);
  }
  ShowItems(onTheFloor, snapshot, live, record);
}

// Explored, live: the live pile, but only the items the 3D view has shown the party somewhere.
// A key that lay under the ashes all along stays off the map until it is seen.
static void ShowSeenLiveItems(const AutoMapCell &levelCell, const AutoMapLevelSnapshot &snapshot,
                              const AutoMapItemMemory &memory, const AutoMapCellRecord &live,
                              AutoMapCellRecord *record)
{
  std::vector<AutoMapRememberedItem> seen;
  for (size_t i = 0; i < live.itemTags.size(); i++)
  {
    if (!memory.HasSeen(live.itemTags[i])) continue;
    AutoMapRememberedItem item = {live.itemTags[i].record, live.itemTags[i].descIndex, levelCell};
    seen.push_back(item);
  }
  ShowItems(seen, snapshot, live, record);
}

static bool HasGlimpseOn(const AutoMapItemMemory &memory, const AutoMapCell &levelCell)
{
  std::vector<AutoMapRememberedItem> remembered = memory.ItemsOn(levelCell);
  for (size_t r = 0; r < remembered.size(); r++)
  {
    if (remembered[r].isGlimpsed) return true;
  }
  return false;
}

// The rock the reader adds around a level is explored once the 3D view painted it
// (AutoMapExplored keeps the rim). At All it also shows, though not painted, beside explored
// floor, so the map does not end in a ragged edge; next to stone only there is nothing worth
// seeing.
static bool RimRockIsSeen(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog, int x, int y)
{
  if (fog.spoilers != AutoMapSpoilersAll) return false;
  static const int stepX[4] = {0, 1, 0, -1};
  static const int stepY[4] = {-1, 0, 1, 0};
  for (int d = 0; d < 4; d++)
  {
    int nx = x + stepX[d];
    int ny = y + stepY[d];
    if (nx < 0 || ny < 0 || nx >= snapshot.width || ny >= snapshot.height) continue;
    const AutoMapCellRecord &beside = snapshot.cells[(size_t)ny * snapshot.width + nx];
    if (beside.isOutsideLevel || beside.kind == AutoMapCellWall) continue;
    if (fog.explored->IsExplored(LevelCellOf(snapshot, nx, ny))) return true;
  }
  return false;
}

static void AddCells(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                     AutoMapDrawList *drawList)
{
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      AutoMapCell levelCell = LevelCellOf(snapshot, x, y);
      AutoMapCell position = {snapshot.level, x, y};
      bool isLive = fog.isOn && fog.isLive;
      // Explored, live draws only items seen in the light, never a glimpsed one, so its "?"
      // stays as under the fog of war
      if (fog.isOn && fog.items != NULL && fog.spoilers != AutoMapSpoilersAll
          && HasGlimpseOn(*fog.items, levelCell))
      {
        drawList->glimpsedItems.push_back(position);
      }
      for (int side = 0; fog.isOn && fog.spoilers != AutoMapSpoilersAll && side < AUTOMAP_SIDE_COUNT; side++)
      {
        if (!fog.explored->FaceIsGlimpsed(levelCell, side)) continue;
        AutoMapGlimpsedFace face = {x, y, side};
        drawList->glimpsedFaces.push_back(face);
      }
      if (fog.isOn && !fog.explored->IsExplored(levelCell))
      {
        bool isRim = snapshot.cells[(size_t)y * snapshot.width + x].isOutsideLevel;
        if (!isRim || !RimRockIsSeen(snapshot, fog, x, y)) continue;
      }
      AutoMapShownCell shown = ShowCell(position, snapshot.cells[(size_t)y * snapshot.width + x]);
      // The remembered look goes first, so every rule below works on what the party saw: faces
      // not seen yet are still blanked and the secret rules still apply. Applied after
      // HideWallSidesNotSeenYet it would bring back the pictures of unseen faces.
      if (fog.isOn && !isLive && fog.cells != NULL && fog.spoilers != AutoMapSpoilersAll)
      {
        AutoMapCellLook look;
        if (fog.cells->LookOf(levelCell, &look)) AutoMapScene_ApplyLook(look, &shown.cell);
      }
      bool seen[AUTOMAP_SIDE_COUNT];
      SeenSides(levelCell, shown.cell, fog, seen);
      if (fog.isOn) HideWallSidesNotSeenYet(seen, &shown.cell);
      if (fog.isOn && fog.spoilers != AutoMapSpoilersAll) HideTextsNotReadYet(levelCell, fog, &shown.cell);
      if (fog.isOn && fog.spoilers != AutoMapSpoilersAll) HideFloorPictureNotSeenYet(levelCell, fog, &shown.cell);
      if (!fog.isOn && fog.spoilers != AutoMapSpoilersAll) HideGoodsNotDrawn(levelCell, fog, &shown.cell);
      if (fog.isOn && fog.items != NULL && fog.spoilers != AutoMapSpoilersAll)
      {
        const AutoMapCellRecord &live = snapshot.cells[(size_t)y * snapshot.width + x];
        if (isLive) ShowSeenLiveItems(levelCell, snapshot, *fog.items, live, &shown.cell);
        else RememberItems(levelCell, snapshot, *fog.items, live, &shown.cell);
        // ShowCell truncated the live pile; the remembered one is cut the same way
        shown.allItemPictures = shown.cell.itemPictures;
        shown.hasMoreItems = (int)shown.cell.itemPictures.size() > AUTOMAP_MOST_ITEMS_SHOWN;
        if (shown.hasMoreItems) shown.cell.itemPictures.resize(AUTOMAP_MOST_ITEMS_SHOWN);
      }
      if (fog.spoilers != AutoMapSpoilersAll) HideCellSecrets(levelCell, fog, &shown.cell);
      shown.isHighlighted = IsHighlighted(levelCell, shown.cell, fog);
      if (fog.notes != NULL) shown.playerPit = fog.notes->PitAt(levelCell);
      drawList->cells.push_back(shown);
    }
  }
}

void AutoMapScene_RememberExploredItems(const AutoMapLevelSnapshot &snapshot,
                                        const AutoMapExplored &explored, AutoMapItemMemory *memory)
{
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      AutoMapCell levelCell = LevelCellOf(snapshot, x, y);
      if (!explored.IsExplored(levelCell)) continue;
      const AutoMapCellRecord &record = snapshot.cells[(size_t)y * snapshot.width + x];
      for (size_t i = 0; i < record.itemTags.size(); i++)
      {
        AutoMapRememberedItem item = {record.itemTags[i].record, record.itemTags[i].descIndex,
                                      levelCell};
        memory->See(item);
      }
    }
  }
}

static AutoMapShownMonster ShowMonster(const AutoMapMonsterRecord &monster,
                                       const AutoMapFog &fog)
{
  AutoMapShownMonster shown;
  shown.x = monster.x;
  shown.y = monster.y;
  shown.transparentColour = monster.kind.transparentColour;
  shown.memberCount = std::min(monster.memberCount, AUTOMAP_LARGEST_GROUP);
  for (int member = 0; member < AUTOMAP_LARGEST_GROUP; member++)
  {
    shown.memberFacings[member] = monster.memberFacings[member];
    shown.memberPictures[member] =
        AutoMapScene_ChooseMonsterPicture(monster.kind, monster.memberFacings[member]);
  }
  // What a monster carries, and what it would drop, is found out by killing it, not by looking
  if (fog.spoilers == AutoMapSpoilersAll)
  {
    shown.possessionPictures = monster.possessionPictures;
    shown.possessionTexts = monster.possessionTexts;
    shown.residuePictures = monster.residuePictures;
    shown.chanceResiduePictures = monster.chanceResiduePictures;
  }
  // The player never sees a monster's hit points, only the damage the game shows for a hit
  if (fog.spoilers == AutoMapSpoilersAll)
  {
    shown.memberHealth = monster.memberHealth;
    if ((int)shown.memberHealth.size() > shown.memberCount)
    {
      shown.memberHealth.resize((size_t)shown.memberCount);
    }
  }
  else
  {
    for (int member = 0; member < shown.memberCount; member++)
    {
      shown.memberDamage.push_back(fog.damage == NULL ? 0 : fog.damage->DealtTo(monster.record, member));
    }
  }
  return shown;
}

// Below All the 3D view's own measurement decides, when the fog was given one: a group half
// behind a corner shows while a member is on screen, a group in the ray but black in the dark
// does not. At All, or without a measurement, the ray decides.
static bool MonsterIsSeen(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                          const AutoMapMonsterRecord &monster)
{
  if (fog.spoilers != AutoMapSpoilersAll && fog.monstersSeenNow != NULL)
  {
    return std::find(fog.monstersSeenNow->begin(), fog.monstersSeenNow->end(), monster.record) !=
           fog.monstersSeenNow->end();
  }
  return fog.visibleNow->Contains(monster.x - snapshot.boundaryMarginX,
                                  monster.y - snapshot.boundaryMarginY);
}

const AutoMapShownCell *FindShownCell(const AutoMapDrawList &drawList, int x, int y);

// Explored, live shows what moves on every explored cell, wherever the party is. A group may
// stand in a closed false wall: below All the cells are already in the draw list, and one drawn
// as stone hides what is in it, or the secret would show. Snapshot coordinates.
static bool ShowsLiveAt(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                        const AutoMapDrawList &drawList, int x, int y)
{
  if (!fog.explored->IsExplored(LevelCellOf(snapshot, x, y))) return false;
  if (fog.spoilers == AutoMapSpoilersAll) return true;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  return shown == NULL || shown->cell.kind != AutoMapCellWall;
}

static void AddMonsters(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                        AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < snapshot.monsters.size(); i++)
  {
    const AutoMapMonsterRecord &monster = snapshot.monsters[i];
    if (fog.isOn && fog.isLive)
    {
      if (!ShowsLiveAt(snapshot, fog, *drawList, monster.x, monster.y)) continue;
    }
    else if (fog.isOn && !MonsterIsSeen(snapshot, fog, monster)) continue;
    drawList->monsters.push_back(ShowMonster(monster, fog));
  }
}

// Missiles and clouds come and go with the fight: like monsters they only show on cells
// the party sees right now while the fog is on, or on every explored cell with Explored, live
static void AddMissiles(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                        AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < snapshot.missiles.size(); i++)
  {
    const AutoMapMissileRecord &missile = snapshot.missiles[i];
    if (fog.isOn && fog.isLive)
    {
      if (!ShowsLiveAt(snapshot, fog, *drawList, missile.x, missile.y)) continue;
    }
    else if (fog.isOn && !fog.visibleNow->Contains(missile.x - snapshot.boundaryMarginX,
                                                   missile.y - snapshot.boundaryMarginY)) continue;
    AutoMapShownMissile shown;
    shown.x = missile.x;
    shown.y = missile.y;
    shown.item = missile.item;
    shown.rangeRemaining = missile.rangeRemaining;
    shown.sizeShowsRange = missile.sizeShowsRange;
    drawList->missiles.push_back(shown);
  }
}

static void AddClouds(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                      AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < snapshot.clouds.size(); i++)
  {
    const AutoMapCloudRecord &cloud = snapshot.clouds[i];
    if (fog.isOn && fog.isLive)
    {
      if (!ShowsLiveAt(snapshot, fog, *drawList, cloud.x, cloud.y)) continue;
    }
    else if (fog.isOn && !fog.visibleNow->Contains(cloud.x - snapshot.boundaryMarginX,
                                                   cloud.y - snapshot.boundaryMarginY)) continue;
    AutoMapShownCloud shown;
    shown.x = cloud.x;
    shown.y = cloud.y;
    shown.picture = cloud.picture;
    drawList->clouds.push_back(shown);
  }
}

// A pad the party has set off is on the map, but what it wants is not: the party only knows
// that stepping there did something. The monster a generator made was seen, so it stays.
static AutoMapFloorActuator HidePadSecrets(const AutoMapFloorActuator &actuator)
{
  AutoMapFloorActuator shown = actuator;
  if (shown.kind == AutoMapActuatorSpentGenerator) shown.kind = AutoMapActuatorGenerator;
  shown.op = AutoMapPadAnything;
  shown.object = AutoMapItem();
  shown.requiredFacing = -1;
  shown.hasGeneratorHealth = false;
  shown.generatedCount = -1;
  shown.counterValue = -1;
  return shown;
}

static void AddActuators(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                         AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < snapshot.actuators.size(); i++)
  {
    const AutoMapFloorActuator &actuator = snapshot.actuators[i];
    AutoMapCell levelCell = LevelCellOf(snapshot, actuator.x, actuator.y);
    if (fog.spoilers == AutoMapSpoilersAll)
    {
      // An actuator does not move, so it stays on every explored cell, as below All
      if (fog.isOn && !fog.explored->IsExplored(levelCell)) continue;
      drawList->actuators.push_back(actuator);
      continue;
    }
    if (actuator.kind == AutoMapActuatorCog || actuator.kind == AutoMapActuatorDormantPad)
    {
      continue;
    }
    if (!Known(fog, AutoMapFact_OfRecord(AutoMapFactActuator, actuator.record))) continue;
    // A learned actuator does not move: like the walls and doors it stays on every explored
    // cell, so its links do not vanish when the party looks away
    if (fog.isOn && !fog.explored->IsExplored(levelCell)) continue;
    drawList->actuators.push_back(HidePadSecrets(actuator));
  }
}

const AutoMapShownCell *FindShownCell(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    if (drawList.cells[i].x == x && drawList.cells[i].y == y) return &drawList.cells[i];
  }
  return NULL;
}

static bool FloorSourceIsShown(const AutoMapDrawList &drawList, int x, int y)
{
  for (size_t i = 0; i < drawList.actuators.size(); i++)
  {
    if (drawList.actuators[i].x == x && drawList.actuators[i].y == y) return true;
  }
  return false;
}

static bool WallSourceIsShown(const AutoMapDrawList &drawList, const AutoMapLink &link)
{
  const AutoMapShownCell *shown = FindShownCell(drawList, link.srcX, link.srcY);
  if (shown == NULL) return false;
  if (link.srcSide < 0 || link.srcSide >= AUTOMAP_SIDE_COUNT) return false;
  const AutoMapWallSide &side = shown->cell.wallSides[link.srcSide];
  return side.picture != AUTOMAP_NO_PICTURE || side.hasWriting || side.functional || side.hidden
      || side.portrait != AUTOMAP_NO_PICTURE;
}

// Both ends must have survived the cell filter; floor sources must also have survived the
// actuator filter (no arrow from a pad the fog hides), wall sources must still carry their
// side after the wall-side filter. Otherwise hovering near secrets would leak the dungeon.
// Same-level targets show from an explored cell; cross-level ones only when explored
// themselves (or with the fog off). Anything else would leak teleporter destinations,
// invisibly-teleporting victims included.
static void AddTeleporterVisibility(const AutoMapFog &fog, AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < drawList->cells.size(); i++)
  {
    AutoMapShownCell &shown = drawList->cells[i];
    if (!shown.cell.hasTeleporterTarget) continue;
    int dstLevel = shown.cell.teleporterDestLevel;
    int dstX = shown.cell.teleporterDestX;
    int dstY = shown.cell.teleporterDestY;
    if (dstLevel == drawList->level)
    {
      shown.teleporterTargetVisible = FindShownCell(*drawList, dstX, dstY) != NULL;
    }
    else if (!fog.isOn)
    {
      shown.teleporterTargetVisible = true;
    }
    else
    {
      AutoMapCell dst = {dstLevel, dstX, dstY};
      shown.teleporterTargetVisible = fog.explored->IsExplored(dst);
    }
  }
}

// Below All the party knows a link only from watching a cell change after an actuator fired:
// one arrow from that actuator to that cell, whatever relays the engine ran in between. The
// arrow starts where the snapshot's own link from that actuator starts and carries nothing else.
// A fact whose actuator is gone (a once-only one, deleted) adds nothing.
static std::vector<AutoMapLink> KnownLinks(const AutoMapLevelSnapshot &snapshot,
                                           const AutoMapFog &fog)
{
  std::vector<AutoMapLink> known;
  if (fog.spoilers != AutoMapSpoilersLearned || fog.knowledge == NULL) return known;
  std::vector<AutoMapFact> facts = fog.knowledge->Facts();
  for (size_t f = 0; f < facts.size(); f++)
  {
    if (facts[f].kind != AutoMapFactLink || facts[f].cell.level != snapshot.level) continue;
    for (size_t i = 0; i < snapshot.links.size(); i++)
    {
      const AutoMapLink &from = snapshot.links[i];
      if (from.source != facts[f].record) continue;
      AutoMapLink link(from.srcX, from.srcY, facts[f].cell.x + snapshot.boundaryMarginX,
                       facts[f].cell.y + snapshot.boundaryMarginY, from.srcIsWall, from.srcSide);
      link.source = from.source;
      known.push_back(link);
      break;
    }
  }
  return known;
}

static void AddLinks(const std::vector<AutoMapLink> &links, AutoMapDrawList *drawList)
{
  for (size_t i = 0; i < links.size(); i++)
  {
    const AutoMapLink &link = links[i];
    if (FindShownCell(*drawList, link.srcX, link.srcY) == NULL) continue;
    if (FindShownCell(*drawList, link.dstX, link.dstY) == NULL) continue;
    if (link.srcIsWall)
    {
      if (!WallSourceIsShown(*drawList, link)) continue;
    }
    else
    {
      if (!FloorSourceIsShown(*drawList, link.srcX, link.srcY)) continue;
    }
    drawList->links.push_back(link);
  }
}

// The player's links, kept apart from the learned ones. Both ends must be shown, so a note never
// names a cell the map keeps hidden. Nothing is checked against the dungeon.
static void AddPlayerLinks(const AutoMapLevelSnapshot &snapshot, const AutoMapFog &fog,
                           AutoMapDrawList *drawList)
{
  if (fog.notes == NULL) return;
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > all = fog.notes->LinksOfLevel(snapshot.level);
  for (size_t i = 0; i < all.size(); i++)
  {
    int srcX = all[i].first.cell.x + snapshot.boundaryMarginX;
    int srcY = all[i].first.cell.y + snapshot.boundaryMarginY;
    int dstX = all[i].second.x + snapshot.boundaryMarginX;
    int dstY = all[i].second.y + snapshot.boundaryMarginY;
    if (FindShownCell(*drawList, srcX, srcY) == NULL) continue;
    if (FindShownCell(*drawList, dstX, dstY) == NULL) continue;
    int side = all[i].first.side;
    drawList->playerLinks.push_back(AutoMapLink(srcX, srcY, dstX, dstY, side >= 0, side));
  }
}

AutoMapCellLook AutoMapScene_LookOf(const AutoMapCellRecord &record)
{
  AutoMapCellLook look;
  look.kind = record.kind;
  look.doorState = record.doorState;
  look.pitIsOpen = record.pitIsOpen;
  look.pitIsHidden = record.pitIsHidden;
  look.spinnerIsVisible = record.spinnerIsVisible;
  look.floorPicture = record.floorPicture;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    look.sidePictures[side] = record.wallSides[side].picture;
  }
  return look;
}

bool AutoMapScene_LooksDiffer(const AutoMapCellLook &a, const AutoMapCellLook &b)
{
  if (a.kind != b.kind || a.doorState != b.doorState) return true;
  if (a.pitIsOpen != b.pitIsOpen || a.pitIsHidden != b.pitIsHidden) return true;
  if (a.spinnerIsVisible != b.spinnerIsVisible) return true;
  if (a.floorPicture != b.floorPicture) return true;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (a.sidePictures[side] != b.sidePictures[side]) return true;
  }
  return false;
}

void AutoMapScene_ApplyLook(const AutoMapCellLook &look, AutoMapCellRecord *record)
{
  record->kind = look.kind;
  record->doorState = look.doorState;
  record->pitIsOpen = look.pitIsOpen;
  record->pitIsHidden = look.pitIsHidden;
  record->spinnerIsVisible = look.spinnerIsVisible;
  record->floorPicture = look.floorPicture;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    record->wallSides[side].picture = look.sidePictures[side];
  }
}

bool AutoMapScene_CellLooksChanged(const AutoMapCellRecord &before, const AutoMapCellRecord &after)
{
  return AutoMapScene_LooksDiffer(AutoMapScene_LookOf(before), AutoMapScene_LookOf(after));
}

AutoMapCellLook AutoMapScene_VisibleLook(const AutoMapCellLook &seen,
                                         const AutoMapCellLook *remembered, bool pitIsKnown)
{
  AutoMapCellLook visible = seen;
  // A first sighting has nothing to keep: the map shows the pit as the reader found it
  if (seen.pitIsHidden && !pitIsKnown && remembered != NULL) visible.pitIsOpen = remembered->pitIsOpen;
  return visible;
}

bool AutoMapScene_SeenToChange(const AutoMapCellRecord &before, const AutoMapCellRecord &after,
                               bool pitIsKnown)
{
  AutoMapCellLook was = AutoMapScene_LookOf(before);
  AutoMapCellLook now = AutoMapScene_VisibleLook(AutoMapScene_LookOf(after), &was, pitIsKnown);
  return AutoMapScene_LooksDiffer(was, now);
}

AutoMapDrawList AutoMapScene_BuildDrawList(const AutoMapLevelSnapshot &snapshot,
                                           const AutoMapFog &fog)
{
  AutoMapDrawList drawList;
  drawList.level = snapshot.level;
  drawList.width = snapshot.width;
  drawList.height = snapshot.height;
  drawList.offsetX = snapshot.offsetX;
  drawList.offsetY = snapshot.offsetY;
  drawList.boundaryMarginX = snapshot.boundaryMarginX;
  drawList.boundaryMarginY = snapshot.boundaryMarginY;
  drawList.party = snapshot.party;
  drawList.partyIsOnShownLevel = snapshot.partyIsOnShownLevel;
  drawList.paletteScheme9 = snapshot.paletteScheme9;
  drawList.paletteScheme10 = snapshot.paletteScheme10;
  drawList.spoilers = fog.spoilers;
  AddCells(snapshot, fog, &drawList);
  AddTeleporterVisibility(fog, &drawList);
  AddMonsters(snapshot, fog, &drawList);
  AddMissiles(snapshot, fog, &drawList);
  AddClouds(snapshot, fog, &drawList);
  AddActuators(snapshot, fog, &drawList);
  if (fog.spoilers == AutoMapSpoilersAll)
  {
    drawList.linkedCells = snapshot.linkedCells;
    AddLinks(snapshot.links, &drawList);
  }
  else
  {
    AddLinks(KnownLinks(snapshot, fog), &drawList);
    // The badges follow the arrows that survived
    for (size_t i = 0; i < drawList.links.size(); i++)
    {
      AutoMapCell target = {snapshot.level, drawList.links[i].dstX, drawList.links[i].dstY};
      bool listed = false;
      for (size_t c = 0; c < drawList.linkedCells.size(); c++)
      {
        if (drawList.linkedCells[c].x == target.x && drawList.linkedCells[c].y == target.y)
        {
          listed = true;
        }
      }
      if (!listed) drawList.linkedCells.push_back(target);
    }
  }
  AddPlayerLinks(snapshot, fog, &drawList);
  return drawList;
}

std::vector<AutoMapLink> AutoMapScene_LinksFrom(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<AutoMapLink> found;
  for (size_t i = 0; i < drawList.links.size(); i++)
  {
    if (drawList.links[i].srcX == x && drawList.links[i].srcY == y) found.push_back(drawList.links[i]);
  }
  return found;
}

std::vector<AutoMapLink> AutoMapScene_LinksTo(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<AutoMapLink> found;
  for (size_t i = 0; i < drawList.links.size(); i++)
  {
    if (drawList.links[i].dstX == x && drawList.links[i].dstY == y) found.push_back(drawList.links[i]);
  }
  return found;
}

void AutoMapScene_GlobalPosition(const AutoMapDrawList &drawList, int x, int y,
                                 int *globalX, int *globalY)
{
  if (globalX != NULL) *globalX = x + drawList.offsetX;
  if (globalY != NULL) *globalY = y + drawList.offsetY;
}

std::vector<std::string> AutoMapScene_PlayerNoteLines(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<std::string> lines;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return lines;
  if (shown->playerPit == 1) lines.push_back("YOUR MARK: HIDDEN PIT OPEN");
  if (shown->playerPit == 0) lines.push_back("YOUR MARK: HIDDEN PIT CLOSED");
  for (size_t i = 0; i < drawList.playerLinks.size(); i++)
  {
    const AutoMapLink &link = drawList.playerLinks[i];
    if (link.srcX != x || link.srcY != y) continue;
    int globalX = 0;
    int globalY = 0;
    AutoMapScene_GlobalPosition(drawList, link.dstX, link.dstY, &globalX, &globalY);
    char line[64];
    std::snprintf(line, sizeof(line), "YOUR LINK: L%d (%d, %d)", drawList.level, globalX, globalY);
    lines.push_back(line);
  }
  return lines;
}

bool AutoMapScene_CellTakesNotes(const AutoMapDrawList &drawList, int x, int y)
{
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  return shown != NULL && !shown->cell.isOutsideLevel;
}

int AutoMapScene_NearestSide(int offsetX, int offsetY, int cellSize)
{
  // Distances to the edges in the tie order, so the first smallest wins
  int distances[4];
  distances[AutoMapNorth] = offsetY;
  distances[AutoMapEast] = cellSize - 1 - offsetX;
  distances[AutoMapSouth] = cellSize - 1 - offsetY;
  distances[AutoMapWest] = offsetX;
  int nearest = AutoMapNorth;
  for (int side = AutoMapEast; side <= AutoMapWest; side++)
  {
    if (distances[side] < distances[nearest]) nearest = side;
  }
  return nearest;
}

std::string AutoMapScene_MakeExcerpt(const std::string &text)
{
  static const size_t MAX_EXCERPT_CHARS = 180;
  std::string collapsed;
  bool lastWasSpace = true;
  for (size_t i = 0; i < text.size(); i++)
  {
    char c = text[i];
    bool isSpace = c == ' ' || c == '\t' || c == '\n' || c == '\r';
    if (isSpace)
    {
      if (!lastWasSpace) collapsed += ' ';
      lastWasSpace = true;
      continue;
    }
    collapsed += c;
    lastWasSpace = false;
  }
  while (!collapsed.empty() && collapsed[collapsed.size() - 1] == ' ')
  {
    collapsed.erase(collapsed.size() - 1);
  }
  if (collapsed.size() > MAX_EXCERPT_CHARS)
  {
    collapsed.erase(MAX_EXCERPT_CHARS - 3);
    collapsed += "...";
  }
  return collapsed;
}

std::vector<std::string> AutoMapScene_ScrollTexts(const AutoMapDrawList &drawList,
                                                  int x, int y)
{
  std::vector<std::string> empty;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return empty;
  std::vector<std::string> texts = shown->cell.scrollTexts;
  for (size_t i = 0; i < shown->cell.alcoveContents.size(); i++)
  {
    if (!shown->cell.alcoveContents[i].text.empty())
    {
      texts.push_back(shown->cell.alcoveContents[i].text);
    }
  }
  for (size_t i = 0; i < shown->cell.chestContents.size(); i++)
  {
    const AutoMapChestContents &chest = shown->cell.chestContents[i];
    for (size_t p = 0; p < chest.pictureTexts.size(); p++)
    {
      if (!chest.pictureTexts[p].empty()) texts.push_back(chest.pictureTexts[p]);
    }
  }
  for (size_t i = 0; i < drawList.monsters.size(); i++)
  {
    if (drawList.monsters[i].x != x || drawList.monsters[i].y != y) continue;
    for (size_t p = 0; p < drawList.monsters[i].possessionTexts.size(); p++)
    {
      if (!drawList.monsters[i].possessionTexts[p].empty())
      {
        texts.push_back(drawList.monsters[i].possessionTexts[p]);
      }
    }
  }
  return texts;
}

std::vector<AutoMapWallText> AutoMapScene_WallTexts(const AutoMapDrawList &drawList,
                                                   int x, int y)
{
  std::vector<AutoMapWallText> found;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return found;
  for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
  {
    if (!shown->cell.wallSides[side].text.empty())
    {
      AutoMapWallText entry;
      entry.side = side;
      entry.text = shown->cell.wallSides[side].text;
      found.push_back(entry);
    }
  }
  return found;
}

std::vector<AutoMapItemStack> AutoMapScene_TooltipItems(const AutoMapDrawList &drawList,
                                                        int x, int y)
{
  std::vector<AutoMapItemStack> empty;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return empty;
  return AutoMapScene_GroupItems(shown->allItemPictures);
}

std::vector<AutoMapItemStack> AutoMapScene_MonsterPossessions(const AutoMapDrawList &drawList,
                                                              int x, int y)
{
  std::vector<AutoMapItem> items;
  for (size_t i = 0; i < drawList.monsters.size(); i++)
  {
    if (drawList.monsters[i].x != x || drawList.monsters[i].y != y) continue;
    for (size_t p = 0; p < drawList.monsters[i].possessionPictures.size(); p++)
    {
      items.push_back(drawList.monsters[i].possessionPictures[p]);
    }
  }
  return AutoMapScene_GroupItems(items);
}

std::vector<std::string> AutoMapScene_MonsterHealthLines(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<std::string> lines;
  for (size_t i = 0; i < drawList.monsters.size(); i++)
  {
    const AutoMapShownMonster &monster = drawList.monsters[i];
    if (monster.x != x || monster.y != y) continue;
    for (size_t m = 0; m < monster.memberHealth.size(); m++)
    {
      char line[32];
      std::snprintf(line, sizeof(line), "HP %d", monster.memberHealth[m]);
      lines.push_back(line);
    }
    for (size_t m = 0; m < monster.memberDamage.size(); m++)
    {
      if (monster.memberDamage[m] <= 0) continue;
      char line[32];
      std::snprintf(line, sizeof(line), "DMG %d", monster.memberDamage[m]);
      lines.push_back(line);
    }
  }
  return lines;
}

AutoMapMonsterDrops AutoMapScene_MonsterDrops(const AutoMapDrawList &drawList, int x, int y)
{
  AutoMapMonsterDrops drops;
  std::vector<AutoMapItem> certainItems;
  std::vector<AutoMapItem> chanceItems;
  for (size_t i = 0; i < drawList.monsters.size(); i++)
  {
    if (drawList.monsters[i].x != x || drawList.monsters[i].y != y) continue;
    for (size_t p = 0; p < drawList.monsters[i].residuePictures.size(); p++)
    {
      certainItems.push_back(drawList.monsters[i].residuePictures[p]);
    }
    for (size_t p = 0; p < drawList.monsters[i].chanceResiduePictures.size(); p++)
    {
      chanceItems.push_back(drawList.monsters[i].chanceResiduePictures[p]);
    }
  }
  drops.certain = AutoMapScene_GroupItems(certainItems);
  drops.chance = AutoMapScene_GroupItems(chanceItems);
  return drops;
}

char FacingInitial(int facing)
{
  switch (facing)
  {
  case AutoMapNorth: return 'N';
  case AutoMapEast: return 'E';
  case AutoMapSouth: return 'S';
  case AutoMapWest: return 'W';
  default: return '?';
  }
}

// The engine's relative rotation (Code11f52.cpp): 0 keeps the facing,
// 1 turns right, 2 turns around, 3 turns left. NULL outside 0..3.
static const char *TurnName(int rotation)
{
  switch (rotation)
  {
  case 0: return "STRAIGHT";
  case 1: return "RIGHT";
  case 2: return "AROUND";
  case 3: return "LEFT";
  default: return NULL;
  }
}

static const float SPINNER_PI = 3.14159265f;

AutoMapSpinnerGeometry AutoMapScene_SpinnerGeometry(int spinnerFacing, int spinnerRotation)
{
  AutoMapSpinnerGeometry geometry = {AutoMapSpinnerUnknown, 0.0f, 2.0f * SPINNER_PI, 0.0f};
  if (spinnerFacing >= AutoMapNorth && spinnerFacing <= AutoMapWest)
  {
    // An absolute spinner forces its exit facing: full ring, radial head out.
    geometry.glyph = AutoMapSpinnerAbsolute;
    geometry.startAngle = 0.0f;
    geometry.sweepAngle = 2.0f * SPINNER_PI;
    geometry.headAngle = (1 - spinnerFacing) * SPINNER_PI / 2.0f;
    return geometry;
  }
  // Relative turns start at north so left/right mirror each other: right runs
  // 90 degrees clockwise to east, left 90 degrees counter-clockwise to west,
  // around a half circle to south.
  static const float NORTH = SPINNER_PI / 2.0f;
  switch (spinnerRotation)
  {
  case 1:
    geometry.glyph = AutoMapSpinnerRight;
    geometry.startAngle = NORTH;
    geometry.sweepAngle = -SPINNER_PI / 2.0f;
    geometry.headAngle = 0.0f;
    break;
  case 2:
    geometry.glyph = AutoMapSpinnerAround;
    geometry.startAngle = NORTH;
    geometry.sweepAngle = SPINNER_PI;
    geometry.headAngle = NORTH + SPINNER_PI;
    break;
  case 3:
    geometry.glyph = AutoMapSpinnerLeft;
    geometry.startAngle = NORTH;
    geometry.sweepAngle = SPINNER_PI / 2.0f;
    geometry.headAngle = SPINNER_PI;
    break;
  case 0:
    geometry.glyph = AutoMapSpinnerStraight;
    geometry.startAngle = 0.0f;
    geometry.sweepAngle = 0.0f;
    geometry.headAngle = 0.0f;
    break;
  default: break;
  }
  return geometry;
}

static std::string RequiredObjectName(AutoMapItemNameFn nameForIndex, const AutoMapItem &object)
{
  if (nameForIndex != NULL && object.nameIndex >= 0)
  {
    const char *name = nameForIndex(object.nameIndex);
    if (name != NULL && name[0] != '\0') return name;
  }
  return "(UNKNOWN)";
}

static void AddTriggerLine(std::vector<std::string> *lines, const std::string &line)
{
  for (size_t i = 0; i < lines->size(); i++)
  {
    if ((*lines)[i] == line) return;
  }
  lines->push_back(line);
}

bool AutoMapScene_GeneratorHealthRange(int levelMultiplier, int healthMultiplier,
                                       int baseHealth, int *minHealth, int *maxHealth)
{
  if (baseHealth < 0 || healthMultiplier < 0) return false;
  // The engine uses the level multiplier whenever the generator names none
  int effective = healthMultiplier == 0 ? levelMultiplier : healthMultiplier;
  if (effective < 0) return false;
  int minimum = effective * baseHealth;
  int maximum = minimum + baseHealth / 4;
  if (minHealth != NULL) *minHealth = minimum;
  if (maxHealth != NULL) *maxHealth = maximum;
  return true;
}

std::vector<std::string> AutoMapScene_GeneratorHealthLines(int levelMultiplier,
                                                             int healthMultiplier,
                                                             int minHealth, int maxHealth)
{
  std::vector<std::string> lines;
  if (minHealth < 0 || maxHealth < minHealth || healthMultiplier < 0) return lines;
  if (healthMultiplier == 0)
  {
    if (levelMultiplier < 0) return lines;
    char level[64];
    std::snprintf(level, sizeof(level), "LEVEL EXPERIENCE MULTIPLIER %d", levelMultiplier);
    lines.push_back(level);
  }
  else
  {
    char override[64];
    std::snprintf(override, sizeof(override), "HEALTH MULTIPLIER %d", healthMultiplier);
    lines.push_back(override);
    if (levelMultiplier >= 0)
    {
      char level[64];
      std::snprintf(level, sizeof(level), "LEVEL EXPERIENCE MULTIPLIER %d", levelMultiplier);
      lines.push_back(level);
    }
  }
  char health[64];
  if (maxHealth == minHealth) std::snprintf(health, sizeof(health), "HP RANGE %d", minHealth);
  else std::snprintf(health, sizeof(health), "HP RANGE %d-%d", minHealth, maxHealth);
  lines.push_back(health);
  return lines;
}

std::vector<std::string> AutoMapScene_TriggerLines(const AutoMapDrawList &drawList, int x, int y,
                                                   AutoMapItemNameFn nameForIndex)
{
  std::vector<std::string> lines;
  if (FindShownCell(drawList, x, y) == NULL) return lines;
  for (size_t i = 0; i < drawList.actuators.size(); i++)
  {
    const AutoMapFloorActuator &pad = drawList.actuators[i];
    if (pad.x != x || pad.y != y) continue;
    switch (pad.kind)
    {
    case AutoMapActuatorDormantPad: AddTriggerLine(&lines, "DORMANT PAD"); break;
    case AutoMapActuatorGenerator:
    {
      if (pad.hasGeneratedMonster && !pad.generatedMonsterName.empty())
      {
        if (pad.generatedCount > 1)
        {
          char generated[128];
          std::snprintf(generated, sizeof(generated), "GENERATES %dX %s", pad.generatedCount,
                        pad.generatedMonsterName.c_str());
          AddTriggerLine(&lines, generated);
        }
        else AddTriggerLine(&lines, "GENERATES " + pad.generatedMonsterName);
      }
      else AddTriggerLine(&lines, "GENERATES MONSTER");
      if (pad.hasGeneratorHealth)
      {
        std::vector<std::string> health = AutoMapScene_GeneratorHealthLines(
            pad.levelMultiplier, pad.healthMultiplier, pad.minHealth, pad.maxHealth);
        for (size_t h = 0; h < health.size(); h++) AddTriggerLine(&lines, health[h]);
      }
      break;
    }
    case AutoMapActuatorSpentGenerator: AddTriggerLine(&lines, "SPENT GENERATOR"); break;
    case AutoMapActuatorCog:
      // Cogs are drawn at All only, where the relay hover block (RelayHoverLines) names
      // each counter with its live count and each DSA: a line here would say it twice
      break;
    default:
    {
      // What a pad wants is not found out by stepping on it: the marker stands alone
      if (drawList.spoilers != AutoMapSpoilersAll) break;
      switch (pad.op)
      {
      case AutoMapPadPartyOrMonster:
        AddTriggerLine(&lines, "TRIGGER: PARTY OR MONSTER");
        break;
      case AutoMapPadParty: AddTriggerLine(&lines, "TRIGGER: PARTY"); break;
      case AutoMapPadPartyFacing:
      {
        std::string line("TRIGGER: PARTY FACING ");
        line += FacingInitial(pad.requiredFacing);
        AddTriggerLine(&lines, line);
        break;
      }
      case AutoMapPadObject:
        AddTriggerLine(&lines,
                       "TRIGGER: OBJECT " + RequiredObjectName(nameForIndex, pad.object));
        break;
      case AutoMapPadMonster: AddTriggerLine(&lines, "TRIGGER: MONSTER"); break;
      case AutoMapPadPossession:
        AddTriggerLine(&lines,
                       "TRIGGER: CARRY " + RequiredObjectName(nameForIndex, pad.object));
        break;
      default: AddTriggerLine(&lines, "TRIGGER: ANYTHING"); break;
      }
      break;
    }
    }
  }
  return lines;
}

static void AddRelayLine(std::vector<std::string> *lines, const std::string &line)
{
  for (size_t i = 0; i < lines->size(); i++)
  {
    if ((*lines)[i] == line) return;
  }
  lines->push_back(line);
}

// Four bits MSB-first, the way the gate compares them: low nibble first,
// e.g. value 3 reads "0011".
std::string GateBits(int value)
{
  std::string bits;
  for (int bit = 3; bit >= 0; bit--) bits += ((value >> bit) & 1) ? '1' : '0';
  return bits;
}

// What a stone-embedded launcher throws, after Timer.cpp TriggerMissileLauncher
// (missileType = value) with Mouse.cpp ConstructPredefinedMissile for stock and
// the 0xff80+n spell numbering for spells. NULL when the value names nothing
// game-legal: the tooltip stays silent instead of inventing ammunition.
static const char *LauncherMissileName(const AutoMapLauncher &launcher)
{
  switch (launcher.actuatorType)
  {
  case 7:
  case 9:
    switch (launcher.value)
    {
    case 4:
    case 5:
    case 6:
    case 7: return "TORCH";
    case 32: return "DAGGER";
    case 51: return "ARROW";
    case 52: return "SLAYER";
    case 54: return "ROCK";
    case 55: return "POISON DART";
    case 56: return "THROWING STAR";
    case 128: return "BOULDER";
    default: return NULL;
    }
  case 8:
  case 10:
    switch (launcher.value)
    {
    case 0: return "FIREBALL";
    case 1: return "POISON BLOB";
    case 2: return "LIGHTNING";
    case 3: return "DISPELL";
    case 6: return "POISON BOLT";
    case 7: return "POISON CLOUD";
    default: return NULL;
    }
  case 14:
  case 15: return "OBJECT";
  default: return NULL;
  }
}

// How many missiles one firing looses: types 9/10/15 a pair, the rest single
// (Timer.cpp: T7or8or14 fires once).
static int LauncherShots(const AutoMapLauncher &launcher)
{
  if (launcher.actuatorType == 9 || launcher.actuatorType == 10 || launcher.actuatorType == 15)
  {
    return 2;
  }
  return 1;
}

// What a firing leaves behind on a door or false-wall target, the way
// ProcessTT_DOOR/ProcessTT_FALSEWALL resolve it: SET opens (sets),
// CLEAR closes, TOGGLE flips, pressure holds while pressed.
const char *DoorEffectWord(AutoMapSignal signal)
{
  switch (signal)
  {
  case AutoMapSignalClear: return " CLOSE";
  case AutoMapSignalToggle: return " TOGGLE";
  case AutoMapSignalPressure: return " HOLD";
  default: return " OPEN";
  }
}

// The raw signal for targets with no open/close reading (and no counter delta
// covering it): engine ACTUATORACTION numbering, as in AsciiDump.
const char *SignalWord(AutoMapSignal signal)
{
  switch (signal)
  {
  case AutoMapSignalClear: return " CLEAR";
  case AutoMapSignalToggle: return " TOGGLE";
  case AutoMapSignalPressure: return " HOLD";
  default: return " SET";
  }
}

// How long the engine waits before the message lands (DB3 delay added to
// d.Time in QueueSwitchAction). Ticks run ~6/s (512 ticks ≈ 85 s, CSBCode.cpp),
// so 0..15 ticks read ~0..2.5 s: one decimal, trimmed.
std::string DelayWord(int delay)
{
  if (delay <= 0) return "";
  int tenths = (delay * 10 + 3) / 6;
  char word[32];
  if (tenths % 10 == 0) std::snprintf(word, sizeof(word), " AFTER %dS", tenths / 10);
  else std::snprintf(word, sizeof(word), " AFTER %d.%dS", tenths / 10, tenths % 10);
  return std::string(word);
}

std::vector<std::string> AutoMapScene_RelayLines(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<std::string> lines;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return lines;
  if (shown->cell.isOutsideLevel) AddRelayLine(&lines, "ADDED ROCK: OUTSIDE THE LEVEL");
  // Sealed doors first: no switch, no incoming link, no axe/fireball way
  // through. Bashable counts as openable and names the force that breaks it.
  if (shown->cell.kind == AutoMapCellDoor)
  {
    bool linked = !AutoMapScene_LinksTo(drawList, x, y).empty();
    if (drawList.spoilers == AutoMapSpoilersAll && AutoMapScene_DoorIsSealed(shown->cell, linked))
    {
      AddRelayLine(&lines, "SEALED");
    }
    const AutoMapCellRecord &door = shown->cell;
    bool breakable = door.doorBreakableWithAxe || door.doorBreakableWithFireball;
    if (breakable && door.doorState != AutoMapDoorDestroyed && door.doorBreakForce >= 0)
    {
      const char *tools = door.doorBreakableWithAxe
          ? (door.doorBreakableWithFireball ? "AXE, FIREBALL" : "AXE")
          : "FIREBALL";
      char line[64];
      std::snprintf(line, sizeof(line), "BREAK FORCE: %d (%s)", door.doorBreakForce, tools);
      AddRelayLine(&lines, line);
    }
  }
  // What this stone cell throws when messaged: one line per launcher, with the
  // firing side (launchers answer only messages naming their side). Unknown
  // ammunition stays silent; once-only launchers vanish from the rebuild spent.
  // Below All the party never learns what a launcher throws, so no line.
  for (size_t l = 0; drawList.spoilers == AutoMapSpoilersAll && l < shown->cell.wallLaunchers.size();
       l++)
  {
    const AutoMapLauncher &launcher = shown->cell.wallLaunchers[l];
    const char *missile = LauncherMissileName(launcher);
    if (missile == NULL) continue;
    char launches[64];
    if (LauncherShots(launcher) > 1)
    {
      std::snprintf(launches, sizeof(launches), "LAUNCHES 2X %s", missile);
    }
    else
    {
      std::snprintf(launches, sizeof(launches), "LAUNCHES %s", missile);
    }
    std::string line(launches);
    if (launcher.side >= AutoMapNorth && launcher.side <= AutoMapWest)
    {
      line += " (";
      line += FacingInitial(launcher.side);
      line += ")";
    }
    AddRelayLine(&lines, line);
  }
  // At All the press block (AutoMapScene_PressLines) names what a press reaches, so
  // the tooltip does not also list this cell's direct links. Below All the party saw
  // that a cell changed, not how: one plain line per outgoing link with a visible
  // target, in global ruler coordinates (levels count from zero, like the teleporter
  // DEST line). Unexplored targets stay silent so hovering never leaks the dungeon
  // under fog.
  if (drawList.spoilers == AutoMapSpoilersAll) return lines;
  std::vector<AutoMapLink> outgoing = AutoMapScene_LinksFrom(drawList, x, y);
  for (size_t i = 0; i < outgoing.size(); i++)
  {
    const AutoMapShownCell *dst = FindShownCell(drawList, outgoing[i].dstX, outgoing[i].dstY);
    if (dst == NULL) continue;
    char target[128];
    std::snprintf(target, sizeof(target), "TARGET: L%d (%d, %d)", drawList.level,
                  outgoing[i].dstX + drawList.offsetX, outgoing[i].dstY + drawList.offsetY);
    std::string line(target);
    if (dst->cell.kind == AutoMapCellDoor) line += " DOOR";
    AddRelayLine(&lines, line);
  }
  return lines;
}

static std::string TakesLine(int scope, bool hypothetical)
{
  const char *takes = NULL;
  switch (scope)
  {
  case 0: takes = "OBJECTS"; break;
  case 1: takes = "MONSTERS"; break;
  case 2: takes = "PARTY+OBJECTS"; break;
  case 3: takes = "ANYTHING"; break;
  default: break;
  }
  if (takes == NULL) return "";
  return std::string(hypothetical ? "WOULD TAKE: " : "TAKES: ") + takes;
}

static std::string FacingLine(int rotation, int facingMode, bool hypothetical)
{
  if (facingMode != 0 && facingMode != 1) return "";
  if (rotation < AutoMapNorth || rotation > AutoMapWest) return "";
  std::string prefix = hypothetical ? "WOULD " : "";
  if (facingMode == 1)
  {
    std::string line = prefix + "FACE ";
    line += FacingInitial(rotation);
    return line;
  }
  return prefix + "TURN: " + TurnName(rotation);
}

std::vector<std::string> AutoMapScene_TeleporterLines(const AutoMapDrawList &drawList,
                                                      int x, int y)
{
  std::vector<std::string> lines;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return lines;
  const AutoMapCellRecord &cell = shown->cell;
  if (cell.kind == AutoMapCellSpinner || cell.kind == AutoMapCellDormantSpinner)
  {
    // A dormant spinner names itself and then what it would do if switched on,
    // the way a dormant teleporter does.
    bool dormant = cell.kind == AutoMapCellDormantSpinner;
    if (dormant) lines.push_back("DORMANT SPINNER");
    std::string prefix = dormant ? "WOULD SPIN: " : "SPIN: ";
    if (cell.spinnerFacing >= AutoMapNorth && cell.spinnerFacing <= AutoMapWest)
    {
      std::string line(prefix);
      line += "FACE ";
      line += FacingInitial(cell.spinnerFacing);
      lines.push_back(line);
    }
    else if (TurnName(cell.spinnerRotation) != NULL)
    {
      lines.push_back(prefix + TurnName(cell.spinnerRotation));
    }
    else
    {
      lines.push_back(prefix + "RELATIVE");
    }
    return lines;
  }
  bool dormant = cell.kind == AutoMapCellDormantTeleporter;
  if (!dormant && cell.kind != AutoMapCellTeleporter
      && cell.kind != AutoMapCellInvisibleTeleporter)
  {
    return lines;
  }
  if (!cell.hasTeleporterTarget)
  {
    // No record behind the cell (or none resolved): nothing hypothetical to name.
    if (dormant) lines.push_back("DORMANT, NO EFFECT");
    return lines;
  }
  if (dormant) lines.push_back("DORMANT TELEPORTER");
  std::string takes = TakesLine(cell.teleporterScope, dormant);
  if (!takes.empty()) lines.push_back(takes);
  // The destination shares the arrow's visibility rule: naming it otherwise
  // would leak an unexplored cell under fog. Levels count the engine's way
  // from zero, as in the title bar and on the cross-level plate; coordinates
  // are global, as on the rulers.
  if (shown->teleporterTargetVisible)
  {
    char destination[64];
    std::snprintf(destination, sizeof(destination), "%sL%d (%d, %d)",
                  dormant ? "WOULD SEND: " : "DEST: ", cell.teleporterDestLevel,
                  cell.teleporterDestGlobalX, cell.teleporterDestGlobalY);
    lines.push_back(destination);
  }
  std::string facing = FacingLine(cell.teleporterRotation, cell.teleporterFacingMode, dormant);
  if (!facing.empty()) lines.push_back(facing);
  return lines;
}

std::vector<std::string> AutoMapScene_PitLines(const AutoMapDrawList &drawList, int x, int y)
{
  std::vector<std::string> lines;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return lines;
  // Only All keeps the flag: the floor hides a pit a message can open
  if (shown->cell.kind == AutoMapCellFloor && shown->cell.pitIsClosedSwitchable)
  {
    lines.push_back("PIT CLOSED");
    return lines;
  }
  if (shown->cell.kind != AutoMapCellPit) return lines;
  if (shown->cell.pitIsHidden)
  {
    lines.push_back(shown->cell.pitIsOpen ? "HIDDEN PIT OPEN" : "HIDDEN PIT CLOSED");
  }
  else
  {
    lines.push_back(shown->cell.pitIsOpen ? "PIT OPEN" : "PIT CLOSED");
  }
  return lines;
}

std::vector<AutoMapTooltipSlot> AutoMapScene_TooltipSlots(const AutoMapDrawList &drawList,
                                                          int x, int y)
{
  std::vector<AutoMapTooltipSlot> slots;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return slots;
  for (size_t i = 0; i < shown->allItemPictures.size(); i++)
  {
    const AutoMapChestContents *chest = NULL;
    for (size_t c = 0; c < shown->cell.chestContents.size(); c++)
    {
      if (shown->cell.chestContents[c].item == i)
      {
        chest = &shown->cell.chestContents[c];
        break;
      }
    }
    if (chest != NULL)
    {
      AutoMapTooltipSlot slot;
      slot.item = shown->allItemPictures[i];
      slot.count = 1;
      slot.isChest = true;
      slot.contents = AutoMapScene_GroupItems(chest->pictures);
      slots.push_back(slot);
      continue;
    }
    bool counted = false;
    for (size_t s = 0; s < slots.size(); s++)
    {
      if (!slots[s].isChest && slots[s].item == shown->allItemPictures[i])
      {
        slots[s].count++;
        counted = true;
        break;
      }
    }
    if (!counted)
    {
      AutoMapTooltipSlot slot;
      slot.item = shown->allItemPictures[i];
      slot.count = 1;
      slot.isChest = false;
      slots.push_back(slot);
    }
  }
  // Alcove contents group per side like the floor pile groups per cell, so a count
  // always means identical items on the same side. Requirements never merge with
  // present items: a stocked alcove next to its lock reads as two slots, not one
  // shared count. Entries without a world picture resolve to nothing and stay out.
  for (size_t i = 0; i < shown->cell.alcoveContents.size(); i++)
  {
    const AutoMapAlcoveEntry &entry = shown->cell.alcoveContents[i];
    if (entry.item.worldPicture == AUTOMAP_NO_PICTURE) continue;
    bool counted = false;
    for (size_t s = 0; s < slots.size(); s++)
    {
      if (!slots[s].isChest && !slots[s].required && slots[s].item == entry.item
          && slots[s].side == entry.side)
      {
        slots[s].count++;
        counted = true;
        break;
      }
    }
    if (!counted)
    {
      AutoMapTooltipSlot slot;
      slot.item = entry.item;
      slot.count = 1;
      slot.isChest = false;
      slot.side = entry.side;
      slots.push_back(slot);
    }
  }
  // One key in hand satisfies every matching actuator on the side at once (the
  // engine walks them all on a single click, Mouse.cpp), so identical requirements
  // on one side collapse to a single NEEDS no matter how many targets hide behind
  // the keyhole. The hover arrows still show each target separately. Plain
  // holders (containers without an action) read as plain icon and name, shown only
  // when missing: a stocked holder stays silent instead of doubling its contents.
  for (size_t i = 0; i < shown->cell.wallLocks.size(); i++)
  {
    const AutoMapWallLock &lock = shown->cell.wallLocks[i];
    if (lock.object.worldPicture == AUTOMAP_NO_PICTURE) continue;
    bool known = false;
    for (size_t s = 0; s < slots.size(); s++)
    {
      if (lock.required)
      {
        if (!slots[s].isChest && slots[s].required && slots[s].item == lock.object
            && slots[s].side == lock.side)
        {
          known = true;
          break;
        }
      }
      else if (!slots[s].isChest && !slots[s].required && slots[s].item == lock.object
               && slots[s].side == lock.side)
      {
        known = true;
        break;
      }
    }
    if (!known)
    {
      AutoMapTooltipSlot slot;
      slot.item = lock.object;
      slot.count = 1;
      slot.isChest = false;
      slot.side = lock.side;
      slot.required = lock.required;
      slots.push_back(slot);
    }
  }
  // Pad requirements read as NEEDS, never as floor loot: a missing firestaff
  // must not look pickable. CARRY needs (Possession) always show, even next to
  // an identical floor item which does not satisfy "carried". OBJECT needs (on
  // the pad) show only when missing: the item lying there both satisfies the
  // pad and is floor loot, so a single HAVE is correct. One carried item
  // satisfies every matching pad at once, so identical CARRY needs fold to one
  // NEEDS; CARRY and OBJECT needs for the same item stay separate. Entries
  // without a world picture resolve to nothing and stay out. Only actuators
  // the fog filter let through reach here, so hidden pads stay silent.
  for (size_t i = 0; i < drawList.actuators.size(); i++)
  {
    const AutoMapFloorActuator &pad = drawList.actuators[i];
    if (pad.x != x || pad.y != y) continue;
    if (pad.kind != AutoMapActuatorPad) continue;
    if (pad.op != AutoMapPadObject && pad.op != AutoMapPadPossession) continue;
    if (pad.object.worldPicture == AUTOMAP_NO_PICTURE) continue;
    bool isCarry = pad.op == AutoMapPadPossession;
    if (!isCarry)
    {
      bool present = false;
      for (size_t s = 0; s < slots.size(); s++)
      {
        if (!slots[s].isChest && !slots[s].required && slots[s].side == -1
            && slots[s].item == pad.object)
        {
          present = true;
          break;
        }
      }
      if (present) continue;
    }
    bool known = false;
    for (size_t s = 0; s < slots.size(); s++)
    {
      if (!slots[s].isChest && slots[s].required && slots[s].side == -1
          && slots[s].carried == isCarry && slots[s].item == pad.object)
      {
        known = true;
        break;
      }
    }
    if (!known)
    {
      AutoMapTooltipSlot slot;
      slot.item = pad.object;
      slot.count = 1;
      slot.isChest = false;
      slot.side = -1;
      slot.required = true;
      slot.carried = isCarry;
      slots.push_back(slot);
    }
  }
  return slots;
}

static std::string Lowered(const std::string &text)
{
  std::string lowered;
  lowered.reserve(text.size());
  for (size_t i = 0; i < text.size(); i++)
  {
    char c = text[i];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    lowered += c;
  }
  return lowered;
}

static std::string Trimmed(const std::string &text)
{
  size_t first = 0;
  while (first < text.size() && (text[first] == ' ' || text[first] == '\t')) first++;
  size_t last = text.size();
  while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t')) last--;
  return text.substr(first, last - first);
}

// 0 = exact, 1 = prefix, 2 = substring, -1 = no match
static int MatchRank(const std::string &loweredName, const std::string &loweredQuery)
{
  if (loweredQuery.empty() || loweredName.empty()) return -1;
  if (loweredName == loweredQuery) return 0;
  if (loweredName.size() >= loweredQuery.size()
      && loweredName.compare(0, loweredQuery.size(), loweredQuery) == 0)
  {
    return 1;
  }
  if (loweredName.find(loweredQuery) != std::string::npos) return 2;
  return -1;
}

int AutoMapScene_SearchRank(const std::string &name, const std::string &query)
{
  std::string trimmed = Trimmed(query);
  if (trimmed.empty() || name.empty() || name == "(UNKNOWN)") return -1;
  return MatchRank(Lowered(name), Lowered(trimmed));
}

// One word finds every item the champions can eat, instead of eight names
static const char *const AUTOMAP_FOOD_WORD = "FOOD";

static int BetterRank(int left, int right)
{
  if (left < 0) return right;
  if (right < 0) return left;
  return left < right ? left : right;
}

const char *AutoMapScene_WallFeatureName(AutoMapWallFeature feature)
{
  switch (feature)
  {
  case AutoMapWallFeatureFountain: return "WATER FOUNTAIN";
  case AutoMapWallFeatureViAltar: return "ALTAR OF VI";
  default: return "";
  }
}

int AutoMapScene_SearchHitRank(const AutoMapSearchHit &hit, const std::string &query)
{
  int rank = AutoMapScene_SearchRank(hit.name, query);
  if (hit.isFood) rank = BetterRank(rank, AutoMapScene_SearchRank(AUTOMAP_FOOD_WORD, query));
  return rank;
}

namespace
{
struct SearchHitOrder
{
  std::string query;
  bool operator()(const AutoMapSearchHit &a, const AutoMapSearchHit &b) const
  {
    int rankA = AutoMapScene_SearchHitRank(a, query);
    int rankB = AutoMapScene_SearchHitRank(b, query);
    if (rankA != rankB) return rankA < rankB;
    if (a.level != b.level) return a.level < b.level;
    if (a.y != b.y) return a.y < b.y;
    if (a.x != b.x) return a.x < b.x;
    bool aIsFeature = a.wallFeature != AutoMapWallFeatureNone;
    bool bIsFeature = b.wallFeature != AutoMapWallFeatureNone;
    if (aIsFeature != bIsFeature) return !aIsFeature;
    if (a.name != b.name) return a.name < b.name;
    if (a.monsterCarried != b.monsterCarried) return !a.monsterCarried;
    return a.slot.side < b.slot.side;
  }
};
}

void AutoMapScene_SortSearchHits(std::vector<AutoMapSearchHit> *hits, const std::string &query)
{
  SearchHitOrder order;
  order.query = Trimmed(query);
  std::stable_sort(hits->begin(), hits->end(), order);
}

std::vector<AutoMapSearchHit> AutoMapScene_SearchItems(const AutoMapDrawList &drawList,
                                                       const std::string &query,
                                                       AutoMapItemNameFn nameForIndex,
                                                       AutoMapItemIsFoodFn isFood)
{
  std::vector<AutoMapSearchHit> hits;
  std::string trimmed = Trimmed(query);
  if (trimmed.empty()) return hits;
  for (size_t i = 0; i < drawList.cells.size(); i++)
  {
    int x = drawList.cells[i].x;
    int y = drawList.cells[i].y;
    // The floor, chests, alcoves and NEEDS first; carried items ride with their monster: under
    // fog the draw list only holds monsters on cells the party sees right now, so these hits obey
    // the same rule without any extra check here.
    std::vector<AutoMapSearchHit> candidates;
    std::vector<AutoMapTooltipSlot> slots = AutoMapScene_TooltipSlots(drawList, x, y);
    for (size_t s = 0; s < slots.size(); s++)
    {
      AutoMapSearchHit hit;
      hit.slot = slots[s];
      candidates.push_back(hit);
    }
    std::vector<AutoMapItemStack> carried = AutoMapScene_MonsterPossessions(drawList, x, y);
    for (size_t s = 0; s < carried.size(); s++)
    {
      AutoMapSearchHit hit;
      hit.slot.item = carried[s].item;
      hit.slot.count = carried[s].count;
      hit.monsterCarried = true;
      candidates.push_back(hit);
    }
    size_t itemCount = candidates.size();
    // A face the fog hides is a plain side here, so only faces the map shows can match
    for (int side = 0; side < AUTOMAP_SIDE_COUNT; side++)
    {
      AutoMapWallFeature feature = drawList.cells[i].cell.wallSides[side].feature;
      if (feature == AutoMapWallFeatureNone) continue;
      AutoMapSearchHit hit;
      hit.slot.side = side;
      hit.wallFeature = feature;
      candidates.push_back(hit);
    }
    for (size_t c = 0; c < candidates.size(); c++)
    {
      AutoMapSearchHit hit = candidates[c];
      hit.level = drawList.level;
      hit.x = x;
      hit.y = y;
      AutoMapScene_GlobalPosition(drawList, x, y, &hit.globalX, &hit.globalY);
      if (c >= itemCount)
      {
        hit.name = AutoMapScene_WallFeatureName(hit.wallFeature);
        if (AutoMapScene_SearchHitRank(hit, trimmed) < 0) continue;
        hits.push_back(hit);
        continue;
      }
      hit.name = RequiredObjectName(nameForIndex, hit.slot.item);
      // An item of no known name is no food the player could tell either
      hit.isFood = isFood != NULL && hit.slot.item.nameIndex >= 0 && hit.name != ""
                   && hit.name != "(UNKNOWN)" && isFood(hit.slot.item.nameIndex);
      if (AutoMapScene_SearchHitRank(hit, trimmed) < 0) continue;
      hits.push_back(hit);
    }
  }
  AutoMapScene_SortSearchHits(&hits, trimmed);
  return hits;
}

bool AutoMapScene_DoorIsSealed(const AutoMapCellRecord &cell, bool isLinked)
{
  if (cell.kind != AutoMapCellDoor) return false;
  // An open door nothing operates stays open: it lets the party through, unlike a wall
  if (cell.doorState == AutoMapDoorDestroyed || cell.doorState == AutoMapDoorOpen) return false;
  if (cell.doorHasSwitch) return false;
  if (isLinked) return false;
  if (cell.doorBreakableWithAxe || cell.doorBreakableWithFireball) return false;
  return true;
}

int AutoMapScene_PredictDoorState(AutoMapDoorState current, AutoMapSignal signal)
{
  if (current == AutoMapDoorDestroyed) return -1;
  AutoMapDoorState predicted = current;
  switch (signal)
  {
  case AutoMapSignalClear: predicted = AutoMapDoorClosed; break;
  case AutoMapSignalToggle:
    predicted = current == AutoMapDoorOpen ? AutoMapDoorClosed : AutoMapDoorOpen;
    break;
  default: predicted = AutoMapDoorOpen; break;
  }
  if (predicted == current) return -1;
  return predicted;
}

int AutoMapScene_ClampZoomSteps(int zoomSteps)
{
  return std::max(0, std::min(zoomSteps, AUTOMAP_MOST_ZOOM_STEPS));
}

static int LargestCellSizeShowingTheWholeLevel(const AutoMapViewRequest &request)
{
  bool nothingToShow = request.levelSize.width <= 0 || request.levelSize.height <= 0;
  bool noRoom = request.windowWidth <= 0 || request.windowHeight <= 0;
  if (nothingToShow || noRoom) return 0;
  return std::min(request.windowWidth / request.levelSize.width,
                  request.windowHeight / request.levelSize.height);
}

struct ViewAxis
{
  int windowPixels;
  int levelCells;
  float centerCell;
};

static int RoundToPixels(float position)
{
  return (int)floor(position + 0.5f);
}

static int OriginAlong(const ViewAxis &axis, int cellSize)
{
  int levelPixels = axis.levelCells * cellSize;
  if (levelPixels <= axis.windowPixels) return (axis.windowPixels - levelPixels) / 2;
  float center = axis.centerCell * cellSize + cellSize / 2.0f;
  int originCentred = axis.windowPixels / 2 - RoundToPixels(center);
  int originShowingLastCell = axis.windowPixels - levelPixels;
  return std::max(originShowingLastCell, std::min(originCentred, 0));
}

AutoMapView AutoMapScene_PlaceLevel(const AutoMapViewRequest &request)
{
  int zoomSteps = AutoMapScene_ClampZoomSteps(request.zoomSteps);
  int fittingCellSize = LargestCellSizeShowingTheWholeLevel(request);
  ViewAxis horizontal = {request.windowWidth, request.levelSize.width, request.centerX};
  ViewAxis vertical = {request.windowHeight, request.levelSize.height, request.centerY};
  AutoMapView view;
  view.cellSize = fittingCellSize * (ZOOM_STEPS_PER_FITTING_SIZE + zoomSteps) / ZOOM_STEPS_PER_FITTING_SIZE;
  view.originX = OriginAlong(horizontal, view.cellSize);
  view.originY = OriginAlong(vertical, view.cellSize);
  return view;
}

int AutoMapScene_LabelStep(int cellSize, int numberPixelWidth)
{
  if (cellSize <= 0) return 1;
  return std::max(1, (numberPixelWidth + cellSize - 1) / cellSize);
}

static int FloorDivide(int value, int divisor)
{
  // divisor is a positive cell size; C++ truncates towards zero, rulers need the floor
  int truncated = value / divisor;
  if (value < 0 && value % divisor != 0) truncated--;
  return truncated;
}

void AutoMapScene_VisibleRange(int originPixels, int cellSize, int mapPixels, int levelCells,
                               int *first, int *last)
{
  if (cellSize <= 0 || mapPixels <= 0 || levelCells <= 0)
  {
    *first = 0;
    *last = -1;
    return;
  }
  int firstCell = FloorDivide(-originPixels, cellSize);
  int lastCell = FloorDivide(mapPixels - originPixels - 1, cellSize);
  *first = std::max(0, firstCell);
  *last = std::min(levelCells - 1, lastCell);
}

// The digits 0 to 9, each five rows of three pixels, the leftmost pixel in the highest bit
static const unsigned char DIGIT_ROWS[10][5] = {
  {0x7, 0x5, 0x5, 0x5, 0x7}, // 0
  {0x2, 0x6, 0x2, 0x2, 0x7}, // 1
  {0x7, 0x1, 0x7, 0x4, 0x7}, // 2
  {0x7, 0x1, 0x7, 0x1, 0x7}, // 3
  {0x5, 0x5, 0x7, 0x1, 0x1}, // 4
  {0x7, 0x4, 0x7, 0x1, 0x7}, // 5
  {0x7, 0x4, 0x7, 0x5, 0x7}, // 6
  {0x7, 0x1, 0x2, 0x2, 0x2}, // 7
  {0x7, 0x5, 0x7, 0x5, 0x7}, // 8
  {0x7, 0x5, 0x7, 0x1, 0x7}, // 9
};

bool AutoMapScene_DigitPixel(int digit, int row, int col)
{
  if (digit < 0 || digit > 9 || row < 0 || row > 4 || col < 0 || col > 2) return false;
  return (DIGIT_ROWS[digit][row] & (1 << (2 - col))) != 0;
}

// Same 3x5 look as the digits: uppercase letters and a little punctuation, five
// rows of three pixels, leftmost pixel in the highest bit.
static const unsigned char LETTER_ROWS[26][5] = {
  {0x2, 0x5, 0x7, 0x5, 0x5}, // A
  {0x6, 0x5, 0x6, 0x5, 0x6}, // B
  {0x3, 0x4, 0x4, 0x4, 0x3}, // C
  {0x6, 0x5, 0x5, 0x5, 0x6}, // D
  {0x7, 0x4, 0x6, 0x4, 0x7}, // E
  {0x7, 0x4, 0x6, 0x4, 0x4}, // F
  {0x3, 0x4, 0x5, 0x5, 0x3}, // G
  {0x5, 0x5, 0x7, 0x5, 0x5}, // H
  {0x7, 0x2, 0x2, 0x2, 0x7}, // I
  {0x1, 0x1, 0x1, 0x5, 0x2}, // J
  {0x5, 0x5, 0x6, 0x5, 0x5}, // K
  {0x4, 0x4, 0x4, 0x4, 0x7}, // L
  {0x5, 0x7, 0x5, 0x5, 0x5}, // M
  {0x6, 0x5, 0x5, 0x5, 0x5}, // N
  {0x2, 0x5, 0x5, 0x5, 0x2}, // O
  {0x6, 0x5, 0x6, 0x4, 0x4}, // P
  {0x2, 0x5, 0x5, 0x6, 0x3}, // Q
  {0x6, 0x5, 0x6, 0x5, 0x5}, // R
  {0x3, 0x4, 0x2, 0x1, 0x6}, // S
  {0x7, 0x2, 0x2, 0x2, 0x2}, // T
  {0x5, 0x5, 0x5, 0x5, 0x7}, // U
  {0x5, 0x5, 0x5, 0x5, 0x2}, // V
  {0x5, 0x5, 0x5, 0x7, 0x5}, // W
  {0x5, 0x5, 0x2, 0x5, 0x5}, // X
  {0x5, 0x5, 0x2, 0x2, 0x2}, // Y
  {0x7, 0x1, 0x2, 0x4, 0x7}, // Z
};

static bool PunctuationPixel(char mark, int row, int col)
{
  unsigned char rows[5];
  switch (mark)
  {
  case ' ': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x0; break;
  case '+': rows[0] = 0x0; rows[1] = 0x2; rows[2] = 0x7; rows[3] = 0x2; rows[4] = 0x0; break;
  case '-': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x7; rows[3] = 0x0; rows[4] = 0x0; break;
  case '.': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x2; break;
  case ',': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x2; rows[4] = 0x4; break;
  case ':': rows[0] = 0x0; rows[1] = 0x2; rows[2] = 0x0; rows[3] = 0x2; rows[4] = 0x0; break;
  case '?': rows[0] = 0x6; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x0; rows[4] = 0x2; break;
  case '!': rows[0] = 0x2; rows[1] = 0x2; rows[2] = 0x2; rows[3] = 0x0; rows[4] = 0x2; break;
  case '/': rows[0] = 0x1; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x4; rows[4] = 0x4; break;
  case '(': rows[0] = 0x2; rows[1] = 0x4; rows[2] = 0x4; rows[3] = 0x4; rows[4] = 0x2; break;
  case ')': rows[0] = 0x2; rows[1] = 0x1; rows[2] = 0x1; rows[3] = 0x1; rows[4] = 0x2; break;
  case '=': rows[0] = 0x0; rows[1] = 0x7; rows[2] = 0x0; rows[3] = 0x7; rows[4] = 0x0; break;
  case '%': rows[0] = 0x5; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x4; rows[4] = 0x5; break;
  case '\'': rows[0] = 0x2; rows[1] = 0x2; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x0; break;
  case '>': rows[0] = 0x4; rows[1] = 0x2; rows[2] = 0x1; rows[3] = 0x2; rows[4] = 0x4; break;
  default: return false;
  }
  if (row < 0 || row > 4 || col < 0 || col > 2) return false;
  return (rows[row] & (1 << (2 - col))) != 0;
}

bool AutoMapScene_FontPixel(char mark, int row, int col)
{
  if (mark >= 'a' && mark <= 'z') mark = (char)(mark - 'a' + 'A');
  if (mark >= 'A' && mark <= 'Z')
  {
    if (row < 0 || row > 4 || col < 0 || col > 2) return false;
    return (LETTER_ROWS[mark - 'A'][row] & (1 << (2 - col))) != 0;
  }
  if (mark >= '0' && mark <= '9') return AutoMapScene_DigitPixel(mark - '0', row, col);
  return PunctuationPixel(mark, row, col);
}

int AutoMapScene_TextWidth(const char *text)
{
  if (text == NULL) return 0;
  int length = 0;
  while (text[length] != '\0') length++;
  if (length == 0) return 0;
  return length * 4 - 1;
}

// The 3D view shrinks spell fire by (range + 1) / 256 of its size, never below 96 / 256
static const int SMALLEST_MISSILE_FRACTION = 96;
static const int MISSILE_SCALE_DIVISOR = 256;

float AutoMapScene_MissileScale(int rangeRemaining)
{
  int fraction = std::max(SMALLEST_MISSILE_FRACTION, rangeRemaining + 1);
  return (float)fraction / MISSILE_SCALE_DIVISOR;
}

static bool PartyCanStandIn(const AutoMapCellRecord &cell)
{
  if (cell.isOutsideLevel) return false;
  switch (cell.kind)
  {
  case AutoMapCellWall:
    return false;
  case AutoMapCellFalseWall:
    return cell.falseWallIsPassable;
  case AutoMapCellDoor:
    return cell.doorState == AutoMapDoorOpen || cell.doorState == AutoMapDoorDestroyed;
  default:
    return true;
  }
}

AutoMapPlacement AutoMapScene_Placement(const AutoMapLevelSnapshot &snapshot, int x, int y)
{
  if (x < 0 || y < 0 || x >= snapshot.width || y >= snapshot.height) return AutoMapPlacementSolid;
  if ((size_t)(y * snapshot.width + x) >= snapshot.cells.size()) return AutoMapPlacementSolid;
  if (!PartyCanStandIn(snapshot.cells[(size_t)(y * snapshot.width + x)]))
  {
    return AutoMapPlacementSolid;
  }
  for (size_t i = 0; i < snapshot.monsters.size(); i++)
  {
    if (snapshot.monsters[i].x == x && snapshot.monsters[i].y == y) return AutoMapPlacementMonster;
  }
  if (snapshot.partyIsOnShownLevel && snapshot.party.x == x && snapshot.party.y == y)
  {
    return AutoMapPlacementParty;
  }
  return AutoMapPlacementFree;
}

const char *AutoMapScene_DescribePlacement(AutoMapPlacement placement)
{
  switch (placement)
  {
  case AutoMapPlacementSolid: return "SOLID";
  case AutoMapPlacementMonster: return "MONSTER";
  case AutoMapPlacementParty: return "PARTY";
  default: return "";
  }
}

static AutoMapSpawnSpot SpawnSpot(const char *name, int positions, int firstFacing, int facingStep)
{
  AutoMapSpawnSpot spot;
  spot.name = name;
  spot.positions = positions;
  for (int facing = firstFacing; facing < AUTOMAP_SIDE_COUNT; facing += facingStep) spot.facings.push_back(facing);
  return spot;
}

int AutoMapScene_LargestGroup(int horizontalSize)
{
  if (horizontalSize == 0) return 4;
  if (horizontalSize == 1) return 2;
  return 1;
}

std::vector<AutoMapSpawnSpot> AutoMapScene_SpawnSpots(int horizontalSize)
{
  std::vector<AutoMapSpawnSpot> spots;
  spots.push_back(SpawnSpot("CENTRE", 255, AutoMapNorth, 1));
  if (horizontalSize == 0)
  {
    spots.push_back(SpawnSpot("NORTH WEST", 0, AutoMapNorth, 1));
    spots.push_back(SpawnSpot("NORTH EAST", 1, AutoMapNorth, 1));
    spots.push_back(SpawnSpot("SOUTH EAST", 2, AutoMapNorth, 1));
    spots.push_back(SpawnSpot("SOUTH WEST", 3, AutoMapNorth, 1));
  }
  else if (horizontalSize == 1)
  {
    // The engine reads a half from one quadrant and the facing (OrdinalOfMonsterAtPosition)
    spots.push_back(SpawnSpot("NORTH HALF", 0, AutoMapEast, 2));
    spots.push_back(SpawnSpot("EAST HALF", 1, AutoMapNorth, 2));
    spots.push_back(SpawnSpot("SOUTH HALF", 2, AutoMapEast, 2));
    spots.push_back(SpawnSpot("WEST HALF", 3, AutoMapNorth, 2));
  }
  return spots;
}

int AutoMapScene_SkillLevel(int experience)
{
  int level = 1;
  for (; experience >= 500; level++) experience >>= 1;
  return level;
}

int AutoMapScene_ExperienceToNextLevel(int experience)
{
  long long next = 500LL << (AutoMapScene_SkillLevel(experience) - 1);
  if (next > AUTOMAP_SKILL_EXPERIENCE_CAP) return 0;
  return (int)(next - experience);
}

void AutoMapScene_RememberExploredLooks(const AutoMapLevelSnapshot &snapshot,
                                        const AutoMapExplored &explored, AutoMapCellMemory *memory)
{
  for (int y = 0; y < snapshot.height; y++)
  {
    for (int x = 0; x < snapshot.width; x++)
    {
      const AutoMapCellRecord &record = snapshot.cells[(size_t)y * snapshot.width + x];
      if (record.isOutsideLevel) continue;
      AutoMapCell levelCell = LevelCellOf(snapshot, x, y);
      if (!explored.IsExplored(levelCell)) continue;
      memory->RememberIfNew(levelCell, AutoMapScene_LookOf(record));
    }
  }
}
