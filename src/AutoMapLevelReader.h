#ifndef AUTOMAPLEVELREADER_H
#define AUTOMAPLEVELREADER_H

#include <string>
#include <vector>

#include "AutoMapScene.h"

// Reads the engine's dungeon into the plain records AutoMapScene works on.

std::vector<AutoMapLevelSize> AutoMapLevelReader_DungeonLevelSizes(void);

// Global offset of a level (its position in the dungeon's ruler coordinates).
// (0, 0) when the descriptors are unavailable or the level is out of range.
void AutoMapLevelReader_LevelOffset(int level, int *offsetX, int *offsetY);

// The identity of an engine record as the knowledge store keeps it: database number in the
// high half, index in the low half. The position bits are left out, they change when an
// item moves within its cell. recordInteger is RN::ConvertToInteger(), as the hooks get it;
// an empty or invalid record gives AUTOMAP_NO_RECORD.
AutoMapRecordId AutoMapLevelReader_RecordId(int recordInteger);

// Drops what was read once per dungeon (the pictures of the object descriptions); call when a
// dungeon is loaded
void AutoMapLevelReader_ForgetDungeon(void);

// The record and description of an RN integer, as the scene's item memory keeps them
AutoMapFloorItemTag AutoMapLevelReader_FloorItemTag(int object);

// The loaded level's floor items on (x, y), weapons to misc, as the snapshot lists them
std::vector<AutoMapFloorItemTag> AutoMapLevelReader_FloorItemsAt(int x, int y);
// A floor item as the visibility tests name it: its record, the name the game shows and its
// corner of the tile (0 north-west, 1 north-east, 2 south-east, 3 south-west)
struct AutoMapFloorItemPlace
{
  AutoMapRecordId record;
  std::string name;
  int corner;
};
// The items FloorItemsAt lists, in the same order
std::vector<AutoMapFloorItemPlace> AutoMapLevelReader_FloorItemPlacesAt(int x, int y);
// The loaded level's look of (x, y), as the cell memory keeps it
AutoMapCellLook AutoMapLevelReader_CellLookAt(int x, int y);

// Expects the party's level to be the level the engine has loaded
AutoMapLevelSnapshot AutoMapLevelReader_ReadPartyLevel(void);

// Any level of the dungeon, by index. Temporarily loads the level the way LoadLevel does
// and rebuilds that level's decoration tables from the dungeon data, then restores
// everything. Safe to call on every engine tick.
AutoMapLevelSnapshot AutoMapLevelReader_ReadLevel(int level);

// A monster type the level lists in the dungeon data: only these may live on it
// (MonsterAllowedOnLevel), and they set the level's palette
struct AutoMapMonsterChoice
{
  int type;
  std::string name;
  // MONSTERDESC horizontalSize: 0 a quarter of the cell, 1 half of it, 2 all of it
  int horizontalSize;
};

// The monster types of a level in the order the dungeon lists them; empty when the level
// is out of range or the descriptors are unavailable
std::vector<AutoMapMonsterChoice> AutoMapLevelReader_MonsterTypesOfLevel(int level);

// Translated name of an object type for tooltips, "" when unknown. The scene only
// carries the type id; the window resolves the text on demand for the hovered cell.
const char *AutoMapLevelReader_ItemName(int nameIndex);
// True when the object type is one the champions eat (objNI_Apple .. objNI_DragonSteak, the range
// the eating code in Character.cpp feeds from), for the search word FOOD
bool AutoMapLevelReader_ItemIsFood(int nameIndex);

// Dungeon-wide item search for the map window (implemented beside the engine
// hooks, which own the explored set): reads every level the way the per-tick
// rebuild does, filters each through the fog (explored cells everywhere,
// monsters/actuators only where the party sees right now on its own level)
// and returns the name hits ordered by match quality, then level, then
// position. Blank queries return nothing. Heavier than one tick rebuild (one
// read per level), so the caller runs it on query edits, not on every tick.
std::vector<AutoMapSearchHit> AutoMap_SearchDungeon(const std::string &query);

