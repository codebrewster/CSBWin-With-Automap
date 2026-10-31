#ifndef AUTOMAPHOOKS_H
#define AUTOMAPHOOKS_H

#include <map>

// The places where the engine and the platform layer call into the automap. This header
// declares functions only, so that it can be included after CSB.h, which leaves
// #pragma pack(1) switched on.

struct SUMMARIZEROOMDATA;

// DrawViewport has summarised its 21 cells
void AutoMap_OnViewportDrawn(const SUMMARIZEROOMDATA *cellsInView);
// DrawWallDecoration has put a champion's face on screen at drawLocation 12, the wall straight
// ahead (from afar it draws the frame only): noticed at once, and the map shows the face
void AutoMap_OnPortraitFaceDrawn(int drawLocation);
// DrawWallDecoration has put the words of a wall text on screen at drawLocation 12, the wall
// straight ahead: noticed, and read if the light reaches it
void AutoMap_OnWallTextRead(int drawLocation);
// DrawWallDecoration is about to draw a decoration at drawLocation 0..12; it counts as noticed
// only if enough of it is still visible in the finished frame (AutoMap_OnViewportFinished)
void AutoMap_BeforeWallDecorationDrawn(int drawLocation, int x1, int x2, int y1, int y2);
// DrawViewport has finished the frame, just before it is shown and the viewport is cleared
// to floor and ceiling for the next one
void AutoMap_OnViewportFinished(void);
// DrawWallDecoration has drawn a decoration into the viewport rectangle x1..x2, y1..y2; isText
// when it is wall text drawn as scribbles
void AutoMap_AfterWallDecorationDrawn(int drawLocation, int x1, int x2, int y1, int y2, bool isText);
// DrawFloorDecorations is about to draw, and has drawn, the floor decoration of relativeCell
// (0..8: three ahead left, centre, right, then two ahead, then one ahead) into the viewport
// rectangle x1..x2, y1..y2; the floor picture counts as noticed only if enough of it is still
// visible in the finished frame. Footprints are not reported.
void AutoMap_BeforeFloorDecorationDrawn(int x1, int x2, int y1, int y2);
void AutoMap_AfterFloorDecorationDrawn(int relativeCell, int x1, int x2, int y1, int y2);
// The viewport interpreter is about to draw, and has drawn, stairs in viewCell (AutoMapSight.h
// numbers the cells) into the viewport rectangle x1..x2, y1..y2; they count, like a floor
// picture, only if enough of them is still visible in the finished frame
void AutoMap_BeforeStairsDrawn(int x1, int x2, int y1, int y2);
void AutoMap_AfterStairsDrawn(int viewCell, int x1, int x2, int y1, int y2);
// The 3D view is about to draw the objects of (x, y) of the loaded level; the item draws that
// follow lie there
void AutoMap_OnObjectCellDrawn(int x, int y);
// DrawSingleObject is about to draw, and has drawn, a floor item (record) into the viewport
// rectangle x1..x2, y1..y2; it counts as seen only if enough of it is still visible in the
// finished frame (AutoMap_OnViewportFinished)
void AutoMap_BeforeItemDrawn(int item, int x1, int x2, int y1, int y2);
// TAG0088b2 has drawn src into the viewport rectangle x1..x2, y1..y2: the pixels whose source
// pixel is not transparentColor (-1: none is) were written, so a wall drawn later over an item in
// the item's own colours still hides it
void AutoMap_OnViewportBlit(const unsigned char *src, int x1, int x2, int y1, int y2,
                            int srcOffsetX, int srcOffsetY, int srcByteWidth, int transparentColor);
