#include "TestSupport.h"
#include "../AutoMapPlayerNotes.h"

static AutoMapLinkSource Floor(int level, int x, int y)
{
  AutoMapLinkSource source = {AutoMapCell{level, x, y}, -1};
  return source;
}

static AutoMapLinkSource Face(int level, int x, int y, int side)
{
  AutoMapLinkSource source = {AutoMapCell{level, x, y}, side};
  return source;
}

static void PitMarksSetAndClear(void)
{
  AutoMapPlayerNotes notes;
  AutoMapCell cell = {2, 5, 6};
  CHECK(notes.PitAt(cell) == -1);
  notes.SetPit(cell, true);
  CHECK(notes.PitAt(cell) == 1);
  notes.SetPit(cell, false);
  CHECK(notes.PitAt(cell) == 0);
  notes.ClearPit(cell);
  CHECK(notes.PitAt(cell) == -1);
  CHECK(notes.Pits().empty());
}

static void ToggleAddsThenRemoves(void)
{
  AutoMapPlayerNotes notes;
  AutoMapLinkSource source = Floor(1, 3, 3);
  AutoMapCell target = {1, 7, 8};
  CHECK(notes.ToggleLinkTarget(source, target));
  CHECK(notes.LinksFrom(source).size() == 1);
  CHECK(notes.LinksFrom(source)[0].x == 7);
  CHECK(!notes.ToggleLinkTarget(source, target));
  CHECK(notes.LinksFrom(source).empty());
  CHECK(notes.Links().empty());
}

static void AddIsIdempotent(void)
{
  AutoMapPlayerNotes notes;
  AutoMapLinkSource source = Floor(1, 3, 3);
  AutoMapCell target = {1, 7, 8};
  notes.AddLinkTarget(source, target);
  notes.AddLinkTarget(source, target);
  CHECK(notes.LinksFrom(source).size() == 1);
  CHECK(notes.Links().size() == 1);
  // A toggle after the adds removes the one link
  CHECK(!notes.ToggleLinkTarget(source, target));
  CHECK(notes.Links().empty());
}

static void FloorAndFaceSourcesAreSeparate(void)
{
  AutoMapPlayerNotes notes;
  AutoMapCell target = {1, 7, 8};
  AutoMapCell other = {1, 9, 9};
  notes.ToggleLinkTarget(Floor(1, 3, 3), target);
  notes.ToggleLinkTarget(Face(1, 3, 3, 2), other);
  CHECK(notes.LinksFrom(Floor(1, 3, 3)).size() == 1);
  CHECK(notes.LinksFrom(Floor(1, 3, 3))[0].x == 7);
  CHECK(notes.LinksFrom(Face(1, 3, 3, 2)).size() == 1);
  CHECK(notes.LinksFrom(Face(1, 3, 3, 2))[0].x == 9);
  CHECK(notes.LinksFrom(Face(1, 3, 3, 1)).empty());
}

static void RemoveLinksOfTakesEveryEndOfTheCell(void)
{
  AutoMapPlayerNotes notes;
  AutoMapCell cell = {1, 3, 3};
  notes.ToggleLinkTarget(Floor(1, 3, 3), AutoMapCell{1, 7, 8});
  notes.ToggleLinkTarget(Face(1, 3, 3, 2), AutoMapCell{1, 9, 9});
  notes.ToggleLinkTarget(Floor(1, 5, 5), cell);
  notes.ToggleLinkTarget(Floor(1, 5, 5), AutoMapCell{1, 6, 6});
  notes.ToggleLinkTarget(Floor(1, 4, 4), cell);
  // The same place on another level is another cell
  notes.ToggleLinkTarget(Floor(2, 3, 3), AutoMapCell{2, 7, 8});
  notes.RemoveLinksOf(cell);
  CHECK(notes.LinksFrom(Floor(1, 3, 3)).empty());
  CHECK(notes.LinksFrom(Face(1, 3, 3, 2)).empty());
  CHECK(notes.LinksFrom(Floor(1, 5, 5)).size() == 1);
  CHECK(notes.LinksFrom(Floor(1, 5, 5))[0].x == 6);
  CHECK(notes.LinksFrom(Floor(1, 4, 4)).empty());
  CHECK(notes.LinksFrom(Floor(2, 3, 3)).size() == 1);
  CHECK(notes.Links().size() == 2);
}

static void ForgetLevelKeepsOthers(void)
{
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{1, 2, 2}, true);
  notes.SetPit(AutoMapCell{2, 2, 2}, false);
  notes.ToggleLinkTarget(Floor(1, 3, 3), AutoMapCell{1, 4, 4});
  notes.ToggleLinkTarget(Floor(2, 3, 3), AutoMapCell{2, 4, 4});
  notes.ForgetLevel(1);
  CHECK(notes.PitAt(AutoMapCell{1, 2, 2}) == -1);
  CHECK(notes.PitAt(AutoMapCell{2, 2, 2}) == 0);
  CHECK(notes.LinksOfLevel(1).empty());
  CHECK(notes.LinksOfLevel(2).size() == 1);
  notes.Forget();
  CHECK(notes.Pits().empty());
  CHECK(notes.Links().empty());
}

static void OrderIsStable(void)
{
  AutoMapPlayerNotes notes;
  notes.SetPit(AutoMapCell{2, 1, 1}, true);
  notes.SetPit(AutoMapCell{1, 5, 1}, true);
  notes.SetPit(AutoMapCell{1, 2, 1}, true);
  notes.SetPit(AutoMapCell{1, 9, 0}, true);
  std::vector<std::pair<AutoMapCell, bool> > pits = notes.Pits();
  CHECK(pits.size() == 4);
  CHECK(pits[0].first.y == 0);
  CHECK(pits[1].first.x == 2);
  CHECK(pits[2].first.x == 5);
  CHECK(pits[3].first.level == 2);

  notes.ToggleLinkTarget(Face(1, 4, 4, 3), AutoMapCell{1, 9, 9});
  notes.ToggleLinkTarget(Face(1, 4, 4, 0), AutoMapCell{1, 8, 2});
  notes.ToggleLinkTarget(Floor(1, 4, 4), AutoMapCell{1, 7, 7});
  notes.ToggleLinkTarget(Floor(1, 4, 4), AutoMapCell{1, 1, 7});
  notes.ToggleLinkTarget(Floor(1, 1, 1), AutoMapCell{1, 0, 0});
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > links = notes.LinksOfLevel(1);
  CHECK(links.size() == 5);
  CHECK(links[0].first.cell.x == 1);
  CHECK(links[1].first.side == -1);
  CHECK(links[1].second.x == 1);
  CHECK(links[2].second.x == 7);
  CHECK(links[3].first.side == 0);
  CHECK(links[4].first.side == 3);
  CHECK(notes.Links().size() == 5);
}

void TestAutoMapPlayerNotes(void)
{
  PitMarksSetAndClear();
  ToggleAddsThenRemoves();
  AddIsIdempotent();
  FloorAndFaceSourcesAreSeparate();
  RemoveLinksOfTakesEveryEndOfTheCell();
  ForgetLevelKeepsOthers();
  OrderIsStable();
}