// Debug placement for the map window (map_debug; implemented beside the engine hooks).
// Cells are in the coordinates of the level's draw list. The placement reads the level
// as it is now; the requests wait for the next engine tick, check the cell again and are
// dropped when it no longer takes the party or a monster. A teleport works like a
// teleporter under the party that keeps its facing: pads, pits and teleporters at the
// destination act as usual. A spawn creates count monsters in one group the way a generator
// does, facing facing (a random one when -1); one alone stands at positions on its cell (an
// AutoMapSpawnSpot's), a group where CreateMonster puts it.
AutoMapPlacement AutoMap_PlacementAt(int level, int x, int y);
void AutoMap_RequestTeleport(int level, int x, int y);
void AutoMap_RequestSpawn(int level, int x, int y, int monsterType, int count, int positions, int facing);

// Debug level-up for the map window (map_debug): a champion of the party with the level of
// each base skill (fighter, ninja, priest, wizard) and whether it may gain one now. Dead
// champions, a sleeping party (the engine counts every skill as level 1 then) and the
// experience cap refuse. The request waits for the next engine tick, checks again and
// adds exactly the experience the next level needs, so LevelUp raises the statistics and
// prints its message as in play.
struct AutoMapChampionLevels
{
  std::string name;
  int level[4];
  bool canGainLevel[4];
};

std::vector<AutoMapChampionLevels> AutoMap_ChampionLevels(void);
void AutoMap_RequestLevelUp(int champion, int skill);

// Debug forgetting for the map window (map_debug), so visibility and learning can be tried
// again on cells already seen: the explored cells and seen wall faces of one level or of
// every level, or every learned fact. The map changes at once; the .map file only when the
// game is saved, so loading without saving brings the old map back.
void AutoMap_ForgetExploredLevel(int level);
void AutoMap_ForgetExploredDungeon(void);
void AutoMap_ForgetLearned(void);

// The player's own notes, for the map window's right-click menu and link mode. Cells are level
// cells of the shown level (a draw-list cell less the list's boundary margins); side is the
// AutoMapDirection of a wall face, -1 for a floor source. The map shows the change at once and
// the .map file keeps it when the game is saved. Nothing is checked against the dungeon, and the
// three forgets above leave the notes alone: they are the player's, not the map's.
// mark: -1 clears the cell's mark, 0 marks a closed hidden pit, 1 an open one.
void AutoMap_SetPlayerPit(int level, int x, int y, int mark);
// Adds the target to the source's links, or removes it when it is one already. A cell outside the
// level's size (the rim rock), a target on another level than the source, or a target on the
// source cell itself is ignored, as is a pit mark outside the level.
void AutoMap_TogglePlayerLink(int level, int srcX, int srcY, int side, int targetLevel, int dstX,
                              int dstY);
// Removes every link that starts on the cell (its floor or any face) or ends on it
void AutoMap_RemovePlayerLinksOf(int level, int x, int y);
void AutoMap_ForgetPlayerNotes(void);

// Debug measuring for the map window (map_debug): on the next tick the 3D view is drawn from
// every floor cell of the party's level and every wall decoration it draws is written to
// automap-walls-L<level>.csv in the working folder (AutoMapMeasure.h)
void AutoMap_RequestDecorationMeasure(void);

// Debug light for the map window (map_debug): the palette the 3D view is shown in, -1 for
// the party's own light or 0 (brightest) to 5 (dark). Only what is shown changes: monsters
// still see the party by the real light (Monster.cpp), and torches and light spells keep
// working on it. Kept until switched off; never saved.
int AutoMap_LightOverride(void);
void AutoMap_SetLightOverride(int palette);
// The palette the party's own light gives right now (d.CurrentPalette)
int AutoMap_RealPalette(void);


#endif
