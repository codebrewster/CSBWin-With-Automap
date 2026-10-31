#ifndef AUTOMAPSCENE_H
#define AUTOMAPSCENE_H

#include <map>
#include <string>
#include <vector>

#include "AutoMapExplored.h"
#include "AutoMapItemMemory.h"
#include "AutoMapKnowledge.h"
#include "AutoMapMonsterDamage.h"
#include "AutoMapSight.h"

// What the map window draws, worked out from plain records of one level. Nothing in here
// touches the engine or SDL: AutoMapLevelReader fills the records, AutoMapWindow draws the list.

// The engine's numbers for facings and for the sides of a cell
enum AutoMapDirection
{
  AutoMapNorth = 0,
  AutoMapEast = 1,
  AutoMapSouth = 2,
  AutoMapWest = 3
};

enum AutoMapCellKind
{
  AutoMapCellWall,
  AutoMapCellFloor,
  AutoMapCellPit,
  AutoMapCellStairs,
  AutoMapCellDoor,
  AutoMapCellTeleporter,
  // An illusionary wall: solid and drawn like a wall, but passable by other means
  AutoMapCellFalseWall,
  // An illusionary wall that is currently passable: walked like a floor
  AutoMapCellOpenFalseWall,
  // A teleporter the 3D view does not show although it fires: real and active
  AutoMapCellInvisibleTeleporter,
  // A teleporter that fires nothing: present in the data, switched off for now
  AutoMapCellDormantTeleporter,
  // A teleporter that keeps its victim on its own cell: it spins instead of moving
  AutoMapCellSpinner,
  // A spinner that fires nothing: present in the data, switched off for now
  AutoMapCellDormantSpinner
};

const int AUTOMAP_SIDE_COUNT = 4;
const int AUTOMAP_NO_PICTURE = -1;
const int AUTOMAP_NO_LEVEL = -1;
const int AUTOMAP_LARGEST_GROUP = 4;
const int AUTOMAP_MOST_ITEMS_SHOWN = 3;
const int AUTOMAP_MOST_ZOOM_STEPS = 12;
// The colour schemes ReadGraphicsForLevel starts from before the monster types of the
// level replace them
extern const int AUTOMAP_DEFAULT_SCHEME9;
extern const int AUTOMAP_DEFAULT_SCHEME10;

// What a wall face is for, beyond its picture: the level's water fountain (Word4040) or its Vi
// altar (the second alcove graphic, Byte4038[1]). Not a square or ornate alcove.
enum AutoMapWallFeature
{
  AutoMapWallFeatureNone = 0,
  AutoMapWallFeatureFountain = 1,
  AutoMapWallFeatureViAltar = 2
};

// What a wall decoration is: random dressing with no object behind it (flavour), a real
// actuator or a usable alcove (functional), or machinery the 3D view hides (hidden).
// A type-0 actuator is an inert decoration, so it reads as functional (its original
// picture shows) unless the decoration is the manacles, the one pure-dressing graphic
// the map greys; a usable alcove or water fountain on a plain face is functional too.
struct AutoMapWallSide
{
  int picture;
  bool hasWriting;
  bool functional;
  bool hidden;
  // True when the face decoration is one of the level's alcove graphics. The
  // 3D view only draws wall-embedded items on alcove faces (Viewport
  // Alcove-gated contents pass), so the reader lists wall items as alcove
  // goods only there; anything embedded in a plain lock/dent wall (like the
  // firestaff inside the gem wall on DM L13) stays invisible like in the game.
  bool isAlcove;
  // Champion portrait on this side (actuator type 127, e.g. the Hall of
  // Champions on DM L0): the champion index 0..511 the 3D view draws
  // (DrawWallDecoration portrait pass), AUTOMAP_NO_PICTURE when none or
  // hidden (PORTRAIT_HidePortrait). Independent of isAlcove and of the
  // alcove-goods gate: portraits hold starting gear like a niche although
  // the 3D view shows just the portrait.
  int portrait;
  // Excerpt of the wall text on this side, "" when none. Filled by the
  // reader, cleared with the side by the fog filter.
  std::string text;
  // Every wall actuator on this side, in record order (inert decorations too), so the
  // knowledge store can tell which of them the party has found out.
  std::vector<AutoMapRecordId> actuators;
  // What the face is for, for the search. Hidden with the side, like the picture.
  AutoMapWallFeature feature;
};

// One item as the game knows it: the small sprite the 3D view draws plus, when the
// type is known, the inventory tile and the name. Spots without a known type carry
// the world picture only.
struct AutoMapItem
{
  AutoMapItem(void)
    : worldPicture(AUTOMAP_NO_PICTURE), hasIcon(false),
      iconGraphic(AUTOMAP_NO_PICTURE), iconTile(0), nameIndex(-1), descIndex(-1)
  {
  }
  int worldPicture;
  bool hasIcon;
  // Basic graphic 42..48, a sheet of 16x16 tiles the way the inventory draws them
  int iconGraphic;
  // Tile index within the graphic: ((tile & 15) * 16, (tile >> 4) * 16), 16x16
  int iconTile;
  // Object type for the engine's object names, -1 when unknown
  int nameIndex;
  // The object description (d.ObjDesc), -1 when unknown; tells the kind of item
  int descIndex;
};

inline bool operator==(const AutoMapItem &left, const AutoMapItem &right)
{
  return left.worldPicture == right.worldPicture && left.hasIcon == right.hasIcon
      && (!left.hasIcon || (left.iconGraphic == right.iconGraphic
                            && left.iconTile == right.iconTile))
      && left.nameIndex == right.nameIndex;
}

// One chest's contents: which full-list item it belongs to plus its items.
// pictureTexts runs parallel to pictures: the scroll excerpt of each contained
// scroll, "" for everything else.
struct AutoMapChestContents
{
  AutoMapChestContents(void) : item(0), chest(AUTOMAP_NO_RECORD)
  {
  }
  size_t item;
  std::vector<AutoMapItem> pictures;
  std::vector<std::string> pictureTexts;
  // The chest's own record, and the record of each contained scroll (parallel to
  // pictures, AUTOMAP_NO_RECORD for everything else).
  AutoMapRecordId chest;
  std::vector<AutoMapRecordId> pictureScrolls;
};

// One item resting in a wall alcove: the side (an AutoMapDirection) it faces.
// The reader keeps the side so the fog filter can hide alcove contents whose
// side has not been seen yet, the way it hides the wall decoration itself.
struct AutoMapAlcoveEntry
{
  AutoMapAlcoveEntry(void)
    : side(-1), record(AUTOMAP_NO_RECORD), scroll(AUTOMAP_NO_RECORD), inChest(AUTOMAP_NO_RECORD)
  {
  }
  AutoMapItem item;
  int side;
  // The item's record. Below All an entry shows once this item was noticed in the alcove (an
  // item in a chest: once the chest was), not because another item there was.
  AutoMapRecordId record;
  // Scroll excerpt when the alcoved item is a scroll with text, "" otherwise.
  // Hides with the entry under fog.
  std::string text;
  // The scroll record when the alcoved item is a scroll, AUTOMAP_NO_RECORD otherwise.
  AutoMapRecordId scroll;
  // The chest record when the item lies in a chest in the alcove (the reader flattens the
  // chest's contents next to it), AUTOMAP_NO_RECORD otherwise.
  AutoMapRecordId inChest;
};

// One wall actuator that wants an object in the party's hand or niche: the side
// it sits on plus the required object type as the item the 3D view draws for it.
// Covers locks (types 3, 4, 11, 17), torch holders (type 13) and stone alcoves
// (type 2, whose value names the object the alcove accepts). Pushbuttons and
// hand-testers whose value names no item leave no entry. Holders without an
// action are mere containers and read plain (required false); everything else
// reads as a NEEDS.
struct AutoMapWallLock
{
  AutoMapWallLock(void) : side(-1), actuatorType(0), required(false),
                          actuator(AUTOMAP_NO_RECORD)
  {
  }
  int side;
  AutoMapItem object;
  int actuatorType;
  bool required;
  AutoMapRecordId actuator;
};

// One missile launcher embedded in stone: the actuator type (7/8/9/10 for
// spell/predefined missiles, 14/15 for room objects, Timer.cpp
// TriggerMissileLauncher), the missile selector (value), and the side
// (an AutoMapDirection) it fires toward, -1 when unset. Types 9/10/15 loose
// two missiles per firing, the rest one.
struct AutoMapLauncher
{
  AutoMapLauncher(void) : actuatorType(0), value(0), side(-1), actuator(AUTOMAP_NO_RECORD)
  {
  }
  int actuatorType;
  int value;
  int side;
  AutoMapRecordId actuator;
};

