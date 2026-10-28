#ifndef AUTOMAPSIGHT_H
#define AUTOMAPSIGHT_H

#include <vector>

// The line-of-sight rule over the cells the 3D view draws in front of the party.
//
// Cell numbers are the 3D view's own (relativeX and relativeY in Viewport.cpp). The view never
// shows more than three tiles ahead or more than one tile to either side, but for the stone of
// 2 and 3 at the screen's edge, so only the bracketed cells can be seen:
//
//               2 left  1 left  ahead  1 right  2 right
//   4 ahead        0       4      6       5        1     <- never seen
//   3 ahead       (2)     [7]     [9]     [8]      (3)    <- 2,3 seen only as stone
//   2 ahead       10      [12]    [14]    [13]     11     <- 10,11 never seen
//   1 ahead              [15]    [17]    [16]
//   party row            [18]    [20]    [19]
//
// What the 3D view shows is read from how it paints: far rows first, in each row the sides
// before the middle, every wall over its picture's screen columns from floor to ceiling. A cell
// is on screen where no blocking cell (stone, shut solid door) painted after it covers its
// columns, a floor cell taking the columns its wall would. That shows the strip of the column
// beside a wall straight ahead, and the inner edge of the tile in front of a wall beside the
// party.
//
// Two questions are answered from this. Is the cell explored at all (FindExploredCells): objects
// lie anywhere in the cell, so any AUTOMAP_SEEN_SCREEN_COLUMNS columns of it on screen count,
// which is why a sword on the front-right tile shows while a wall beside the party still hides
// the monster standing on the same tile. Is the cell seen right now (FindVisibleCells): a wall
// as it is explored; on a floor cell a monster stands at the centre, so the ray from the party's
// cell centre to it must also miss every blocking cell, even its corners.

const int AUTOMAP_SIGHT_CELL_COUNT = 21;
const int AUTOMAP_SIGHT_PARTY_CELL = 20;
// A cell is on screen from as many of its columns left uncovered. The narrowest strips the 3D view
// shows are 9 columns (a floor three ahead past a wall two ahead on that side) and 11 (two ahead
// past a wall one ahead); fewer are the edges of pictures meeting.
const int AUTOMAP_SEEN_SCREEN_COLUMNS = 4;

struct AutoMapSightOffset
{
  int stepsForward;
  int stepsRight;
};

AutoMapSightOffset AutoMapSight_OffsetOfCell(int cellNumber);

// The engine's numbers for the position of a door
enum AutoMapDoorState
{
  AutoMapDoorOpen = 0,
  AutoMapDoorAlmostOpen = 1,
  AutoMapDoorHalfOpen = 2,
  AutoMapDoorAlmostClosed = 3,
  AutoMapDoorClosed = 4,
  AutoMapDoorDestroyed = 5
};

struct AutoMapSightDoor
{
  AutoMapDoorState state;
  // Grates and portcullises: the engine lets monsters look through them while they are closed
  bool isSeeThroughType;
};

bool AutoMapSight_CanSeeThroughDoor(const AutoMapSightDoor &door);

// All arrays have AUTOMAP_SIGHT_CELL_COUNT entries. On cells 2 and 3 a blocking cell is stone:
// the 3D view draws nothing of a door there, so it is passed as see-through. isShutDoor marks the
// blocking cells that are a shut door facing the party: one covers only the columns its door
// is painted in, not the whole wall's (beside the way the door alone, without the stone around
// it; beside the party nothing at all).
void AutoMapSight_FindVisibleCells(const bool *canSeeThrough, const bool *isShutDoor, bool *isVisible);
void AutoMapSight_FindExploredCells(const bool *canSeeThrough, const bool *isShutDoor, bool *isExplored);

// Where the 3D view draws a wall decoration: DrawWallDecoration's drawLocation (0..12) names
// one view cell and one of its faces. summaryIndex is 0 for the right face, 1 for the front
// face and 2 for the left face. False outside 0..12.
bool AutoMapSight_DecorationPlacement(int drawLocation, int *cellNumber, int *summaryIndex);
// The absolute side (an AutoMapDirection) of a face, the rule ReadWallSides uses
int AutoMapSight_SideOfSummaryIndex(int partyFacing, int summaryIndex);
// The faces of a view cell the 3D view draws, indexed by summaryIndex: exactly the faces some
// drawLocation of AutoMapSight_DecorationPlacement names. 15 shows only its right face, 16 only
// its left, 7 and 12 right and front, 8 and 13 left and front, 9, 14 and 17 the front; the cells
// beside the party, the party's own cell and the never seen cells show none.
void AutoMapSight_FacesShownOfCell(int cellNumber, bool shown[3]);

