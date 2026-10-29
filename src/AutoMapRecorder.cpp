// AutoMapRecorder.cpp : records which cells the 3D view shows the party.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include "AutoMapRecorder.h"

#include <map>

#include "AutoMapLevelReader.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

static bool IsOnLevel(const SUMMARIZEROOMDATA &cell)
{
  return cell.x >= 0 && cell.x < d.width && cell.y >= 0 && cell.y < d.height;
}

static bool IsOnRim(const SUMMARIZEROOMDATA &cell)
{
  return !IsOnLevel(cell) && cell.x >= -1 && cell.x <= d.width && cell.y >= -1 && cell.y <= d.height;
}

i32 GetPixel(pnt, i16, i16, i16); // Chaos.cpp

// The closest door graphic, as DrawDoor composes it: 96x88 pixels, 48 bytes per line
static const i32 DOOR_BYTE_WIDTH = 48;
static const i32 DOOR_WIDTH = 2 * DOOR_BYTE_WIDTH;
static const i32 DOOR_HEIGHT = 0x1080 / DOOR_BYTE_WIDTH;
static const i32 DOOR_TRANSPARENT_COLOR = 10;     // BltTempToViewport
static const i32 ORNAMENT_TRANSPARENT_COLOR = 9;  // TAG004b26
static const i32 ORNAMENT_NEARNESS_F1 = 2;

// Counts the pixels of a shut door through which the 3D view shows the cells behind it.
// A door ornament is painted over the door graphic: it can cover a grate entirely, and its
// own pixels in the door's transparent color open a window in a solid door.
static i32 CountSeeThroughPixels(DB0 *engineDoor)
{
  ui8 *doorGraphic = GetBasicGraphicAddress(d.DoorGraphic[0][engineDoor->doorType()]);
  ui8 *ornamentGraphic = NULL;
  const ui8 *ornamentRect = NULL;
  i32 ornament = engineDoor->ornateness();
  if (ornament != 0)
  {
    ornamentGraphic = GetBasicGraphicAddress(d.Word2514[ornament - 1][0]);
    ornamentRect = (const ui8 *)d.Byte5070 + 18 * d.Word2514[ornament - 1][1] + 6 * ORNAMENT_NEARNESS_F1;
  }
  i32 count = 0;
  for (i32 y = 0; y < DOOR_HEIGHT; y++)
  {
    for (i32 x = 0; x < DOOR_WIDTH; x++)
    {
      i32 color = GetPixel((pnt)doorGraphic, sw(x), sw(y), DOOR_BYTE_WIDTH);
      if (ornamentGraphic != NULL
          && x >= ornamentRect[0] && x <= ornamentRect[1]
          && y >= ornamentRect[2] && y <= ornamentRect[3])
      {
        i32 ornamentColor = GetPixel((pnt)ornamentGraphic, sw(x - ornamentRect[0]), sw(y - ornamentRect[2]), ornamentRect[4]);
        if (ornamentColor != ORNAMENT_TRANSPARENT_COLOR) color = ornamentColor;
      }
      if (color == DOOR_TRANSPARENT_COLOR) count++;
    }
  }
  return count;
}

// A plain wooden door leaves 114 stray pixels transparent, a small window ornament in it 304,
// grates and glass-like doors thousands
static const i32 MIN_SEE_THROUGH_PIXELS = DOOR_WIDTH * DOOR_HEIGHT / 40;

// The count only depends on the door and ornament graphics, so it is worked out once per pair
static i32 SeeThroughPixelsWhenShut(DB0 *engineDoor)
{
  static std::map<i32, i32> known;
  i32 ornament = engineDoor->ornateness();
  i32 key = d.DoorGraphic[0][engineDoor->doorType()] << 16;
  if (ornament != 0) key |= (d.Word2514[ornament - 1][0] << 4) | d.Word2514[ornament - 1][1];
  std::map<i32, i32>::iterator found = known.find(key);
  if (found != known.end()) return found->second;
  i32 count = CountSeeThroughPixels(engineDoor);
  known[key] = count;
  return count;
}