// What the actuator sends its target: the engine's ACTUATORACTION numbering
// (SET opens, CLEAR closes, TOGGLE flips, CONSTPRESSURE holds while pressed)
enum AutoMapSignal
{
  AutoMapSignalSet = 0,
  AutoMapSignalClear = 1,
  AutoMapSignalToggle = 2,
  AutoMapSignalPressure = 3
};

// What kind of relay record sits in a cell.
enum AutoMapRelayKind
{
  AutoMapRelayCounter,
  AutoMapRelayGate,
  AutoMapRelayDSA
};

// One counter (actuator type 48, or type 6 in stone), type-5 And/Nor gate or DSA
// (type 47) record, kept as its own entry so the press effect can follow each
// one to its own target. For a gate the low nibble of value is the current bit
// state, the high nibble the match pattern (Timer.cpp stone-room handling);
// localActionType is >= 0 for a local-action gate (word4 bit 11), which has no
// target of its own and fires only when a message reaches the cell.
struct AutoMapRelay
{
  AutoMapRelay(void)
    : kind(AutoMapRelayCounter), value(0), signal(AutoMapSignalSet),
      normallyClosed(false), onceOnly(false), delay(0), edgeMode(0),
      localActionType(-1), record(AUTOMAP_NO_RECORD)
  {
  }
  AutoMapRelayKind kind;
  // Counter remainder 0..511; gate low nibble bits | pattern << 4; 0 for a DSA
  int value;
  // DB3 action() & 3
  AutoMapSignal signal;
  bool normallyClosed;
  bool onceOnly;
  int delay;
  // DB3 andOrEdge(); 0 when the extended features are off
  int edgeMode;
  // DB3 localActionType() when localActionOnly(), else -1
  int localActionType;
  AutoMapRecordId record;
};

struct AutoMapCellRecord
{
  AutoMapCellRecord(void);
  AutoMapCellKind kind;
  // Rock the reader added around the level (boundaryMarginX/Y) to hold the walls the engine
  // fabricates past an open edge. Drawn like any wall; the tooltip names it as added rock.
  bool isOutsideLevel;
  // A spinner that sets an absolute exit facing (DB1 facing mode): an AutoMapDirection,
  // or -1 when the spin is relative and the exit facing unknown
  int spinnerFacing;
  // Where a firing teleporter (visible or invisible) sends its victim: a level index
  // with cell coordinates. Dormant teleporters, spinners (live or dormant) and plain
  // floors send nowhere.
  bool hasTeleporterTarget;
  int teleporterDestLevel;
  int teleporterDestX;
  int teleporterDestY;
  // The destination in the dungeon's global coordinates (the ruler frame):
  // local coordinates plus the destination level's offset. -1 when there is
  // no firing teleporter here.
  int teleporterDestGlobalX;
  int teleporterDestGlobalY;
  // Who the teleporter takes, the DB1 scope (0 = objects only, 1 = monsters only,
  // 2 = party or objects, 3 = anything). -1 when there is no firing teleporter here.
  int teleporterScope;
  // How a firing teleporter (visible or invisible) turns its victim: the DB1
  // rotation (0 = straight, 1 = right, 2 = around, 3 = left) and facing mode
  // (1 = absolute face, 0 = relative turn). -1 when there is no firing
  // teleporter here.
  int teleporterRotation;
  int teleporterFacingMode;
  // The relative turn of a spinner (a teleporter targeting its own cell),
  // even when the spin is relative and spinnerFacing stays -1. -1 when this
  // cell is no spinner (live or dormant).
  int spinnerRotation;
  // A spinner whose field the 3D view draws: it looks like a teleporter until it was used
  bool spinnerIsVisible;
  // Pit or floor decoration
  int floorPicture;
  // True when a pit cell looks like floor in the 3D view: closed (raw room
  // type PIT but the summary reports roomOPEN), or open with the obscure bit
  // 0x04 set (concealed look). Such pits get a dashed marker on the map, where
  // the bare sprite alone reads as floor at low zoom.
  bool pitIsHidden;
  // True when the pit is open (cell-flags bit 0x08): victims fall through.
  // False for closed pits, which look like floor but can open.
  bool pitIsOpen;
  // A normal switchable pit while closed: the reader draws it as floor, the way the 3D
  // view shows it, but a message can open it. Only All keeps the flag, so a press names
  // the pit and the map gives it a dashed outline; below All it is floor.
  bool pitIsClosedSwitchable;
  // A false wall drawn as stone that the party walks through all the same (cell-flags bit
  // 0x01, an imaginary wall). Only All keeps the flag; below All it is a wall.
  bool falseWallIsPassable;
  // Doors and stairs: the direction of travel
  bool runsNorthSouth;
  AutoMapDoorState doorState;
  // Whether the door carries its own clickable switch (DB0 bit 6): the 3D view
  // draws the switch and an empty-handed click opens the door (Mouse.cpp).
  bool doorHasSwitch;
  // Which tools break the door, from the DB0 record: axe, fireball, both or neither
  bool doorBreakableWithAxe;
  bool doorBreakableWithFireball;
  // The least force a blow or fireball must reach to break the door (HitDoor compares
  // it with the low byte of the level's DoorTOC entry for the door type); -1 unknown
  int doorBreakForce;
  bool stairsLeadUp;
  // Walls: indexed by AutoMapDirection
  AutoMapWallSide wallSides[AUTOMAP_SIDE_COUNT];
  std::vector<AutoMapItem> itemPictures;
  // Record and description of each itemPictures entry, parallel to it as the reader fills it.
  // Empty when the cell was not built by the reader.
  std::vector<AutoMapFloorItemTag> itemTags;
  // Per-chest contents from the reader: item indexes itemPictures as filled (the full,
  // pre-truncation list), pictures are the contained items' sprites in chain order.
  // The tooltip resolves entries against allItemPictures, so chests past the display
  // truncation still show their contents.
  std::vector<AutoMapChestContents> chestContents;
  // Alcove contents, wall-lock requirements and goods a local gate will reveal
  // from the reader, each tagged with the side they belong to. Hover-only: walls
  // draw no floor items, so these never reach the static layer; the tooltip
  // merges them with the floor pile below. The fog filter trims them to the
  // sides seen so far.
  std::vector<AutoMapAlcoveEntry> alcoveContents;
  std::vector<AutoMapWallLock> wallLocks;
  // Counters (actuator type 48, or type 6 in stone which counts in a closed
  // room and generates in an open one), type-5 gates and DSAs (type 47) embedded
  // in this wall/stone cell, one entry per record in record order; empty when
  // none. Doors are left out: their counters are inert (ProcessTT_DOOR walks no
  // actuators). Floor counters live in drawList.actuators instead; walls never
  // reach that list, so the relay behind e.g. a coin slot would otherwise stay
  // undescribed. Several records can share a cell and feed different targets (DM
  // L5's (18,28) starts one counter at 4 for the gate and one at 3 for the door).
  // Rebuilt every tick, so an inserted coin ticks the counts down and switches
  // flip the gate bits.
  std::vector<AutoMapRelay> wallRelays;
  // Missile launchers in this wall/stone cell, in record order (launchers only
  // operate in stone: Timer.cpp stone-room handling). Rebuilt every tick, so
  // once-only launchers vanish once spent.
  std::vector<AutoMapLauncher> wallLaunchers;
  // Excerpts of scroll texts lying on this floor cell, in chain order.
  // Like items they persist under fog once the cell is explored.
  std::vector<std::string> scrollTexts;
  // The scroll record behind each scrollTexts entry, parallel to it.
  std::vector<AutoMapRecordId> scrollRecords;
};

struct AutoMapMonsterKind
{
  int frontPicture;
  bool hasSidePicture;
  bool hasBackPicture;
  int transparentColour;
};

struct AutoMapMonsterRecord
{
  int x;
  int y;
  AutoMapMonsterKind kind;
  int memberCount;
  AutoMapDirection memberFacings[AUTOMAP_LARGEST_GROUP];
  // Current hit points per group member in group order (DB4 hitPoints6).
  // No max exists per instance: spawn max is not stored after damage.
  std::vector<int> memberHealth;
  // Carried items in possession-chain order, from the reader. Shown
  // nested under the group in the tooltip; fog-filtered with the monster itself.
  // possessionTexts runs parallel to possessionPictures: the scroll excerpt of
  // each carried scroll, "" for everything else.
  std::vector<AutoMapItem> possessionPictures;
  std::vector<std::string> possessionTexts;
  // Death-residue preview from the monster-droppings table (what killing the
  // group spawns): guaranteed drops vs 50%-chance drops (0x8000 entries).
  std::vector<AutoMapItem> residuePictures;
  std::vector<AutoMapItem> chanceResiduePictures;
  // The group's record, which the 3D view's measurement names it by
  AutoMapRecordId record = AUTOMAP_NO_RECORD;
};