// Whether a wall decoration on screen is noticed (AutoMapSight_DecorationIsSeen): a pixel it
// left in the finished frame contrasts when its brightness in the palette shown differs by
// AUTOMAP_DECORATION_CONTRAST_STEPS steps of the palette's 0..7 scale from the median brightness
// of the wall within AUTOMAP_CAMOUFLAGE_RADIUS around it; a decoration is seen from
// AUTOMAP_SEEN_DECORATION_PIXELS contrasting pixels. Measured in the game, noticed: a ring beside
// the party in full light 42, rings farther down the corridor 14 and 10, a big switch front left
// in full light 11; missed: tiny switches 3 and 4 (one on a plain wall), the big switch at light
// 3 (4, only when looking for it) and 4 (3), a small switch 2, the ring in the dark 2.
const int AUTOMAP_DECORATION_CONTRAST_STEPS = 2;
const int AUTOMAP_SEEN_DECORATION_PIXELS = 8;
// A decoration is also noticed, like a floor item, from a patch of as many touching pixels whose
// colour the wall around them does not show (AutoMapSight_ShapeOfItem): in a dark palette a tint
// tells it where brightness cannot. Measured in the game, a wooden board: straight ahead at light
// 4, black and dark red on a wall of 1,1,1, no pixel two steps off, patch 258, seen; seen from the
// side on the tile ahead-right, dark red 1,0,0 on a wall gone black, at light 4 patch 38 (seen
// when looked for), 3: 43 (seen), 2: 70; a crack at the foot of the wall straight ahead at light
// 4, 7 pixels two steps off, patch 14 (seen); at light 5 every time 1 or less, not seen.
const int AUTOMAP_SEEN_DECORATION_PATCH = 8;
// A patch of such pixels that are also tinted (not grey, as the stone is) and each a brightness
// step off the wall around them (the median, as for contrast) needs only half as many: the eye
// picks out a colour the stone never shows when it also has light. A grey a step off the wall
// reads as the stone's own speckle, however it touches. Measured in the game, a brown line on the
// side of the wall three ahead-left, on a wall gone mostly black: light 1 (3,1,0), 5 such pixels
// touching, light 2 (2,1,0), 6, both seen; light 3 (1,0,0), a third of a step, none, missed. A
// grey line of 7 pixels (1,1,1) on black at light 4 (party on 0,4,15 facing south, the line on
// 0,5,17 west face): missed.
const int AUTOMAP_SEEN_DECORATION_TINT_PATCH = 4;
// A decoration is also noticed as a blot: when its pixels together are on average as many
// hundredths of a palette step darker or brighter than the wall they cover, and it is at least
// so large. In a speckled dark wall every black pixel of a slot has a black speck beside it, so
// neither test above sees it, but the eye sees the specks gone. Measured in the game, a slot of
// 86 pixels on the wall ahead-right: light 2 0.97 steps, light 3 0.72 (seen), light 4 0.47 (only
// when looked for), light 5 0.23 (missed).
const int AUTOMAP_SEEN_DECORATION_DARKENING = 60;
const int AUTOMAP_SEEN_BLOT_PIXELS = 40;
// A floor item is seen from a patch of as many touching pixels that stand out from what is around
// them (AutoMapSight_ShapeOfItem). Measured in the game: the hilt of a sword in the dark (patch 4)
// and corn in the dark (5) can be noticed; the bread mostly behind a wall (2) is missed.
const int AUTOMAP_SEEN_ITEM_PATCH = 4;
// How far around an item pixel the background is looked at for its colour
const int AUTOMAP_CAMOUFLAGE_RADIUS = 2;
// A monster group is seen from as many of its pixels standing out (AutoMapSight_ShapeOfItem), all
// its members together, touching or not. Measured in the game: two eyes on black (2) are noticed,
// a single bright eye pixel (1) barely; a Rock monster, worms and a Vexirk drawn as dark as the
// floor (0) are missed.
const int AUTOMAP_SEEN_MONSTER_PIXELS = 2;