// The engine's see-through attribute (d.DoorTOC & 0x100) only decides whether monsters look
// through a door. What the party sees is what the door graphic leaves transparent: a grate, a
// glass-like door, or a window ornament in a wooden door.
static bool CanSeeThroughDoor(const SUMMARIZEROOMDATA &doorCell)
{
  DB0 *engineDoor = GetRecordAddressDB0(doorCell.decorations[1]);
  AutoMapSightDoor door;
  door.state = (AutoMapDoorState)doorCell.decorations[0];
  door.isSeeThroughType = SeeThroughPixelsWhenShut(engineDoor) >= MIN_SEE_THROUGH_PIXELS;
  return AutoMapSight_CanSeeThroughDoor(door);
}

// A door seen from the side is not drawn by the 3D view, which shows the cells behind it
static bool CanSeeThrough(const SUMMARIZEROOMDATA &cell)
{
  if (!IsOnLevel(cell)) return false;
  if (cell.roomType == roomSTONE) return false;
  if (cell.roomType == roomDOORFACING) return CanSeeThroughDoor(cell);
  return true;
}

AutoMapRecorder::AutoMapRecorder(void)
  : m_frameLevel(AUTOMAP_NO_LEVEL), m_frameFacing(0), m_viewIsHidden(false)
{
}

void AutoMapRecorder::RecordViewport(const SUMMARIZEROOMDATA *cellsInView, int lightReach)
{
  bool canSeeThrough[AUTOMAP_SIGHT_CELL_COUNT];
  bool isShutDoor[AUTOMAP_SIGHT_CELL_COUNT];
  bool isVisible[AUTOMAP_SIGHT_CELL_COUNT];
  bool isExplored[AUTOMAP_SIGHT_CELL_COUNT];
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    canSeeThrough[i] = CanSeeThrough(cellsInView[i]);
    isShutDoor[i] = !canSeeThrough[i] && IsOnLevel(cellsInView[i]) && cellsInView[i].roomType == roomDOORFACING;
  }
  // Three ahead and two to a side the 3D view draws stone and nothing else (AutoMapSight.h)
  static const int STONE_ONLY_CELLS[2] = {2, 3};
  for (int i = 0; i < 2; i++)
  {
    const SUMMARIZEROOMDATA &cell = cellsInView[STONE_ONLY_CELLS[i]];
    if (IsOnLevel(cell) && cell.roomType != roomSTONE) canSeeThrough[STONE_ONLY_CELLS[i]] = true;
  }
  AutoMapSight_FindVisibleCells(canSeeThrough, isShutDoor, isVisible);
  AutoMapSight_FindExploredCells(canSeeThrough, isShutDoor, isExplored);
  bool isInSight[AUTOMAP_SIGHT_CELL_COUNT];
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++) isInSight[i] = isExplored[i];
  // What the light does not reach is drawn black: it is not seen, whatever the ray says
  AutoMapSight_CutAtLightReach(lightReach, isVisible);
  AutoMapSight_CutAtLightReach(lightReach, isExplored);
  m_visibleNow.Clear();
  m_monstersSeenNow.clear();
  m_viewIsHidden = false;
  m_frameLevel = d.partyLevel;
  m_frameFacing = d.partyFacing;
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const SUMMARIZEROOMDATA &cell = cellsInView[i];
    // The rim is rock, made up by the engine
    bool isWall = cell.roomType == roomSTONE || IsOnRim(cell);
    FrameCell frameCell = {cell.x, cell.y, IsOnLevel(cell), IsOnRim(cell), isWall, isExplored[i]};
    m_frame[i] = frameCell;
    if (IsOnRim(cell) && isExplored[i])
    {
      AutoMapCell rimCell = {d.partyLevel, cell.x, cell.y};
      m_explored.Mark(rimCell);
    }
    if (!IsOnLevel(cell)) continue;
    if (isVisible[i]) m_visibleNow.Add(cell.x, cell.y);
    // A visible teleporter shimmers in the dark too: the cell shows on the map, but what lies
    // in it is only glimpsed
    bool isShimmering = isInSight[i] && cell.roomType == roomTELEPORTER && cell.decorations[0] != 0;
    if (isExplored[i] || isShimmering)
    {
      AutoMapCell seenCell = {d.partyLevel, cell.x, cell.y};
      m_explored.Mark(seenCell);
    }
  }
  ForgetItemsGoneFromFrame();
}