// A projectile in flight: the item is what the 3D view draws, so thrown stones and
// flying fireballs look the way they do in the game. Thrown weapons keep their
// inventory tile and name; spell fire carries the world picture only. Spell fire
// shrinks as its range runs out, the way the 3D view shrinks it; thrown stones
// keep their size.
struct AutoMapMissileRecord
{
  int x;
  int y;
  AutoMapItem item;
  int rangeRemaining;
  bool sizeShowsRange;
};

// A lingering cloud: poison, fire remnants and the like, with its 3D view picture
struct AutoMapCloudRecord
{
  int x;
  int y;
  int picture;
};

// A floor actuator the 3D view may or may not show: pressure pads (of any operator type),
// dormant ones that cannot fire, monster generators (spent ones included), and the
// remaining contraptions (counters, DSAs) as a generic cog
enum AutoMapActuatorKind
{
  AutoMapActuatorPad,
  AutoMapActuatorDormantPad,
  AutoMapActuatorGenerator,
  AutoMapActuatorSpentGenerator,
  AutoMapActuatorCog
};

// Who or what sets a pressure pad off, from the engine's actuator type and value
// (PressurePadOrAlcove): anything, party and/or monsters, the party facing a direction,
// a specific object lying on the pad, or a specific object carried by the party
enum AutoMapPadOperator
{
  AutoMapPadAnything,
  AutoMapPadPartyOrMonster,
  AutoMapPadParty,
  AutoMapPadPartyFacing,
  AutoMapPadObject,
  AutoMapPadMonster,
  AutoMapPadPossession
};

struct AutoMapFloorActuator
{
  AutoMapFloorActuator(int x_ = 0, int y_ = 0, AutoMapActuatorKind kind_ = AutoMapActuatorPad)
    : x(x_), y(y_), kind(kind_), op(AutoMapPadAnything),
      requiredFacing(-1), hasGeneratedMonster(false), generatedCount(-1),
      hasGeneratorHealth(false), levelMultiplier(-1), healthMultiplier(-1),
      minHealth(-1), maxHealth(-1), counterValue(-1), relay(), record(AUTOMAP_NO_RECORD)
  {
    generatedKind.frontPicture = AUTOMAP_NO_PICTURE;
    generatedKind.hasSidePicture = false;
    generatedKind.hasBackPicture = false;
    generatedKind.transparentColour = 0;
  }
  int x;
  int y;
  AutoMapActuatorKind kind;
  // Meaningful only when kind is AutoMapActuatorPad; every other kind keeps Anything
  AutoMapPadOperator op;
  // The required object type as the item the 3D view draws for it, for Object (on the pad)
  // and Possession (carried): unknown when the type resolves to nothing
  AutoMapItem object;
  // The facing the party must have, for PartyFacing: an AutoMapDirection, -1 when unknown
  int requiredFacing;
  // Which unit a live generator creates, as the monster pictures the 3D view draws.
  // Spent generators and every other contraption keep hasGeneratedMonster false.
  bool hasGeneratedMonster;
  AutoMapMonsterKind generatedKind;
  // Display name of the generated unit, resolved by the reader (custom dungeon
  // names included), "" when unresolvable. Engine-free builders print it as-is;
  // the tooltip font renders lowercase as uppercase.
  std::string generatedMonsterName;
  // How many units one firing creates: the actuator delay (Timer.cpp), or -1
  // when random (delay bit 3 set) or unknown. Shown only when > 1.
  int generatedCount;
  // Spawn health preview of one generated unit (Monster.cpp CreateMonster, Timer.cpp
  // generator firing: HP = M * BH + random 0..BH/4 with M = HM, or LEM when HM is 0).
  // True only for live generators whose range the reader could resolve; everything
  // else keeps it false and shows no health line.
  bool hasGeneratorHealth;
  // The level experience multiplier of the shown level (dungeon.dat), -1 when unknown
  int levelMultiplier;
  // The raw health multiplier of the generator (sensor properties), 0 meaning the
  // engine uses the level multiplier instead. -1 when unknown.
  int healthMultiplier;
  // Smallest and largest HP one firing can give a unit. Equal when BH < 4.
  int minHealth;
  int maxHealth;
  // Remaining count of a counter (actuator type 48, Timer.cpp OperateCounterActuator):
  // 0..511 when kind is Cog and the counter value is known, -1 for DSAs (type 47)
  // and for every other contraption. One firing counts once, so this ticks down
  // 2 -> 1 -> 0 as coins go in.
  int counterValue;
  // The relay behind a Cog: a counter (value == counterValue) or a DSA. Other
  // kinds keep the default entry.
  AutoMapRelay relay;
  // The actuator record behind this contraption.
  AutoMapRecordId record;
};

struct AutoMapParty
{
  int x;
  int y;
  AutoMapDirection facing;
};

// One directed action-link: the actuator on (srcX, srcY) points at (dstX, dstY).
// Wall switches name the side they sit on; floor contraptions use the cell centre.
struct AutoMapLink
{
  AutoMapLink(int srcX_ = 0, int srcY_ = 0, int dstX_ = 0, int dstY_ = 0,
              bool srcIsWall_ = false, int srcSide_ = -1,
              AutoMapSignal signal_ = AutoMapSignalSet, bool onceOnly_ = false,
              int delay_ = 0, bool fromCounter_ = false,
              int targetPos_ = -1, int gatePattern_ = -1)
    : srcX(srcX_), srcY(srcY_), dstX(dstX_), dstY(dstY_), srcIsWall(srcIsWall_),
      srcSide(srcSide_), signal(signal_), onceOnly(onceOnly_),
      delay(delay_), fromCounter(fromCounter_),
      targetPos(targetPos_), gatePattern(gatePattern_), source(AUTOMAP_NO_RECORD),
      sourceNormallyClosed(false)
  {
  }
  int srcX;
  int srcY;
  int dstX;
  int dstY;
  bool srcIsWall;
  int srcSide;
  AutoMapSignal signal;
  // Fires once, then the engine deletes the actuator: a dashed, one-shot arrow
  bool onceOnly;
  // Ticks the engine waits before the message lands (DB3 delay, 0..15 at ~6
  // ticks/s): a delayed close reads "AFTER 1S" in the tooltip.
  int delay;
  // The source actuator is itself a counter (type 48, or type 6 in stone):
  // it only fires when its count reaches zero, so its trigger reads "WHEN 0".
  bool fromCounter;
  // Target position (0..3) of the message: the wall part for stone targets, the
  // bit number for type-5 gate targets. -1 when unset (hand-built links).
  int targetPos;
  // Match pattern (0..15, the gate's high nibble) when the source actuator is
  // itself a type-5 gate: it fires on bit equality, so its trigger reads
  // "WHEN MATCH 1111". -1 otherwise.
  int gatePattern;
  // The actuator record this link comes from.
  AutoMapRecordId source;
  // The source actuator is normally closed (DB3 word4 bit 5): pressing it opens it, so a
  // CONSTPRESSURE source reports CLEAR on a press and SET on release (PressurePadOrAlcove,
  // TouchWall), the way DM L5's alcoves count their counters down.
  bool sourceNormallyClosed;
};

