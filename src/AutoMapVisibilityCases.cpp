#include "AutoMapVisibilityCases.h"

#include <cstdio>
#include <sstream>

static const char *const SIDE_NAMES[4] = {"N", "E", "S", "W"};
// The engine's quadrants of a tile, clockwise from the north-west
static const char *const CORNER_NAMES[4] = {"NW", "NE", "SE", "SW"};

const char *AutoMapVisibilityCases_ThingName(AutoMapVisibilityThing thing)
{
  switch (thing)
  {
  case AutoMapVisibilityDecoration: return "decoration";
  case AutoMapVisibilityItem: return "item";
  case AutoMapVisibilityMonster: return "monster";
  case AutoMapVisibilityDoor: return "door";
  case AutoMapVisibilityAlcove: return "alcove";
  case AutoMapVisibilityTile: return "tile";
  case AutoMapVisibilityFloor: return "floor";
  case AutoMapVisibilityPortrait: return "portrait";
  }
  return "?";
}

const char *AutoMapVisibilityCases_VerdictName(AutoMapVisibilityVerdict verdict)
{
  switch (verdict)
  {
  case AutoMapVisibilityNotSeen: return "not seen";
  case AutoMapVisibilityGlimpsed: return "glimpsed";
  case AutoMapVisibilitySeen: return "seen";
  }
  return "?";
}

const char *AutoMapVisibilityCases_SideName(int side)
{
  return side >= 0 && side < 4 ? SIDE_NAMES[side] : "?";
}

const char *AutoMapVisibilityCases_CornerName(int corner)
{
  return corner >= 0 && corner < 4 ? CORNER_NAMES[corner] : "?";
}

std::string AutoMapVisibilityCases_CheckLine(const AutoMapVisibilityCheck &check,
                                             AutoMapVisibilityVerdict verdict)
{
  char line[96];
  bool hasSide = check.thing == AutoMapVisibilityDecoration || check.thing == AutoMapVisibilityAlcove
                 || check.thing == AutoMapVisibilityPortrait;
  if (hasSide)
  {
    // An item in an alcove is named after the face, as on the floor after the tile
    std::string name = check.itemName.empty() ? "" : " \"" + check.itemName + "\"";
    std::snprintf(line, sizeof(line), "%s (%d,%d) %s%s %s", AutoMapVisibilityCases_ThingName(check.thing),
                  check.x, check.y, AutoMapVisibilityCases_SideName(check.side), name.c_str(),
                  AutoMapVisibilityCases_VerdictName(verdict));
  }
  else
  {
    std::snprintf(line, sizeof(line), "%s (%d,%d)", AutoMapVisibilityCases_ThingName(check.thing),
                  check.x, check.y);
  }
  std::string text = line;
  if (hasSide) return text;
  if (check.corner >= 0) text = text + " " + AutoMapVisibilityCases_CornerName(check.corner);
  if (!check.itemName.empty()) text += " \"" + check.itemName + "\"";
  return text + " " + AutoMapVisibilityCases_VerdictName(verdict);
}

static int CornerOfName(const std::string &name)
{
  for (int corner = 0; corner < 4; corner++)
  {
    if (name == CORNER_NAMES[corner]) return corner;
  }
  return -1;
}

// An item's optional corner and "name" before its verdict; what follows them stays in words
static bool ReadItemPlace(std::istringstream &words, AutoMapVisibilityCheck *check)
{
  std::string rest;
  std::getline(words, rest);
  std::string::size_type at = rest.find_first_not_of(" \t");
  if (at == std::string::npos) return false;
  std::string::size_type wordEnd = rest.find_first_of(" \t", at);
  int corner = CornerOfName(rest.substr(at, wordEnd == std::string::npos ? std::string::npos : wordEnd - at));
  if (corner >= 0)
  {
    check->corner = corner;
    at = wordEnd == std::string::npos ? rest.size() : rest.find_first_not_of(" \t", wordEnd);
    if (at == std::string::npos) at = rest.size();
  }
  if (at < rest.size() && rest[at] == '"')
  {
    std::string::size_type close = rest.find('"', at + 1);
    if (close == std::string::npos || close == at + 1) return false;
    check->itemName = rest.substr(at + 1, close - at - 1);
    at = close + 1;
  }
  words.clear();
  words.str(rest.substr(at));
  return true;
}

static int SideOfName(const std::string &name)
{
  for (int side = 0; side < 4; side++)
  {
    if (name == SIDE_NAMES[side]) return side;
  }
  return -1;
}

// "(12,5)"
static bool ReadCell(std::istringstream &words, int *x, int *y)
{
  std::string word;
  if (!(words >> word)) return false;
  char rest = 0;
  return std::sscanf(word.c_str(), "(%d,%d%c", x, y, &rest) == 3 && rest == ')';
}

static bool ReadVerdict(std::istringstream &words, AutoMapVisibilityVerdict *verdict)
{
  std::string word;
  if (!(words >> word)) return false;
  if (word == "seen") *verdict = AutoMapVisibilitySeen;
  else if (word == "glimpsed") *verdict = AutoMapVisibilityGlimpsed;
  else if (word == "not")
  {
    if (!(words >> word) || word != "seen") return false;
    *verdict = AutoMapVisibilityNotSeen;
  }
  else return false;
  return true;
}

