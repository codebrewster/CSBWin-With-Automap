#ifndef AUTOMAPRECORDER_H
#define AUTOMAPRECORDER_H

#include "AutoMapCellMemory.h"
#include "AutoMapExplored.h"
#include "AutoMapItemMemory.h"
#include "AutoMapKnowledge.h"
#include "AutoMapLinkWatch.h"
#include "AutoMapMonsterDamage.h"
#include "AutoMapPlayerNotes.h"
#include "AutoMapSight.h"

struct SUMMARIZEROOMDATA;

// Keeps what the party has seen, what it sees right now and what it has found out, fed by the
// 3D view and the engine hooks. Every method expects the party's level to be the level the
// engine has loaded.
class AutoMapRecorder
{
public:
  AutoMapRecorder(void);
  // cellsInView: the 21 cells DrawViewport has just summarised, in its own order. lightReach:
  // how many rows ahead the light on screen reaches (AutoMapSight_LightReach); farther cells are
  // neither explored nor visible now. A remembered item no longer on a cell the frame explores
  // is forgotten.
  void RecordViewport(const SUMMARIZEROOMDATA *cellsInView, int lightReach);
  void MarkPartyCell(void);
  // For the times the 3D view is not drawn: a character sheet is open or the party sleeps
  void SeeOnlyThePartyCell(void);
  // Refreshes the remembered looks from what the party sees this tick: the cells of the last
  // frame it explored or can tell, and the faces of the explored walls turned towards it. Only
  // the party's own cell while the 3D view is hidden. Reads single cells, never the level.
  void SeeCellLooks(void);
  // DrawWallDecoration has put pixels on screen at drawLocation 0..12 of the frame
  // RecordViewport stored last: when the frame is on the party's level and the view cell is
  // drawn as a wall, marks that wall face seen if the view cell is explored in that frame, else
  // glimpsed
  void MarkDrawnDecoration(int drawLocation);
  // The 3D view has put the words of a wall text on screen at drawLocation: read when the frame
  // is on the party's level and the light reaches that wall (its view cell is explored)
  void MarkTextRead(int drawLocation);
  // An item (record, descIndex) in the alcove of the wall (x, y) of the party's level, which the
  // 3D view draws only on the face towards the party, stood out in the finished frame: the wall
  // is explored, its face seen and that item remembered there, even beyond the light's reach
  void SeeAlcoveItem(int x, int y, AutoMapRecordId record, int descIndex);
  // The party put the item (record, descIndex) into the alcove of the wall (x, y) of its level,
  // on the face towards it: explored, the face seen and the item remembered there, in the dark too
  void PutIntoAlcove(int x, int y, AutoMapRecordId record, int descIndex);
  // The party clicked the wall (x, y) of its level straight ahead and an actuator on the face
  // towards it fired: explored and the face seen, in the dark too
  void UseFaceAhead(int x, int y);
  // The picture of the door on (x, y) of the party's level stood out in the finished frame: the
  // cell is explored and its look seen, even beyond the light's reach
  void SeeDoor(int x, int y);
  // The floor picture of (x, y) was drawn and noticed; beyond the light it tells nothing
  void SeeFloorPicture(int x, int y);
  // A champion portrait on side of the wall (x, y) of the party's level was clicked and the
  // champion's inventory shown: what the portrait holds is known now
  void LookIntoPortrait(int x, int y, int side);
  // Whether the party can tell what happens on (x, y) of its level: its own cell, the cell
  // ahead of it while the 3D view is drawn, or a cell it sees right now
  bool CanTell(int x, int y) const;
  // Whether the light of the frame RecordViewport stored last reached (x, y) of the party's
  // level and that frame explored it: what lies there is seen, not merely glimpsed
  bool IsLitInFrame(int x, int y) const;
  // The cells of the last frame the party sees right now, in level coordinates
  std::vector<AutoMapCell> CellsInSightNow(void) const;
  // The cells the party can see this tick, level coordinates: frame cells on the party's level
  // that are explored this frame, plus every cell CanTell accepts. What link learning counts
  // as seen; while the 3D view is hidden only the party's cell.
  AutoMapVisibleCells SeenThisTick(void) const;
  // The party fell through the pit (x, y) of level, which must be the loaded level: the fall
  // shows it open, so its remembered look is refreshed now, before the party lands below and
  // the next tick looks at another level
  void SeeFallenThroughPit(int level, int x, int y);
  void ForgetDungeon(const std::vector<AutoMapLevelSize> &levelSizes);
  AutoMapExplored &Explored(void);
  const AutoMapVisibleCells &VisibleNow(void) const;
  AutoMapKnowledge &Knowledge(void);
  AutoMapItemMemory &Items(void);
  AutoMapCellMemory &Cells(void);
  AutoMapPlayerNotes &Notes(void);
  AutoMapMonsterDamage &Damage(void);
  // The monster groups the last finished frame showed enough of, cleared when a frame starts and
  // when the 3D view is not drawn
  const std::vector<AutoMapRecordId> &MonstersSeenNow(void) const;
  void SeeMonster(AutoMapRecordId group);
  AutoMapLinkWatch &LinkWatch(void);
  AutoMapFalseWallWatch &FalseWallWatch(void);

private:
  // The 3D view's last frame, kept for the decorations it draws after summarising its cells
  struct FrameCell
  {
    int x;
    int y;
    bool isOnLevel;
    // One step outside the level, where the engine makes up rock (AutoMapExplored's rim)
    bool isOnRim;
    bool isWall;
    bool isExplored;
  };
  void ForgetItemsGoneFromFrame(void);
  // The look the party sees of (x, y) of its level now: a concealed pit it does not know keeps
  // the open state the map remembers
  AutoMapCellLook VisibleLookAt(const AutoMapCell &cell) const;
  AutoMapExplored m_explored;
  AutoMapVisibleCells m_visibleNow;
  AutoMapKnowledge m_knowledge;
  AutoMapItemMemory m_items;
  AutoMapCellMemory m_cells;
  AutoMapPlayerNotes m_notes;
  AutoMapMonsterDamage m_damage;
  std::vector<AutoMapRecordId> m_monstersSeenNow;
  AutoMapLinkWatch m_linkWatch;
  AutoMapFalseWallWatch m_falseWallWatch;
  int m_frameLevel;
  int m_frameFacing;
  // From SeeOnlyThePartyCell until the 3D view is drawn again
  bool m_viewIsHidden;
  FrameCell m_frame[AUTOMAP_SIGHT_CELL_COUNT];
};

#endif