// One level as it is right now, secrets left out the way the 3D view leaves them out
struct AutoMapLevelSnapshot
{
  AutoMapLevelSnapshot(void);
  int level;
  int width;
  int height;
  // The level's offset in the dungeon's global coordinates: the global position of the
  // display cell (x, y) is (x + offsetX, y + offsetY). Unlike the level descriptors, the
  // snapshot can be grown on the west and north by boundaryMarginX/Y, so a cell's position
  // in the snapshot is its position in the dungeon plus those margins.
  int offsetX;
  int offsetY;
  // Rock the reader added on the west and north sides so the fabricated boundary walls
  // (the cells GetCellFlags invents just past an open edge, carrying the random
  // decorations the 3D view shows there) become real cells with somewhere to draw their
  // features. A side whose edge is solid stone needs no ring, so the reader adds a column
  // or row only where it is needed; these hold the west and north margins (0 or 1) that
  // shift the real cells. The east and south margins need no field: they only add cells,
  // they do not move any.
  int boundaryMarginX;
  int boundaryMarginY;
  // Row by row: the cell at (x, y) is cells[y * width + x]
  std::vector<AutoMapCellRecord> cells;
  // The picture, icon and name of every object description of the dungeon, so an item the
  // party remembers can be drawn after it left the cell. The default for charges and the like.
  std::map<int, AutoMapItem> itemOfDescIndex;
  std::vector<AutoMapMonsterRecord> monsters;
  std::vector<AutoMapMissileRecord> missiles;
  std::vector<AutoMapCloudRecord> clouds;
  std::vector<AutoMapFloorActuator> actuators;
  // Cells an actuator points at: walls (and doors) among them get an action-link badge.
  // Targets name no level and always sit on the level the snapshot shows.
  std::vector<AutoMapCell> linkedCells;
  // The directed pairs behind the badge set: one entry per actuator-to-target edge.
  // Deduped on the full edge (same source and target); distinct sources sharing one
  // target each keep their own entry for reverse highlight.
  std::vector<AutoMapLink> links;
  AutoMapParty party;
  // False when the snapshot shows a level the party is not on: the party marker is hidden
  // and the zoomed view centres on the middle of the level instead
  bool partyIsOnShownLevel;
  // The engine repaints two palette entries for every level from the monster types on it
  // (ReadGraphicsForLevel): the colour schemes the sprites of this level are drawn with
  int paletteScheme9;
  int paletteScheme10;
};

struct AutoMapMonsterPicture
{
  int picture;
  bool isMirrored;
};

// side: the direction from the wall cell to the floor in front of the face (AutoMapDirection)
struct AutoMapGlimpsedFace
{
  int x;
  int y;
  int side;
};

struct AutoMapShownCell
{
  int x;
  int y;
  AutoMapCellRecord cell;
  bool hasMoreItems;
  // The full pile for the hover tooltip: cell.itemPictures is truncated to
  // AUTOMAP_MOST_ITEMS_SHOWN, this one is not. Only present when the cell
  // itself survived the fog filter, so an absent cell still means "show nothing".
  std::vector<AutoMapItem> allItemPictures;
  bool isHighlighted;
  // The teleporter target may be shown from here: same-level targets sit on a filtered
  // cell, cross-level ones are explored (or the fog is off). Showing it otherwise would
  // leak the dungeon, invisibly-teleporting victims included.
  bool teleporterTargetVisible;
  // The player's own pit mark on this cell: -1 none, 0 closed, 1 open. Drawn on top of the cell,
  // which it never changes; shown at every spoiler level.
  int playerPit;
};

// One icon slot of the hover tooltip: identical items grouped with a count.
// Identity is the full item (world picture, inventory tile, name): a skeleton key
// and a Ra key share their world picture but never group together.
struct AutoMapItemStack
{
  AutoMapItem item;
  int count;
};

// Groups identical items in first-seen order for the icon tooltip.
std::vector<AutoMapItemStack> AutoMapScene_GroupItems(const std::vector<AutoMapItem> &items);

// What kind of thing an item is, for the map's show/hide choices. Read from the object
// description, whose ranges the engine fixes (data.cpp): scroll 0, chest 1, potions 2..22,
// weapons 23..68, clothing 69..126, everything else from 127. Among those, the name index
// (Objects.h) picks out food (Apple..Dragon Steak) and keys (Iron Key..Master Key).
enum AutoMapItemKind
{
  AutoMapItemScroll,
  AutoMapItemChest,
  AutoMapItemPotion,
  AutoMapItemWeapon,
  AutoMapItemArmour,
  AutoMapItemOther,
  AutoMapItemFood,
  AutoMapItemKey,
  AUTOMAP_ITEM_KIND_COUNT
};

static const unsigned AUTOMAP_ALL_ITEM_KINDS = (1u << AUTOMAP_ITEM_KIND_COUNT) - 1;

// Whether a floor picture is mere dressing (grate, moss, flame pit, crack, puddle) rather than a
// pressure pad, for hiding floor decorations. Floor ornament n draws picture 247 + 6 n + 5 (the
// nearest view, Code390e.cpp); 1, 4 and 7 are the square, round and tiny pads. Pits and
// ornaments past the standard nine are not dressing.
bool AutoMapScene_FloorPictureIsFlavour(int floorPicture);

// An item of unknown type counts as Other
AutoMapItemKind AutoMapScene_KindOfItem(const AutoMapItem &item);
// The items whose kind bit (1 << kind) is set in kinds, in pile order
std::vector<AutoMapItem> AutoMapScene_ItemsOfKinds(const std::vector<AutoMapItem> &items,
                                                   unsigned kinds);

struct AutoMapShownMonster
{
  int x;
  int y;
  int transparentColour;
  int memberCount;
  AutoMapMonsterPicture memberPictures[AUTOMAP_LARGEST_GROUP];
  AutoMapDirection memberFacings[AUTOMAP_LARGEST_GROUP];
  // Copied through like possessions at All: visible exactly when the group is. Empty below All,
  // where the hit points are a spoiler.
  std::vector<int> memberHealth;
  // Below All, one value per member slot (memberCount): the damage the game showed, 0 for none.
  // Empty at All, where memberHealth shows instead.
  std::vector<int> memberDamage;
  // Carried items, copied through from the record: visible exactly when the group is.
  // possessionTexts runs parallel to possessionPictures, as in the record.
  std::vector<AutoMapItem> possessionPictures;
  std::vector<std::string> possessionTexts;
  // Death-residue preview, copied through like possessions.
  std::vector<AutoMapItem> residuePictures;
  std::vector<AutoMapItem> chanceResiduePictures;
};

struct AutoMapShownMissile
{
  int x;
  int y;
  AutoMapItem item;
  int rangeRemaining;
  bool sizeShowsRange;
};

// How large a spell missile with this range draws, from fresh (1.0) down to the floor the
// 3D view never shrinks past. Thrown stones always draw at 1.0.
float AutoMapScene_MissileScale(int rangeRemaining);

struct AutoMapShownCloud
{
  int x;
  int y;
  int picture;
};

struct AutoMapDrawList
{
  AutoMapDrawList(void);
  int level;
  int width;
  int height;
  // The level's global offset, copied from the snapshot; see AutoMapLevelSnapshot
  int offsetX;
  int offsetY;
  // The snapshot's margins, copied too: a cell of the list less these is the level cell the
  // player's notes are kept under
  int boundaryMarginX;
  int boundaryMarginY;
  std::vector<AutoMapShownCell> cells;
  std::vector<AutoMapShownMonster> monsters;
  std::vector<AutoMapShownMissile> missiles;
  std::vector<AutoMapShownCloud> clouds;
  std::vector<AutoMapFloorActuator> actuators;
  std::vector<AutoMapCell> linkedCells;
  std::vector<AutoMapLink> links;
  // The player's own links whose two ends are both shown, in snapshot coordinates. Kept apart
  // from links: a note is never a learned link and no press, relay or chain logic reads it.
  std::vector<AutoMapLink> playerLinks;
  // Cells (x, y) where something was glimpsed in the dark: a "?", whether or not the cell
  // itself is in the list
  std::vector<AutoMapCell> glimpsedItems;
  // Wall faces where something was glimpsed in the dark: a "?" on that side of (x, y), whether
  // or not the wall itself is in the list
  std::vector<AutoMapGlimpsedFace> glimpsedFaces;
  AutoMapParty party;
  bool partyIsOnShownLevel;
  int paletteScheme9;
  int paletteScheme10;
  // The spoiler level the list was filtered at, copied from the fog. The tooltips and the
  // window skip what only the full dungeon data can tell (a sealed door) below All.
  AutoMapSpoilerLevel spoilers;
};

// Declared here for the fog, which carries the remembered looks (AutoMapCellMemory.h includes this
// header, so it cannot be included back)
class AutoMapCellMemory;
class AutoMapPlayerNotes;