void AutoMap_AfterItemDrawn(int item, int x1, int x2, int y1, int y2);
// Likewise an item in the alcove of the wall whose objects are drawn: if enough of it stands out
// in the finished frame, the party has looked into that alcove, in the dark too
void AutoMap_BeforeAlcoveItemDrawn(int x1, int x2, int y1, int y2);
void AutoMap_AfterAlcoveItemDrawn(int item, int x1, int x2, int y1, int y2);
void AutoMap_BeforeMonsterDrawn(int group, int x1, int x2, int y1, int y2);
// DrawDoor is about to draw, and has drawn, the picture of the door record door into the
// viewport rectangle
void AutoMap_BeforeDoorDrawn(int x1, int x2, int y1, int y2);
void AutoMap_AfterDoorDrawn(int door, int x1, int x2, int y1, int y2);
void AutoMap_AfterMonsterDrawn(int group, int x1, int x2, int y1, int y2);
// The party took the item from the floor
void AutoMap_OnItemPickedUp(int item);
// For the statistics: per level, the explored cells inside the level that the map remembers as
// anything but solid wall (a closed false wall counts as wall)
std::map<int, int> AutoMap_ExploredTilesPerLevel(void);
// Per level, what the party has read and seen on the walls: wall texts read, champion portraits
// seen (drawn on the wall straight ahead) and champions viewed (the mirror clicked)
struct AutoMapWallCounts { int textsRead, portraitsSeen, portraitsViewed; };
std::map<int, AutoMapWallCounts> AutoMap_WallCountsPerLevel(void);
// The party put the item onto (x, y) of the loaded level: dropped it, or a champion died
void AutoMap_OnItemDropped(int item, int x, int y);
// The party put the item into the alcove of the wall (x, y) straight ahead of it
void AutoMap_OnItemPutIntoAlcove(int item, int x, int y);
// What the party finds out by playing. Records are RN::ConvertToInteger(); cells are on the
// level the engine has loaded unless a level is given.
// A wall actuator fired from a click of the party; itemMatched: the item in hand was the one
// the actuator compares with
void AutoMap_OnWallActuatorFired(int actuator, bool itemMatched);
// A pad or alcove actuator fired on (x, y) of the loaded level
void AutoMap_OnFloorActuatorFired(int actuator, int x, int y);
// The party clicked the wall ahead and no wall sound played: a passable false wall
void AutoMap_OnWallClickedSilently(int x, int y);
// Something fell through the pit on (level, x, y); isParty also covers climbing down a rope
void AutoMap_OnFellThroughPit(int level, int x, int y, bool isParty);
// A teleporter or spinner on (level, x, y) took the party
void AutoMap_OnPartyTeleported(int level, int x, int y);
void AutoMap_OnGeneratorFired(int actuator, int x, int y);
// A launcher in the wall (x, y) fired towards side
void AutoMap_OnLauncherFired(int actuator, int x, int y, int side);
void AutoMap_OnChestOpened(int chest);
// The damage the game showed the player. Groups are RN::ConvertToInteger() of the group on the
// loaded level. A melee hit's number on the attack panel, or PROJECTILE DAMAGE below the 3D view,
// for one member:
void AutoMap_OnMonsterDamageShown(int group, int member, int damage);
// SPELL DAMAGE below the 3D view: one number for the group, credited to every member it has now
void AutoMap_OnGroupDamageShown(int group, int damage);
// The member left the group; the ones above it move down a slot
void AutoMap_OnMonsterMemberRemoved(int group, int member);
// FindEmptyDB4Entry handed out the record for a new group: none of the old group's damage stays
void AutoMap_OnMonsterRecordTaken(int group);
// The party clicked the champion portrait on side of the wall (x, y) of its level and saw the
// champion's inventory, so the gear the portrait holds counts as looked into
void AutoMap_OnPortraitViewed(int x, int y, int side);
void AutoMap_OnScrollRead(int scroll);
// Once per tick of the main game loop, whether or not the 3D view was drawn
void AutoMap_OnEngineTick(void);
// A dungeon is ready to play: a new game or a saved one
void AutoMap_OnDungeonLoaded(void);
// Around the engine's renaming of the previous save to its .BAK name
void AutoMap_BeforeSaveIsRotated(void);
void AutoMap_AfterSaveIsRotated(void);
// The saved game is written and closed
void AutoMap_OnGameSaved(void);
// The engine loaded the .BAK and is about to rename it to the name of the save
void AutoMap_BeforeBackupIsRestored(void);

// csb --visibility-test: reads the case file (AutoMapVisibilityCases.h) and returns the name of
// the save of its section (-1: the only one), to be loaded; NULL, with the reason printed, when it
// cannot. On the first tick with the party's level loaded the run draws every case of the section,
// prints the failed checks and ends the program: exit code 0 when all checks passed, 1 otherwise.
// Nothing is saved. A file of several saves with no section given never returns: program runs
// once per section (--visibility-section), and the exit code is 0 when every section passed.
const char *AutoMap_PrepareVisibilityTest(const char *program, const char *casePath, int section);
// From a prepared visibility test until its run ends the program
bool AutoMap_VisibilityTestIsPending(void);

// Debug freeze for the map window (map_debug): the game's own Freeze Life
// (d.freezeLifeTimer), kept from running out while on, so monsters that are not
// invincible stand still and do not attack. Freeze Life holds the party's level only;
// while on, InitialChecks also skips the random step of monsters on other levels.
// Switching off ends any freeze at once. A game saved while frozen keeps up to 255 ticks of
// Freeze Life when it is loaded.
bool AutoMap_MonstersAreFrozen(void);
void AutoMap_SetMonstersFrozen(bool isFrozen);

// 0 fog of war, 1 explored, live, 2 whole level, as map_vision in csb.cfg
int AutoMap_Vision(void);
void AutoMap_SetVision(int vision);
// With the whole level shown: tells the cells already seen from the rest of the level
bool AutoMap_ExploredIsHighlighted(void);
void AutoMap_SetExploredHighlight(bool isOn);
// 0 seen, 1 learned, 2 all, as map_spoilers in csb.cfg
int AutoMap_SpoilerLevel(void);
void AutoMap_SetSpoilerLevel(int level);
// Which level the map window shows. Following the party is the default; locking to one
// level shows what lies beneath or above the party. Levels count from zero.
bool AutoMap_FollowsParty(void);
void AutoMap_FollowParty(void);
int AutoMap_ShownLevel(void);
int AutoMap_LevelCount(void);
void AutoMap_ShowLevel(int level);
// The palette the vertical blank shows the 3D view in: realPalette, or the map_debug light
// override when one is set. Game logic keeps reading d.CurrentPalette itself.
int AutoMap_ShownPalette(int realPalette);
// Read and write the automap's entries of csb.cfg; the caller has selected the file
void AutoMap_ReadSettings(void);
void AutoMap_WriteSettings(void);

#endif