// An explored cell is one the party can look at: a remembered item that is no longer there,
// or whose record now holds another kind of item, is seen to be gone. Beyond the light's reach
// the map keeps it, as the player could not tell either.
void AutoMapRecorder::ForgetItemsGoneFromFrame(void)
{
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const FrameCell &frameCell = m_frame[i];
    if (!frameCell.isOnLevel || frameCell.isWall || !frameCell.isExplored) continue;
    AutoMapCell cell = {m_frameLevel, frameCell.x, frameCell.y};
    // Reading the live pile costs more than asking the memory, and most cells hold nothing
    if (m_items.ItemsOn(cell).empty()) continue;
    m_items.KeepOnlyItemsStillOn(cell, AutoMapLevelReader_FloorItemsAt(frameCell.x, frameCell.y));
  }
}

void AutoMapRecorder::MarkDrawnDecoration(int drawLocation)
{
  int cellNumber = 0;
  int summaryIndex = 0;
  if (!AutoMapSight_DecorationPlacement(drawLocation, &cellNumber, &summaryIndex)) return;
  if (m_frameLevel != d.partyLevel) return;
  const FrameCell &cell = m_frame[cellNumber];
  // A closed false wall is summarised as stone, so its faces count like any wall's; the rim's
  // made-up rock has decorations too
  if ((!cell.isOnLevel && !cell.isOnRim) || !cell.isWall) return;
  AutoMapCell wallCell = {m_frameLevel, cell.x, cell.y};
  int side = AutoMapSight_SideOfSummaryIndex(m_frameFacing, summaryIndex);
  // Beyond the light only that something is on the wall shows, not what
  if (cell.isExplored) m_explored.MarkFace(wallCell, side);
  else m_explored.MarkGlimpsedFace(wallCell, side);
}

void AutoMapRecorder::MarkTextRead(int drawLocation)
{
  int cellNumber = 0;
  int summaryIndex = 0;
  if (!AutoMapSight_DecorationPlacement(drawLocation, &cellNumber, &summaryIndex)) return;
  if (m_frameLevel != d.partyLevel) return;
  const FrameCell &cell = m_frame[cellNumber];
  if (!cell.isOnLevel || !cell.isWall || !cell.isExplored) return;
  AutoMapCell wallCell = {m_frameLevel, cell.x, cell.y};
  m_explored.MarkTextRead(wallCell, AutoMapSight_SideOfSummaryIndex(m_frameFacing, summaryIndex));
}

void AutoMapRecorder::SeeAlcoveItem(int x, int y, AutoMapRecordId record, int descIndex)
{
  if (m_frameLevel != d.partyLevel) return;
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const FrameCell &cell = m_frame[i];
    if (!cell.isOnLevel || !cell.isWall || cell.x != x || cell.y != y) continue;
    AutoMapCell wallCell = {m_frameLevel, x, y};
    int side = AutoMapSight_SideOfSummaryIndex(m_frameFacing, 1);
    // The wall is seen with it, or the map would draw neither the wall nor its glimpse
    m_explored.Mark(wallCell);
    m_explored.MarkFace(wallCell, side);
    // This item, not the others in the alcove: a cheese that stands out in the dark tells
    // nothing of the bread beside it. Kept on the wall cell, where floor items never lie.
    AutoMapRememberedItem seen = {record, descIndex, wallCell, false};
    m_items.See(seen);
    return;
  }
}

void AutoMapRecorder::PutIntoAlcove(int x, int y, AutoMapRecordId record, int descIndex)
{
  UseFaceAhead(x, y);
  AutoMapCell wallCell = {d.partyLevel, x, y};
  AutoMapRememberedItem put = {record, descIndex, wallCell, false};
  m_items.See(put);
}

void AutoMapRecorder::UseFaceAhead(int x, int y)
{
  AutoMapCell wallCell = {d.partyLevel, x, y};
  m_explored.Mark(wallCell);
  m_explored.MarkFace(wallCell, (d.partyFacing + 2) % AUTOMAP_SIDE_COUNT);
}