struct AutoMapFog
{
  bool isOn;
  const AutoMapExplored *explored;
  const AutoMapVisibleCells *visibleNow;
  // Tells the cells already seen from the rest of a level shown whole
  bool highlightsExplored;
  // Last, so aggregate initialisation with fewer fields keeps today's map: All, no knowledge.
  // NULL knowledge is allowed at All and Seen; Seen never reads it. At Learned, NULL knowledge
  // means nothing is known.
  AutoMapSpoilerLevel spoilers;
  const AutoMapKnowledge *knowledge;
  // Below All with the fog on, a floor cell shows the items remembered on it instead of the live
  // pile. NULL keeps the live pile. Last, like knowledge.
  const AutoMapItemMemory *items;
  // Below All with the fog on, a monster group shows exactly when its record is in this list, the
  // groups the 3D view's last frame showed enough of. NULL keeps the ray rule (visibleNow). Last,
  // like items.
  const std::vector<AutoMapRecordId> *monstersSeenNow;
  // Below All with the fog on, an explored cell shows the look the party last saw (door, pit,
  // floor and wall pictures) instead of the live one. NULL keeps the live map. Last, like
  // monstersSeenNow.
  const AutoMapCellMemory *cells;
  // The player's own marks and links, shown at every spoiler level. NULL shows none. Last, like
  // cells.
  const AutoMapPlayerNotes *notes;
  // The damage the game showed per group, for the tooltip below All. NULL shows none. Last, like
  // notes.
  const AutoMapMonsterDamage *damage;
  // Explored, live: with the fog on, explored cells show their live look and pile (below All
  // only the items the party has seen somewhere, so it needs items), and what
  // moves on them shows on every explored cell, as if the party still saw it there. Ignored
  // with the fog off. Last, like damage.
  bool isLive;
};

// Every live floor item on an explored cell goes into the memory, the rule of version-2 map
// files, which did not record what was seen. Cells in the snapshot's coordinates.
void AutoMapScene_RememberExploredItems(const AutoMapLevelSnapshot &snapshot,
                                        const AutoMapExplored &explored, AutoMapItemMemory *memory);

// Every explored cell of the snapshot not yet in the memory takes its live look: the first
// impression of a cell explored outside a frame, and the migration of older map files. Cells
// the memory knows are left alone. Cells in the snapshot's coordinates.
void AutoMapScene_RememberExploredLooks(const AutoMapLevelSnapshot &snapshot,
                                        const AutoMapExplored &explored, AutoMapCellMemory *memory);

// The map is looked at from the south, so a monster facing south shows its front
AutoMapMonsterPicture AutoMapScene_ChooseMonsterPicture(const AutoMapMonsterKind &kind,
                                                        AutoMapDirection facing);

// The fog filter. Fog off: the whole level, on request with every explored cell that is not a
// wall highlighted. Fog on: explored cells, the features of a wall side only when the floor cell
// in front of that side is explored, monsters, missiles and clouds only on cells the party sees
// right now, and floor actuators on every explored cell, since they do not move (below All only
// the learned ones). A link survives only when both its ends survived the cell
// filter and its source actuator survived the actuator filter, so hovering near secrets leaks
// nothing. The badge set passes through whole: badges only ever draw on cells the filter let
// through. Explored, live (fog on with isLive) keeps the explored cells of the fog but draws
// them live: no remembered look, and below All the live items the party has seen somewhere.
// The spoiler level strips secrets from the cells. All changes nothing. Below All a wall face
// keeps its features only once the 3D view has shown that face (every face counts as shown with
// the fog off, a door's side once the floor cell in front of it is explored); relay machinery (counters, DSA, gates) and the breaking data of doors go; a
// false wall reads as a wall, a hidden pit and an invisible or dormant teleporter or spinner as
// floor, and a visible teleporter loses its destination. Seen strips for good. Learned lets a
// secret come back once the knowledge holds the fact that exposes it: the actuator fired (a
// switch works, a launcher shows), the lock opened for an item, the false wall walked
// through, the pit fallen into, the teleporter or spinner used.
// Below All floor actuators show only once fired, stripped of their condition (counters and
// dormant pads never); links are the ones learned, one arrow from the actuator to the cell that
// changed; chest contents, also of a chest in an alcove, need the chest opened, scroll text the
// scroll read; monsters carry
// nothing and show no death residue.
AutoMapDrawList AutoMapScene_BuildDrawList(const AutoMapLevelSnapshot &snapshot,
                                           const AutoMapFog &fog);

// The cell's look: kind, door position, pit state, whether a spinner's field shows, floor
// picture and the picture on each side
struct AutoMapCellLook
{
  AutoMapCellKind kind;
  AutoMapDoorState doorState;
  bool pitIsOpen;
  bool pitIsHidden;
  bool spinnerIsVisible;
  int floorPicture;
  int sidePictures[AUTOMAP_SIDE_COUNT];
};

AutoMapCellLook AutoMapScene_LookOf(const AutoMapCellRecord &record);
bool AutoMapScene_LooksDiffer(const AutoMapCellLook &a, const AutoMapCellLook &b);
// Overwrites exactly the look's fields of record
void AutoMapScene_ApplyLook(const AutoMapCellLook &look, AutoMapCellRecord *record);

// Whether a cell looks different to the eye: its kind, door position, pit state (open or
// concealed), whether a spinner's field shows, floor picture or the picture on any wall side.
// Things lying on it do not count.
bool AutoMapScene_CellLooksChanged(const AutoMapCellRecord &before, const AutoMapCellRecord &after);

// What a sighting shows of a cell: a concealed pit (pitIsHidden) reads as floor whether open or
// closed until the party knows it, so its open state stays the remembered one
AutoMapCellLook AutoMapScene_VisibleLook(const AutoMapCellLook &seen,
                                         const AutoMapCellLook *remembered, bool pitIsKnown);
// A change the eye sees: CellLooksChanged, except a concealed pit's open state while unknown
bool AutoMapScene_SeenToChange(const AutoMapCellRecord &before, const AutoMapCellRecord &after,
                               bool pitIsKnown);

// Hover queries over the filtered draw list: outgoing edges of a source cell, incoming edges
// of a target cell. Both return in snapshot order.
std::vector<AutoMapLink> AutoMapScene_LinksFrom(const AutoMapDrawList &drawList, int x, int y);
std::vector<AutoMapLink> AutoMapScene_LinksTo(const AutoMapDrawList &drawList, int x, int y);

// Global ruler coordinates of a level cell: local position plus the level's offset.
// Always defined, even for unexplored cells: coordinates are no secret, the rulers show them.
void AutoMapScene_GlobalPosition(const AutoMapDrawList &drawList, int x, int y,
                                 int *globalX, int *globalY);

// The player's own notes on a cell for the tooltip: "YOUR MARK: HIDDEN PIT OPEN" (or CLOSED), then
// one "YOUR LINK: L<level> (<x>, <y>)" per player link from the cell, any side. Empty when the
// cell is not shown.
std::vector<std::string> AutoMapScene_PlayerNoteLines(const AutoMapDrawList &drawList, int x, int y);

// Whether the player may put a note on (x, y) of the list: a pit mark, either end of a link. Only
// a shown cell of the level itself: the rim rock the reader adds around it (isOutsideLevel) lies
// outside the level, where no note can be kept. That the rim is outside is no secret, the tooltip
// says so.
bool AutoMapScene_CellTakesNotes(const AutoMapDrawList &drawList, int x, int y);

// The wall face a pointer inside a cell is nearest to: offsetX and offsetY run 0..cellSize-1 from
// the cell's top-left. Returns an AutoMapDirection; ties go to north, then east, south and west.
int AutoMapScene_NearestSide(int offsetX, int offsetY, int cellSize);

// Collapses newlines to spaces, trims and caps at 180 characters with "...".
// Engine-free so the reader and the tests share it.
std::string AutoMapScene_MakeExcerpt(const std::string &text);

// Fog-safe text excerpts for the hover tooltip: empty when the cell is absent
// from the draw list (unexplored). Floor, alcove, chest and monster-carried
// scrolls aggregated; scroll texts persist like items, a wall side's text
// survives only with its side.
std::vector<std::string> AutoMapScene_ScrollTexts(const AutoMapDrawList &drawList,
                                                  int x, int y);
struct AutoMapWallText
{
  int side;
  std::string text;
};
std::vector<AutoMapWallText> AutoMapScene_WallTexts(const AutoMapDrawList &drawList,
                                                   int x, int y);

// Fog-safe tooltip source: empty when the cell is absent from the draw list
// (unexplored), the full grouped pile otherwise. Fogged-but-visited cells keep
// their items, so they keep their tooltip; monsters/missiles/clouds/actuators
// stay separate (visibleNow-only) and are not part of this list.
std::vector<AutoMapItemStack> AutoMapScene_TooltipItems(const AutoMapDrawList &drawList,
                                                        int x, int y);

