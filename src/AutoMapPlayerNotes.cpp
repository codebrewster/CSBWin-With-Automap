// AutoMapPlayerNotes.cpp : the player's own hidden-pit marks and links.
#include "AutoMapPlayerNotes.h"

#include <climits>

bool AutoMapPlayerNotes::Key::operator<(const Key &other) const
{
  if (level != other.level) return level < other.level;
  if (y != other.y) return y < other.y;
  if (x != other.x) return x < other.x;
  return side < other.side;
}

AutoMapPlayerNotes::Key AutoMapPlayerNotes::KeyOf(const AutoMapCell &cell, int side)
{
  Key key = {cell.level, cell.y, cell.x, side};
  return key;
}

AutoMapCell AutoMapPlayerNotes::CellOf(const Key &key)
{
  AutoMapCell cell = {key.level, key.x, key.y};
  return cell;
}

AutoMapLinkSource AutoMapPlayerNotes::SourceOf(const Key &key)
{
  AutoMapLinkSource source = {CellOf(key), key.side};
  return source;
}

void AutoMapPlayerNotes::Forget(void)
{
  m_pits.clear();
  m_links.clear();
}

void AutoMapPlayerNotes::ForgetLevel(int level)
{
  Key first = {level, INT_MIN, INT_MIN, INT_MIN};
  Key past = {level + 1, INT_MIN, INT_MIN, INT_MIN};
  m_pits.erase(m_pits.lower_bound(first), m_pits.lower_bound(past));
  m_links.erase(m_links.lower_bound(first), m_links.lower_bound(past));
}

void AutoMapPlayerNotes::SetPit(const AutoMapCell &cell, bool open)
{
  m_pits[KeyOf(cell, -1)] = open;
}

void AutoMapPlayerNotes::ClearPit(const AutoMapCell &cell)
{
  m_pits.erase(KeyOf(cell, -1));
}

int AutoMapPlayerNotes::PitAt(const AutoMapCell &cell) const
{
  PitMap::const_iterator found = m_pits.find(KeyOf(cell, -1));
  if (found == m_pits.end()) return -1;
  return found->second ? 1 : 0;
}

bool AutoMapPlayerNotes::ToggleLinkTarget(const AutoMapLinkSource &source,
                                          const AutoMapCell &target)
{
  Key sourceKey = KeyOf(source.cell, source.side);
  Key targetKey = KeyOf(target, -1);
  std::set<Key> &targets = m_links[sourceKey];
  if (targets.erase(targetKey) != 0)
  {
    // No empty entry stays behind, so Links() lists only real links
    if (targets.empty()) m_links.erase(sourceKey);
    return false;
  }
  targets.insert(targetKey);
  return true;
}

void AutoMapPlayerNotes::AddLinkTarget(const AutoMapLinkSource &source, const AutoMapCell &target)
{
  m_links[KeyOf(source.cell, source.side)].insert(KeyOf(target, -1));
}

void AutoMapPlayerNotes::RemoveLinksOf(const AutoMapCell &cell)
{
  Key targetKey = KeyOf(cell, -1);
  for (LinkMap::iterator it = m_links.begin(); it != m_links.end();)
  {
    bool startsHere = it->first.level == cell.level && it->first.y == cell.y && it->first.x == cell.x;
    if (!startsHere) it->second.erase(targetKey);
    // No empty entry stays behind, so Links() lists only real links
    if (startsHere || it->second.empty()) m_links.erase(it++);
    else ++it;
  }
}

std::vector<AutoMapCell> AutoMapPlayerNotes::LinksFrom(const AutoMapLinkSource &source) const
{
  std::vector<AutoMapCell> result;
  LinkMap::const_iterator found = m_links.find(KeyOf(source.cell, source.side));
  if (found == m_links.end()) return result;
  for (std::set<Key>::const_iterator it = found->second.begin(); it != found->second.end(); ++it)
  {
    result.push_back(CellOf(*it));
  }
  return result;
}

void AutoMapPlayerNotes::Collect(const LinkMap::const_iterator &from,
                                 const LinkMap::const_iterator &to,
                                 std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > *out)
{
  for (LinkMap::const_iterator it = from; it != to; ++it)
  {
    for (std::set<Key>::const_iterator target = it->second.begin(); target != it->second.end();
         ++target)
    {
      out->push_back(std::make_pair(SourceOf(it->first), CellOf(*target)));
    }
  }
}

std::vector<std::pair<AutoMapLinkSource, AutoMapCell> >
AutoMapPlayerNotes::LinksOfLevel(int level) const
{
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > result;
  Key first = {level, INT_MIN, INT_MIN, INT_MIN};
  Key past = {level + 1, INT_MIN, INT_MIN, INT_MIN};
  Collect(m_links.lower_bound(first), m_links.lower_bound(past), &result);
  return result;
}

std::vector<std::pair<AutoMapCell, bool> > AutoMapPlayerNotes::Pits(void) const
{
  std::vector<std::pair<AutoMapCell, bool> > result;
  for (PitMap::const_iterator it = m_pits.begin(); it != m_pits.end(); ++it)
  {
    result.push_back(std::make_pair(CellOf(it->first), it->second));
  }
  return result;
}

std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > AutoMapPlayerNotes::Links(void) const
{
  std::vector<std::pair<AutoMapLinkSource, AutoMapCell> > result;
  Collect(m_links.begin(), m_links.end(), &result);
  return result;
}