void AutoMapRecorder::SeeDoor(int x, int y)
{
  if (m_frameLevel != d.partyLevel) return;
  AutoMapCell doorCell = {m_frameLevel, x, y};
  m_explored.Mark(doorCell);
  m_cells.See(doorCell, VisibleLookAt(doorCell));
}

void AutoMapRecorder::SeeFloorPicture(int x, int y)
{
  if (m_frameLevel != d.partyLevel) return;
  if (!IsLitInFrame(x, y)) return;
  AutoMapCell floorCell = {m_frameLevel, x, y};
  m_explored.MarkFloorSeen(floorCell);
}

void AutoMapRecorder::LookIntoPortrait(int x, int y, int side)
{
  if (side < 0 || side >= AUTOMAP_SIDE_COUNT) return;
  AutoMapCell wallCell = {d.partyLevel, x, y};
  m_explored.MarkAlcoveLookedInto(wallCell, side);
}

bool AutoMapRecorder::CanTell(int x, int y) const
{
  if (x == d.partyX && y == d.partyY) return true;
  // With a character sheet open or the party asleep nothing ahead is on screen
  if (!m_viewIsHidden
      && x == d.partyX + d.DeltaX[d.partyFacing] && y == d.partyY + d.DeltaY[d.partyFacing])
  {
    return true;
  }
  return m_visibleNow.Contains(x, y);
}

bool AutoMapRecorder::IsLitInFrame(int x, int y) const
{
  if (m_frameLevel != d.partyLevel) return false;
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const FrameCell &cell = m_frame[i];
    if (cell.isOnLevel && cell.isExplored && cell.x == x && cell.y == y) return true;
  }
  return false;
}

std::vector<AutoMapCell> AutoMapRecorder::CellsInSightNow(void) const
{
  std::vector<AutoMapCell> cells;
  if (m_frameLevel != d.partyLevel) return cells;
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const FrameCell &cell = m_frame[i];
    if (!cell.isOnLevel || !m_visibleNow.Contains(cell.x, cell.y)) continue;
    AutoMapCell seenCell = {m_frameLevel, cell.x, cell.y};
    cells.push_back(seenCell);
  }
  return cells;
}

AutoMapVisibleCells AutoMapRecorder::SeenThisTick(void) const
{
  AutoMapVisibleCells seen;
  seen.Add(d.partyX, d.partyY);
  // Asleep or with a character sheet open nothing but the party's cell is on screen, and a
  // frame of another level shows nothing of this one, the cell ahead neither: as SeeCellLooks
  if (m_viewIsHidden || m_frameLevel != d.partyLevel) return seen;
  seen.Add(d.partyX + d.DeltaX[d.partyFacing], d.partyY + d.DeltaY[d.partyFacing]);
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    const FrameCell &cell = m_frame[i];
    if (!cell.isOnLevel) continue;
    // Any sliver in the light shows the cell, though the ray to its centre may graze a wall
    if (cell.isExplored || CanTell(cell.x, cell.y)) seen.Add(cell.x, cell.y);
  }
  return seen;
}

void AutoMapRecorder::SeeFallenThroughPit(int level, int x, int y)
{
  if (level != d.LoadedLevel) return;
  AutoMapCell cell = {level, x, y};
  m_cells.See(cell, AutoMapLevelReader_CellLookAt(x, y));
}

AutoMapCellLook AutoMapRecorder::VisibleLookAt(const AutoMapCell &cell) const
{
  AutoMapCellLook seen = AutoMapLevelReader_CellLookAt(cell.x, cell.y);
  AutoMapCellLook remembered;
  bool isRemembered = m_cells.LookOf(cell, &remembered);
  bool pitIsKnown = m_knowledge.Knows(AutoMapFact_OfCell(AutoMapFactPit, cell));
  return AutoMapScene_VisibleLook(seen, isRemembered ? &remembered : NULL, pitIsKnown);
}

void AutoMapRecorder::MarkPartyCell(void)
{
  AutoMapCell partyCell = {d.partyLevel, d.partyX, d.partyY};
  m_explored.Mark(partyCell);
}