// One display slot of the tile tooltip. Chests never group: each keeps its own
// slot with its grouped contents, so different chests stay distinguishable.
struct AutoMapTooltipSlot
{
  AutoMapTooltipSlot(void) : count(1), isChest(false), side(-1), required(false),
                              carried(false)
  {
  }
  AutoMapItem item;
  int count;
  bool isChest;
  // The wall side this entry belongs to (an AutoMapDirection), or -1 for floor
  // goods, chests, figures and everything else without a side. Alcove contents
  // and lock requirements group per side, so a count always means identical
  // items on the same side rather than across the whole cell.
  int side;
  // A lock requirement is a need, not a have: identical items never merge with
  // present ones, and the window labels them NEEDS.
  bool required;
  // For a required pad slot (side == -1): true when the item must be carried
  // (Possession, "NEEDS CARRIED"), false when it must lie on the pad (Object,
  // "NEEDS ON PAD"). Wall locks keep false and read as bare NEEDS with a side.
  bool carried;
  std::vector<AutoMapItemStack> contents;
  // Predicted rather than present: death-residue chance drops nest without an
  // outline, guaranteed contents with one.
  std::vector<AutoMapItemStack> uncertainContents;
};

// Display-order slots for the tile tooltip: floor items grouped first-seen, each
// chest ungrouped with its contents grouped beneath it, pad requirements
// (CARRY always, OBJECT only when missing) as NEEDS slots that never merge
// with present items. Empty when the cell is absent from the draw list.
std::vector<AutoMapTooltipSlot> AutoMapScene_TooltipSlots(const AutoMapDrawList &drawList,
                                                           int x, int y);

// Grouped carried items of the monster groups on a cell, in chain order.
// Fog-safe like everything else here: only groups the draw list shows.
std::vector<AutoMapItemStack> AutoMapScene_MonsterPossessions(const AutoMapDrawList &drawList,
                                                              int x, int y);

// Tooltip lines of the monsters on (x, y) in group order: "HP 42" per member at All; below All
// "DMG 17" per member with damage, none for a member not hit yet. Fog-safe like everything else
// here.
std::vector<std::string> AutoMapScene_MonsterHealthLines(const AutoMapDrawList &drawList, int x, int y);

// Grouped death-residue preview of the monster groups on a cell: guaranteed
// drops vs 50%-chance drops. Same fog rule as possessions.
struct AutoMapMonsterDrops
{
  std::vector<AutoMapItemStack> certain;
  std::vector<AutoMapItemStack> chance;
};
AutoMapMonsterDrops AutoMapScene_MonsterDrops(const AutoMapDrawList &drawList, int x, int y);

// Name lookup for trigger lines: the engine's object name for a type id,
// "" when unknown. The window passes AutoMapLevelReader_ItemName; the tests
// pass a fake. Keeps the builders engine-free.
typedef const char *(*AutoMapItemNameFn)(int nameIndex);

// One item hit of the name search: the level and cell plus the tooltip slot
// it came from (floor / chest / alcove / NEEDS, or a monster-carried item),
// with the resolved display name. The draw list is already fog-filtered, so
// searching it never leaks unexplored cells: unexplored simply has no cell
// to search. Monsters only ever reach the draw list on cells the party sees
// right now, so carried hits vanish again once the carrier walks off.
struct AutoMapSearchHit
{
  AutoMapSearchHit(void) : level(AUTOMAP_NO_LEVEL), x(-1), y(-1), globalX(0), globalY(0),
                           monsterCarried(false), isFood(false), wallFeature(AutoMapWallFeatureNone)
  {
  }
  int level;
  // The cell in the draw list; globalX, globalY: the same cell as the rulers number it
  int x;
  int y;
  int globalX;
  int globalY;
  AutoMapTooltipSlot slot;
  std::string name;
  bool monsterCarried;
  // The item is food, so it also matches the word FOOD
  bool isFood;
  // A wall face rather than an item: slot.side is the face, name the feature's
  AutoMapWallFeature wallFeature;
};

// The name a wall feature is searched and listed by: "WATER FOUNTAIN", "ALTAR OF VI", "" for none
const char *AutoMapScene_WallFeatureName(AutoMapWallFeature feature);

// Match quality of a resolved item name against a query: 0 exact, 1 prefix,
// 2 substring, -1 no match. Case-insensitive; blank queries and unknown names
// ("", "(UNKNOWN)") never match. Shared by the single-level search and the
// dungeon-wide ranking so both order hits the same way.
int AutoMapScene_SearchRank(const std::string &name, const std::string &query);

// True when the object type is one the champions eat
typedef bool (*AutoMapItemIsFoodFn)(int nameIndex);

// Match quality of a hit: the better of its name's rank and, for food, the rank of the word
// FOOD. -1 when neither matches. Every sort of hits uses it.
int AutoMapScene_SearchHitRank(const AutoMapSearchHit &hit, const std::string &query);

// The one order of the search results: rank, level, y, x; on one cell the items before the wall
// features, then by name, lying before carried, and by wall side. Hits on one cell would
// otherwise swap places whenever the list is rebuilt.
void AutoMapScene_SortSearchHits(std::vector<AutoMapSearchHit> *hits, const std::string &query);

// Name search over one filtered draw list. Empty or blank queries return
// nothing. Matching is case-insensitive substring on the resolved item name;
// unknown names never match. Covers floor piles, chests, alcoves, NEEDS
// requirements and monster-carried items. Rank: exact first, then prefix,
// then substring; ties in AutoMapScene_SortSearchHits order. A food item
// (isFood, NULL for none) also matches the word FOOD; its rank is the better of
// its name's and the word's, and it is listed once, under its own name. Each
// shown wall face with a feature is a hit too, after the items of its cell.
std::vector<AutoMapSearchHit> AutoMapScene_SearchItems(const AutoMapDrawList &drawList,
                                                       const std::string &query,
                                                       AutoMapItemNameFn nameForIndex,
                                                       AutoMapItemIsFoodFn isFood);

// Spawn health range of one unit a generator creates (Monster.cpp CreateMonster,
// Timer.cpp generator firing): min = M * BH, max = min + BH / 4 (integer division)
// with M = healthMultiplier, or levelMultiplier when healthMultiplier is 0.
// Returns false when the range is unknown: negative base health, unknown health
// multiplier, or a level multiplier that is missing while needed (HM == 0).
// Engine-free so the reader and the tests share the engine formula.
bool AutoMapScene_GeneratorHealthRange(int levelMultiplier, int healthMultiplier,
                                       int baseHealth, int *minHealth, int *maxHealth);

// Tooltip footer lines for a resolved spawn range, one value per line:
// "HP RANGE 30-37" (or "HP RANGE 30" when min == max), preceded by
// "LEVEL EXPERIENCE MULTIPLIER 3", or by "HEALTH MULTIPLIER 5" and
// "LEVEL EXPERIENCE MULTIPLIER 3" when the generator overrides the level
// ("HEALTH MULTIPLIER 5" alone with the HP line when the level value is unknown).
// Empty when the range is unknown (negative health). Only uses the legend font glyphs.
std::vector<std::string> AutoMapScene_GeneratorHealthLines(int levelMultiplier,
                                                            int healthMultiplier,
                                                            int minHealth, int maxHealth);

// Text lines describing what sets the floor actuators on a cell off, in
// draw-list order with duplicates folded: "TRIGGER: PARTY",
// "TRIGGER: PARTY FACING N", "TRIGGER: OBJECT <name>",
// "TRIGGER: CARRY <name>", "DORMANT PAD", "GENERATES 3X <monster>"
// (plus its spawn range on separate lines: the multipliers and "HP RANGE",
// when resolved),
// "SPENT GENERATOR". Cogs (counters, DSAs) print nothing: they are drawn at All
// only, where AutoMapScene_RelayHoverLines names them. Empty when the cell is
// absent from the draw list (unexplored), so fog-filtered pads stay silent.
// Below All only pads the party has set off are listed, and a plain pad prints no line: what
// a pad wants is not found out by stepping on it. A known generator still reads "GENERATES ...".
std::vector<std::string> AutoMapScene_TriggerLines(const AutoMapDrawList &drawList, int x, int y,
                                                   AutoMapItemNameFn nameForIndex);

// Text lines for what a cell is, not what a press does (that is AutoMapScene_PressLines
// and AutoMapScene_RelayHoverLines): "ADDED ROCK: OUTSIDE THE LEVEL"; a stone-embedded
// missile launcher reads "LAUNCHES POISON CLOUD (N)" (ammunition and firing side, "2X"
// for paired types 9/10/15); a door that is not open, with no own switch, no incoming link
// and no axe/fireball way through reads "SEALED", at All only; a breakable door reads
// "BREAK FORCE: 60 (AXE, FIREBALL)". Counter, gate and DSA state, and where this
// cell's actuators point, belong to the press block at All and are not printed here.
// Below All the only lines are "TARGET: L<level> (<gx>, <gy>)" per outgoing link, plus
// " DOOR" for a door (the party saw the cell change, not how), and the rock line, which is no
// secret. Empty when the cell is absent from the draw list (unexplored); targets naming
// unexplored cells stay silent like the teleporter destination rule.
std::vector<std::string> AutoMapScene_RelayLines(const AutoMapDrawList &drawList, int x, int y);