struct AutoMapPixelPair
{
  int behind;
  int drawn;
  int count;
};

// palette: 16 colours 0x0RGB, 3 bits per channel
int AutoMapSight_VisiblePixels(const std::vector<AutoMapPixelPair> &pixels, const short *palette);
// A draw and what is around it on screen, row-major, width * height, colours 0x0RGB.
// screen: the finished frame; isDrawn: the draw's kept pixels; background: what it is seen
// against: for a floor item the frame without the floor items, so that under a pile the floor
// shows; for a decoration the wall, under its pixels what they cover.
struct AutoMapDrawView
{
  int width;
  int height;
  std::vector<short> screen;
  std::vector<short> background;
  std::vector<bool> isDrawn;
};

// drawn: the item's kept pixels; visible: those that stand out, as no background within
// AUTOMAP_CAMOUFLAGE_RADIUS shows their colour (grey bread by grey stone, a black club on a
// black floor are camouflaged); largestPatch: the most of them touching, sideways or diagonally
struct AutoMapItemShape { int drawn; int visible; int largestPatch; };
// standsOut, unless NULL: per pixel of the view, whether it is an item pixel that stands out
AutoMapItemShape AutoMapSight_ShapeOfItem(const AutoMapDrawView &view, std::vector<bool> *standsOut);
// A shut door is seen from as many of its pixels standing out (AutoMapSight_ShapeOfItem), touching
// or not: its pattern spreads over the doorway, so no patch need be large. Measured in the game:
// a portcullis two ahead at light 5 shows as a grid of 25 patches, 5 of one pixel and 20 wedges of
// three (65 pixels), and is noticed.
const int AUTOMAP_SEEN_DOOR_PIXELS = 16;
bool AutoMapSight_DoorIsSeen(const AutoMapItemShape &shape);
// The decoration's kept pixels that contrast with the wall around them; contrasts, unless NULL:
// per pixel of the view, whether it is one of them
int AutoMapSight_ContrastingPixels(const AutoMapDrawView &view, std::vector<bool> *contrasts);
// How much the decoration's kept pixels together differ from the wall under them: the mean
// brightness of each, in hundredths of a palette step, made positive. 0 for no pixels.
int AutoMapSight_DecorationDarkening(const AutoMapDrawView &view);
// The most kept pixels touching that stand out (AutoMapSight_ShapeOfItem), are tinted and are
// each at least a brightness step from the wall's median around them
int AutoMapSight_DecorationTintPatch(const AutoMapDrawView &view);
bool AutoMapSight_DecorationIsSeen(const AutoMapDrawView &view);

// How far ahead the light on screen reaches. The 3D view shows three rows ahead; in a dark
// palette the far ones are drawn black, and what is black has not been seen. Row r (F1, F2, F3)
// is reached when at least AUTOMAP_REACHED_ROW_PERCENT of the pixels of the level's front wall
// picture of that row are one brightness step above black in the palette shown. A row behind an
// unreached one is not reached. The standard wall set should give
//   light 0..3 -> reach 3, light 4 -> reach 2, light 5 -> reach 1
// (the share is a fraction because the pictures differ in size: F1 has about 28000 pixels, F3
// about 6500). The table is the target the calibration must reach, not yet a measured result:
// set the share in the game until the counts of palettes 3, 4 and 5 give it. The party's own
// cell is always seen, even at reach 0.
const int AUTOMAP_REACH_ROWS = 3;
const int AUTOMAP_REACHED_ROW_PERCENT = 4;

// colourCounts[16]: pixels of a picture per colour index. 0..100, rounded down; 0 for no pixels.
int AutoMapSight_LitPercent(const int *colourCounts, const short *palette);
// Row index 0 is F1. Counts the reached rows from the nearest: 0..AUTOMAP_REACH_ROWS
int AutoMapSight_LightReach(const int colourCounts[AUTOMAP_REACH_ROWS][16], const short *palette);
// Sets to false every cell more rows ahead than reach. Array of AUTOMAP_SIGHT_CELL_COUNT entries
void AutoMapSight_CutAtLightReach(int reach, bool *cells);

// The cells of one level the party can see right now
class AutoMapVisibleCells
{
public:
  void Clear(void);
  void Add(int x, int y);
  bool Contains(int x, int y) const;

private:
  struct Position
  {
    int x;
    int y;
  };
  std::vector<Position> m_positions;
};

#endif