static bool IsAtEnd(std::istringstream &words)
{
  std::string word;
  return !(words >> word);
}

// at L0 (12,7) facing N light 4
static bool ReadCase(std::istringstream &words, AutoMapVisibilityCase *visibilityCase)
{
  std::string word;
  if (!(words >> word) || word.size() < 2 || word[0] != 'L') return false;
  char rest = 0;
  if (std::sscanf(word.c_str() + 1, "%d%c", &visibilityCase->level, &rest) != 1) return false;
  if (visibilityCase->level < 0) return false;
  if (!ReadCell(words, &visibilityCase->x, &visibilityCase->y)) return false;
  if (!(words >> word) || word != "facing") return false;
  if (!(words >> word) || (visibilityCase->facing = SideOfName(word)) < 0) return false;
  if (!(words >> word) || word != "light") return false;
  if (!(words >> visibilityCase->light) || visibilityCase->light < 0 || visibilityCase->light > 5)
  {
    return false;
  }
  return IsAtEnd(words);
}

static bool ReadCheck(const std::string &thingName, std::istringstream &words,
                      AutoMapVisibilityCheck *check)
{
  if (thingName == "decoration") check->thing = AutoMapVisibilityDecoration;
  else if (thingName == "item") check->thing = AutoMapVisibilityItem;
  else if (thingName == "monster") check->thing = AutoMapVisibilityMonster;
  else if (thingName == "door") check->thing = AutoMapVisibilityDoor;
  else if (thingName == "alcove") check->thing = AutoMapVisibilityAlcove;
  else if (thingName == "tile") check->thing = AutoMapVisibilityTile;
  else if (thingName == "floor") check->thing = AutoMapVisibilityFloor;
  else if (thingName == "portrait") check->thing = AutoMapVisibilityPortrait;
  else return false;
  if (!ReadCell(words, &check->x, &check->y)) return false;
  check->side = -1;
  check->corner = -1;
  check->itemName.clear();
  if (check->thing == AutoMapVisibilityItem && !ReadItemPlace(words, check)) return false;
  if (check->thing == AutoMapVisibilityDecoration || check->thing == AutoMapVisibilityAlcove
      || check->thing == AutoMapVisibilityPortrait)
  {
    std::string side;
    if (!(words >> side) || (check->side = SideOfName(side)) < 0) return false;
    // One item of an alcove: alcove (36,40) W "BREAD" seen
    if (check->thing == AutoMapVisibilityAlcove && !ReadItemPlace(words, check)) return false;
    if (check->corner >= 0) return false;
  }
  if (!ReadVerdict(words, &check->expected)) return false;
  // Only what the map can show as something in the dark is glimpsed
  bool canBeGlimpsed = check->thing == AutoMapVisibilityDecoration || check->thing == AutoMapVisibilityItem;
  if (check->expected == AutoMapVisibilityGlimpsed && !canBeGlimpsed) return false;
  return IsAtEnd(words);
}

static std::string LineError(int lineNumber, const char *what)
{
  char text[256];
  std::snprintf(text, sizeof(text), "line %d: %s", lineNumber, what);
  return text;
}

bool AutoMapVisibilityCases_Parse(const std::string &text, AutoMapVisibilityCases *cases,
                                  std::string *error)
{
  *cases = AutoMapVisibilityCases();
  error->clear();
  std::istringstream lines(text);
  std::string line;
  int lineNumber = 0;
  while (std::getline(lines, line))
  {
    lineNumber++;
    std::string::size_type comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    std::istringstream words(line);
    std::string first;
    if (!(words >> first)) continue;
    if (first == "save")
    {
      AutoMapVisibilitySection section;
      section.lineNumber = lineNumber;
      if (!(words >> section.save) || !IsAtEnd(words))
      {
        *error = LineError(lineNumber, "expected save <name of the saved game>");
        return false;
      }
      cases->sections.push_back(section);
    }
    else if (first == "at")
    {
      AutoMapVisibilityCase visibilityCase;
      visibilityCase.lineNumber = lineNumber;
      if (cases->sections.empty())
      {
        *error = LineError(lineNumber, "a case needs a save line above it");
        return false;
      }
      if (!ReadCase(words, &visibilityCase))
      {
        *error = LineError(lineNumber, "expected at L<level> (<x>,<y>) facing <N|E|S|W> light <0..5>");
        return false;
      }
      cases->sections.back().cases.push_back(visibilityCase);
    }
    else
    {
      AutoMapVisibilityCheck check;
      check.lineNumber = lineNumber;
      if (cases->sections.empty() || cases->sections.back().cases.empty())
      {
        *error = LineError(lineNumber, "a check needs an at line before it");
        return false;
      }
      if (!ReadCheck(first, words, &check))
      {
        *error = LineError(lineNumber, "expected decoration (<x>,<y>) <N|E|S|W>, item (<x>,<y>) "
                                       "[NW|NE|SE|SW] [\"<name>\"], alcove (<x>,<y>) <N|E|S|W>, monster, door or "
                                       "tile (<x>,<y>), then "
                                       "seen, glimpsed or not seen");
        return false;
      }
      cases->sections.back().cases.back().checks.push_back(check);
    }
  }
  if (cases->sections.empty())
  {
    *error = "the file names no save";
    return false;
  }
  return true;
}