// How far a goal is from happening, from the path of the press tree that reaches it:
// it happens on this press (or already has: OPEN on an open door), needs more presses
// of this source, needs other sources too, is blocked (a SET into a counter, a counter
// at 0, a once-only relay this press spent), only holds while a CONSTPRESSURE source or
// relay stays pressed, or runs through a DSA the walk does not simulate. Unknown on
// anything but a goal.
enum AutoMapGoalStatus
{
  AutoMapGoalUnknown,
  AutoMapGoalOnThisPress,
  AutoMapGoalMore,
  AutoMapGoalNeedsOthers,
  AutoMapGoalBlocked,
  AutoMapGoalHolds,
  AutoMapGoalThroughDSA,
  AutoMapGoalAlreadyDone
};

struct AutoMapGoalDistance
{
  AutoMapGoalDistance(void) : status(AutoMapGoalUnknown), more(-1), bitsOff(0), blocked(NULL)
  {
  }
  AutoMapGoalStatus status;
  // Presses of this source still needed after this one; -1 unless More
  int more;
  // Mismatched gate bits on the path after this press; 0 without a gate
  int bitsOff;
  // "COUNTER AT 0", "COUNTS UP", "SPENT"; NULL otherwise
  const char *blocked;
};

// One receiver a press reaches, after the engine's stone-room and open-room handling
// (Timer.cpp ProcessTT_STONEROOM, ProcessTT_OPENROOM, OperateCounterActuator):
// a counter or gate with its state before and after the message, a DSA (not
// simulated, so not followed), the goal behind the relays (door, false wall, pit,
// teleporter, spinner, generator, launcher, or a bare target), a local action (actuator rotation or
// skill bonus) or a loop back to a relay already on the path.
enum AutoMapEffectKind
{
  AutoMapEffectCounter,
  AutoMapEffectGate,
  AutoMapEffectDSA,
  AutoMapEffectGoal,
  AutoMapEffectLocalAction,
  AutoMapEffectLoop
};

struct AutoMapEffect
{
  AutoMapEffect(void)
    : kind(AutoMapEffectGoal), x(-1), y(-1), relay(-1), floorRelay(false),
      before(-1), after(-1), fires(false), later(false), afterDSA(false), delayTicks(0),
      target(false)
  {
  }
  AutoMapEffectKind kind;
  // The receiver cell, draw-list coordinates
  int x;
  int y;
  // Index into the cell's wallRelays, or into drawList.actuators when floorRelay;
  // -1 for goals and loops
  int relay;
  bool floorRelay;
  // The link whose message this is; a local action, which has no link, keeps the default
  AutoMapLink via;
  // Counter value or gate low nibble before and after the message; -1 otherwise
  int before;
  int after;
  // This receiver queues its own action as a result. For a later effect: would
  // queue it when that message arrives.
  bool fires;
  // The message arrives only once an upstream relay fires later, so before/after
  // start from the state this press leaves
  bool later;
  // A DSA precedes it in the cell and may stop the walk before it
  bool afterDSA;
  // Delays summed from the source to this message, in ticks
  int delayTicks;
  // Goals only: a bare target, a cell the message lands on with no receiver and no goal
  // the walk knows (a plain floor, stone without a launcher on the message's side). It
  // reads TARGET, so a link at All never goes silent.
  bool target;
  // Goals only
  AutoMapGoalDistance distance;
  std::vector<AutoMapEffect> next;
};

// The effects of one press on the hovered cell: a switch face, whose actuators one click
// fires together, or the pads of a floor cell, which one step fires together. A cell can
// hold several (switches on two faces), each pressed on its own.
struct AutoMapPressSource
{
  AutoMapPressSource(void)
    : source(AUTOMAP_NO_RECORD), srcIsWall(false), srcSide(-1), truncated(false)
  {
  }
  // The first actuator record of the face or cell, in link order
  AutoMapRecordId source;
  bool srcIsWall;
  int srcSide;
  std::vector<AutoMapEffect> effects;
  // The walk ran out of its budget (1000 effects per call) while on this source, so
  // the effects are incomplete
  bool truncated;
};

// What one press of each face (or the pads) of (x, y) does, down the whole relay chain:
// every receiver in a target cell in record order (stone wallRelays, or the floor cogs
// in drawList.actuators order on an open floor), then the goal of the cell. A press
// closes its source, so a CONSTPRESSURE source sends SET. Once the whole cell has the
// message, the relays that fired are followed through the links they send, earliest
// send first, and so are those that can still fire later (a counter above 0, a gate),
// marked later. The engine state is simulated along the walk, so a second message to
// one relay starts from what the first left, and a once-only relay that fired is inert
// afterwards. A relay already on the path reads as a loop and ends that branch (a dead
// counter just reads 0, and what it would send is followed as later once per path, so
// the goals behind it read blocked). Each goal carries its distance, worked out from
// its path once the source's tree is complete. Sources are grouped by face (srcIsWall,
// srcSide; all pads of a floor cell are one) in link order, and the links of one share
// its state. The walk stops after 1000 effects and marks the source it was on
// truncated. Links sent by the relays of (x, y) itself (stone relays, floor cogs) are no
// press and are left out: a relay cell reads through AutoMapScene_RelayHoverLines. Empty
// below All, or when (x, y) has no outgoing link with an explored target; unexplored
// cells are neither named nor followed.
std::vector<AutoMapPressSource> AutoMapScene_PressEffects(const AutoMapDrawList &drawList,
                                                          int x, int y);

// The tooltip lines for the effects of a press, from AutoMapScene_PressEffects. Compact
// (fullChain false) is a THIS PRESS block of the receivers the press reaches first and a
// PUZZLE block with every goal behind them and how far it is, then a SHIFT hint; fullChain
// is the whole tree, indented one space per depth, with delays, gate edge modes, once-only
// and normally-closed marks. Several sources get a header each (SWITCH (E):, PAD:). The
// font is ASCII: arrows read 4->3, folds 3X ..., a loop LOOPS BACK. At most
// AUTOMAP_PRESS_MAX_LINES lines below the headers in the compact view, the last one
// +N MORE (SHIFT); the hint is the first line to go. A source whose
// walk ran out of budget ends with CHAIN CUT SHORT. Empty without sources. fullChain
// lifts the cap as well: every line is kept (still folded), with no +N MORE.
const size_t AUTOMAP_PRESS_MAX_LINES = 10;
std::vector<std::string> AutoMapScene_PressLines(const AutoMapDrawList &drawList,
                                                 const std::vector<AutoMapPressSource> &sources,
                                                 bool fullChain);

// Every link in the trees of a press, depth first and without duplicates, for the map to
// light the whole chain. A local action has no link and adds none.
std::vector<AutoMapLink> AutoMapScene_PressChainLinks(const std::vector<AutoMapPressSource> &sources);

// Short goal badge for the map: NOW, 2 MORE, 1 BIT OFF, NEEDS OTHERS, BLOCKED, HOLD, DSA, DONE;
// empty for Unknown. Within the tooltip font.
std::string AutoMapScene_GoalBadge(const AutoMapGoalDistance &distance);

// Hovering a relay: the lettered relays of (x, y) in record order with what each feeds,
// then FED BY, the sources that reach the cell with what a press does to which letter
// (A-1 counts down, A+1 up, A BIT 2 TOGGLE flips a gate bit). Empty below All, for an
// unexplored cell, or when the cell holds no relay. At most AUTOMAP_PRESS_MAX_LINES lines
// below the headers, the last one +N MORE; full (Shift held) lifts that cap.
std::vector<std::string> AutoMapScene_RelayHoverLines(const AutoMapDrawList &drawList, int x, int y,
                                                     bool full);

// How a spinner reads on the map: an absolute spinner forces its exit facing
// no matter how the victim entered, a relative one turns it (right / around /
// left), a straight one keeps the facing, unknown covers missing rotation.
enum AutoMapSpinnerGlyph
{
  AutoMapSpinnerUnknown,
  AutoMapSpinnerAbsolute,
  AutoMapSpinnerRight,
  AutoMapSpinnerAround,
  AutoMapSpinnerLeft,
  AutoMapSpinnerStraight
};