void AutoMapRecorder::SeeOnlyThePartyCell(void)
{
  m_visibleNow.Clear();
  m_visibleNow.Add(d.partyX, d.partyY);
  m_monstersSeenNow.clear();
  m_viewIsHidden = true;
}

void AutoMapRecorder::SeeCellLooks(void)
{
  // Also when the frame is from another level or the view is hidden: the party sees its own cell
  AutoMapCell partyCell = {d.partyLevel, d.partyX, d.partyY};
  m_cells.See(partyCell, VisibleLookAt(partyCell));
  // Asleep or with a character sheet open nothing else is on screen, so a pit toggling beside
  // the party keeps the look it had
  if (m_viewIsHidden || m_frameLevel != d.partyLevel) return;
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++)
  {
    if (i == AUTOMAP_SIGHT_PARTY_CELL) continue;
    const FrameCell &frameCell = m_frame[i];
    if (!frameCell.isOnLevel) continue;
    AutoMapCell cell = {m_frameLevel, frameCell.x, frameCell.y};
    // A sliver in the light explores a cell; without its look it would later be remembered as
    // whatever it had become by then
    if (!frameCell.isExplored && !CanTell(frameCell.x, frameCell.y))
    {
      // Explored without being looked at in the light (a teleporter shimmering in the dark, a
      // wall whose alcove item stood out): its look now, on the tick it is explored, not the
      // one a later map build would find after a change out of sight
      AutoMapCellLook known;
      if (m_explored.IsExplored(cell) && !m_cells.LookOf(cell, &known))
      {
        m_cells.RememberIfNew(cell, AutoMapLevelReader_CellLookAt(frameCell.x, frameCell.y));
      }
      continue;
    }
    // A concealed pit the party does not know reads as floor open or closed, so a sighting
    // does not tell its state
    AutoMapCellLook look = VisibleLookAt(cell);
    m_cells.See(cell, look);
    if (!frameCell.isWall || !frameCell.isExplored) continue;
    bool shown[3];
    AutoMapSight_FacesShownOfCell(i, shown);
    for (int summaryIndex = 0; summaryIndex < 3; summaryIndex++)
    {
      if (!shown[summaryIndex]) continue;
      int side = AutoMapSight_SideOfSummaryIndex(m_frameFacing, summaryIndex);
      m_cells.SeeSide(cell, side, look.sidePictures[side]);
    }
  }
}

void AutoMapRecorder::ForgetDungeon(const std::vector<AutoMapLevelSize> &levelSizes)
{
  m_explored.Reset(levelSizes);
  m_visibleNow.Clear();
  m_monstersSeenNow.clear();
  m_knowledge.Forget();
  m_items.Forget();
  m_cells.Forget();
  m_notes.Forget();
  m_damage.Forget();
  m_linkWatch.Clear();
  m_falseWallWatch.Clear();
  m_frameLevel = AUTOMAP_NO_LEVEL;
  m_viewIsHidden = false;
}

AutoMapExplored &AutoMapRecorder::Explored(void)
{
  return m_explored;
}

const std::vector<AutoMapRecordId> &AutoMapRecorder::MonstersSeenNow(void) const
{
  return m_monstersSeenNow;
}

void AutoMapRecorder::SeeMonster(AutoMapRecordId group)
{
  m_monstersSeenNow.push_back(group);
}

const AutoMapVisibleCells &AutoMapRecorder::VisibleNow(void) const
{
  return m_visibleNow;
}

AutoMapKnowledge &AutoMapRecorder::Knowledge(void)
{
  return m_knowledge;
}

AutoMapItemMemory &AutoMapRecorder::Items(void)
{
  return m_items;
}

AutoMapCellMemory &AutoMapRecorder::Cells(void)
{
  return m_cells;
}

AutoMapPlayerNotes &AutoMapRecorder::Notes(void)
{
  return m_notes;
}

AutoMapMonsterDamage &AutoMapRecorder::Damage(void)
{
  return m_damage;
}

AutoMapLinkWatch &AutoMapRecorder::LinkWatch(void)
{
  return m_linkWatch;
}

AutoMapFalseWallWatch &AutoMapRecorder::FalseWallWatch(void)
{
  return m_falseWallWatch;
}
