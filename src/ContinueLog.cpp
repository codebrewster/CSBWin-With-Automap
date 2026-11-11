// ContinueLog.cpp : makes a recording that ends with an inventory Save and Quit ready to be
// replayed to its end and recorded on (ContinueLog.h).
#include "ContinueLog.h"

#include <cstdio>
#include <vector>

namespace
{

struct Line
{
  std::string text;     // without its line end
  std::string end;      // "\n", "\r\n" or "" for a last line without one
  bool isEvent;         // "time x y code random"; # lines and others are not
  unsigned time, x, y, code, random;
};

const unsigned DISK_MENU = 0x008c;     // the disk icon of the inventory
const unsigned FIRST_OPTION = 0x00d2;  // the options of a menu, first to fourth
const unsigned SECOND_OPTION = 0x00d3; // in the disk menu: Save and Quit
const unsigned LAST_OPTION = 0x00d5;
const unsigned MOUSE_UP = 0x0add;
const unsigned SAVE_SIGNATURE = 0x6668;
const unsigned SAVE_EFFECTS = 0x2223;  // does what a save does to the game

bool IsMenuClick(unsigned code)
{
  return code >= FIRST_OPTION && code <= LAST_OPTION;
}

// Written by the recorder on its own, with the random number it changes: kept as they are
bool IsSignature(unsigned code)
{
  return code == 0x2221 || code == 0x6665 || code == 0x6666 || code == 0x6667 || code == 0x666a;
}

std::vector<Line> SplitLines(const std::string &log)
{
  std::vector<Line> lines;
  size_t start = 0;
  while (start < log.size())
  {
    size_t newline = log.find('\n', start);
    Line line;
    if (newline == std::string::npos)
    {
      line.text = log.substr(start);
      start = log.size();
    }
    else
    {
      line.text = log.substr(start, newline - start);
      line.end = "\n";
      start = newline + 1;
    }
    if (!line.text.empty() && line.text[line.text.size() - 1] == '\r')
    {
      line.text.erase(line.text.size() - 1);
      line.end = "\r" + line.end;
    }
    char extra;
    line.isEvent = line.text.size() > 0 && line.text[0] != '#'
                && std::sscanf(line.text.c_str(), "%x %x %x %x %x %c", &line.time, &line.x, &line.y,
                               &line.code, &line.random, &extra) == 5;
    lines.push_back(line);
  }
  return lines;
}

} // namespace

std::string ContinueLog_Prepare(const std::string &log, bool *changed)
{
  *changed = false;
  std::vector<Line> lines = SplitLines(log);
  // The last time the disk menu was opened, and what followed it
  int diskMenu = -1;
  for (int i = (int)lines.size() - 1; i >= 0; i--)
  {
    if (lines[i].isEvent && lines[i].code == DISK_MENU) { diskMenu = i; break; }
  }
  if (diskMenu < 0) return log;
  int firstOption = -1;
  int save = -1;
  for (int i = diskMenu + 1; i < (int)lines.size(); i++)
  {
    const Line &line = lines[i];
    if (!line.isEvent) continue;
    if (IsMenuClick(line.code)) { if (firstOption < 0) firstOption = i; continue; }
    if (line.code == SAVE_SIGNATURE && save < 0) { save = i; continue; }
    if (line.code == MOUSE_UP || IsSignature(line.code)) continue;
    return log; // the game went on after the disk menu
  }
  if (firstOption < 0 || lines[firstOption].code != SECOND_OPTION || save < 0) return log;
  for (int i = save + 1; i < (int)lines.size(); i++)
  {
    if (lines[i].isEvent && lines[i].code != MOUSE_UP && !IsSignature(lines[i].code)) return log;
  }
  std::string out;
  for (int i = 0; i < (int)lines.size(); i++)
  {
    const Line &line = lines[i];
    bool menuInput = i >= diskMenu && i < save && line.isEvent
                  && (line.code == DISK_MENU || IsMenuClick(line.code) || line.code == MOUSE_UP);
    if (i == save)
    {
      char effects[64];
      std::snprintf(effects, sizeof effects, "%08x 0000 0000 %04x %08x", line.time, SAVE_EFFECTS,
                    line.random);
      out += effects + line.end;
    }
    out += (menuInput ? "#" : "") + line.text + line.end;
  }
  *changed = true;
  return out;
}