// Engine-free geometry behind the spinner mark, so the window and the tests
// share it. Angles are in radians, 0 is east and PI/2 is north, matching the
// map's circle math (x = cx + r*cos, y = cy - r*sin). Absolute spinners draw
// a full ring with a radial head at headAngle; relative ones draw the arc
// from startAngle sweeping sweepAngle (negative runs clockwise) with a
// tangential head at headAngle; straight and unknown carry no head.
struct AutoMapSpinnerGeometry
{
  AutoMapSpinnerGlyph glyph;
  float startAngle;
  float sweepAngle;
  float headAngle;
};

AutoMapSpinnerGeometry AutoMapScene_SpinnerGeometry(int spinnerFacing, int spinnerRotation);

// Text lines describing a teleporter or spinner cell: who it takes
// ("TAKES: ..."), where it sends them ("DEST: ...", only when the target is
// visible under the arrow's rule so unexplored cells never leak; levels count
// from zero as in the title bar, coordinates are global as on the rulers)
// and what it does to their facing ("FACE ..." for an absolute facing,
// "TURN: ..." for a relative turn), or the spin ("SPIN: ...") for a spinner.
// A dormant teleporter names the same as a hypothetical ("DORMANT TELEPORTER"
// plus "WOULD ..." lines); one without a record reads "DORMANT, NO EFFECT".
// A dormant spinner reads "DORMANT SPINNER" plus its "WOULD SPIN: ..." line.
// Anything else reads nothing. Same fog rule as everything else here.
std::vector<std::string> AutoMapScene_TeleporterLines(const AutoMapDrawList &drawList,
                                                       int x, int y);

// Text line naming the state of every pit cell, so hovering always tells open
// from closed: "PIT OPEN"/"PIT CLOSED", or "HIDDEN PIT OPEN"/"HIDDEN PIT
// CLOSED" when the pit looks like floor in the 3D view (closed, or open with
// the obscure bit set). At All a closed switchable pit, drawn as floor, reads
// "PIT CLOSED" too. Anything else reads nothing. Empty when the cell is
// absent from the draw list (unexplored). Same fog rule as everything else here.
std::vector<std::string> AutoMapScene_PitLines(const AutoMapDrawList &drawList,
                                               int x, int y);

// Whether a door cell can never be opened normally: no own switch (DB0 bit 6),
// no incoming action-link, and no axe/fireball way through. Bashable counts as
// openable, so a sealed-but-bashable door keeps only its break marks. Destroyed
// doors are never sealed. Engine-free so the window and the tests share it;
// isLinked is the draw-list verdict (CellIsLinked), hence fog-filtered.
bool AutoMapScene_DoorIsSealed(const AutoMapCellRecord &cell, bool isLinked);

// The door position a signal would leave behind, the way ProcessTT_DOOR works it out:
// SET opens, CLEAR closes, TOGGLE heads for the other extreme, pressure shows the pressed
// (SET) outcome. Returns -1 when there is nothing to preview: destroyed doors ignore every
// signal, and a signal that changes nothing previews nothing.
int AutoMapScene_PredictDoorState(AutoMapDoorState current, AutoMapSignal signal);

struct AutoMapViewRequest
{
  // The size of the map area in pixels: the window without the coordinate rulers
  AutoMapLevelSize levelSize;
  int windowWidth;
  int windowHeight;
  int zoomSteps;
  // The view centre in cells, fractions included: the party while the view follows it,
  // the exact frozen centre once the user pans away. Fractions keep the same spot centred
  // across zoom steps, where integer cells plus pixels would jump.
  float centerX;
  float centerY;
};

// Cell (x, y) covers the square of cellSize pixels at (originX + x * cellSize, originY + y * cellSize)
struct AutoMapView
{
  int cellSize;
  int originX;
  int originY;
};

int AutoMapScene_ClampZoomSteps(int zoomSteps);

// Every how many cells a ruler shows a number, so that neighbouring numbers never touch:
// the smallest step whose cells are at least as wide as one number
int AutoMapScene_LabelStep(int cellSize, int numberPixelWidth);

// The cells a ruler must cover, inclusive: the ones touching the map area
void AutoMapScene_VisibleRange(int originPixels, int cellSize, int mapPixels, int levelCells,
                               int *first, int *last);

// A 3x5 bitmap font for the ruler numbers: true when the pixel in column col (0 is left)
// and row row (0 is top) of the digit is set
bool AutoMapScene_DigitPixel(int digit, int row, int col);

// The same 3x5 look for legend labels: letters (either case), digits and
// " +-.,:?/()=!%'". Anything else, including out-of-range rows and columns,
// reads blank.
bool AutoMapScene_FontPixel(char mark, int row, int col);

// Width of a label in font pixels: one 3-wide glyph plus one spacing column
// each, no trailing space. Multiply by the draw scale for screen pixels.
int AutoMapScene_TextWidth(const char *text);

// Fits the whole level into the map area. Zoomed in, the level no longer fits and the view
// centres on the request's cell plus the pan offset, without showing anything beyond the
// edge of the level. A fitting level ignores the pan offset.
AutoMapView AutoMapScene_PlaceLevel(const AutoMapViewRequest &request);

// Debug placement (map_debug in csb.cfg): whether the party or a new monster may be put
// on a cell of the snapshot. Only a cell the party could stand on takes either: no wall,
// closed false wall, door that is not fully open (or broken), or rock around the level,
// no monster and not the party itself.
enum AutoMapPlacement
{
  AutoMapPlacementFree,
  AutoMapPlacementSolid,
  AutoMapPlacementMonster,
  AutoMapPlacementParty
};

AutoMapPlacement AutoMapScene_Placement(const AutoMapLevelSnapshot &snapshot, int x, int y);

// Why a cell is refused, as the debug menu shows it; "" for a free cell
const char *AutoMapScene_DescribePlacement(AutoMapPlacement placement);

// Where on its cell the debug menu can spawn a monster. positions is the group's position byte
// for one monster: 255 the centre, else the quadrant 0 north-west, 1 north-east, 2 south-east,
// 3 south-west; facings the directions (AutoMapDirection) it may face there.
struct AutoMapSpawnSpot
{
  std::string name;
  int positions;
  std::vector<int> facings;
};

// By the monster's size (MONSTERDESC horizontalSize): 0 takes a quarter of the cell, the centre
// or any quadrant; 1 half of it, the centre or a half, lying along its facing (in the west or
// east half it faces north or south); 2 the whole cell, the centre.
std::vector<AutoMapSpawnSpot> AutoMapScene_SpawnSpots(int horizontalSize);
// How many monsters of that size share a cell: 4, 2 or 1 (CreateMonster cuts bigger groups).
// A group is placed the way CreateMonster places it, a full one fills the cell; the spots are
// for one monster alone. Its own AI moves a lone monster about its cell now and then
// (Monster.cpp, "Turn right"): back to the centre or to a neighbouring quadrant.
int AutoMapScene_LargestGroup(int horizontalSize);

// Debug level-up (map_debug): a base skill (fighter, ninja, priest, wizard) is at level 1
// below 500 experience and goes up a level each time the experience doubles, the way
// DetermineMastery counts it without temporary adjustments or possessions
static const int AUTOMAP_SKILL_EXPERIENCE_CAP = 0x10000000; // LimitSkillExperience
int AutoMapScene_SkillLevel(int experience);

// The experience that lifts a skill exactly to its next level; 0 when the cap is in the way
int AutoMapScene_ExperienceToNextLevel(int experience);

// The wall texts read and champion portraits seen and viewed on one level
struct AutoMapWallCounts { int textsRead, portraitsSeen, portraitsViewed; };
// Adds one wall side to counts. read: the side was seen straight ahead; lookedInto: its mirror
// was clicked (only a portrait sets that bit). A recruited champion's mirror shows no portrait
// any more, so a mirror that was clicked counts as seen and viewed whatever it shows now.
void AutoMapScene_CountWallSide(const AutoMapWallSide &side, bool read, bool lookedInto, AutoMapWallCounts *counts);

// A view centre for a level that is not on screen yet (Page Down, a search hit elsewhere): the
// level still shown keeps its own view until the requested level's draw list arrives
struct AutoMapPendingCentre { int level; float x, y; };
AutoMapPendingCentre AutoMapScene_NoPendingCentre(void);
void AutoMapScene_RequestCentre(AutoMapPendingCentre *pending, int level, float x, float y);
// True, with the centre, when shownLevel is the level it waits for; then it is gone
bool AutoMapScene_TakeCentre(AutoMapPendingCentre *pending, int shownLevel, float *x, float *y);

#endif
