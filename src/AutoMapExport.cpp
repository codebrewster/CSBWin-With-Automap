// AutoMapExport.cpp : writes what the map shows, per level, vision and spoiler level: into a
// folder for the reference mockup in docs/automap-mockup (pack-data.py turns the files into its
// data block), and into one web page for the map window's EXPORT.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "AutoMapLayout.h"
#include "AutoMapBar.h"
#include "AutoMapExport.h"
#include "HeroStatsHooks.h"
#include "AutoMapExportDialog.h"
#include "AutoMapHtml.h"
#include "AutoMapLevelReader.h"
#include "AutoMapRecorder.h"
#include "AutoMapScene.h"
#include "AutoMapSprites.h"
#include "AutoMapWindowTooltip.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

bool AutoMapExport_IsRequested(void)
{
  return getenv("CSB_MAP_EXPORT") != NULL || getenv("CSB_MAP_RENDER") != NULL || getenv("CSB_MAP_HTML") != NULL;
}

// The draw list the map window would get for level at a vision (0 fog, 1 explored live, 2 whole
// level) and a spoiler level of the scene (0 All, 1 Learned, 2 Seen)
static AutoMapDrawList DrawListOf(AutoMapRecorder &recorder, int partyLevel, const AutoMapLevelSnapshot &s,
                                  int vision, int spoil)
{
  static const AutoMapVisibleCells nothingVisibleNow;
  static const std::vector<AutoMapRecordId> noMonstersSeenNow;
  const AutoMapVisibleCells *visibleNow = s.level == partyLevel ? &recorder.VisibleNow() : &nothingVisibleNow;
  const std::vector<AutoMapRecordId> *seenNow = s.level == partyLevel ? &recorder.MonstersSeenNow() : &noMonstersSeenNow;
  AutoMapFog fog = {vision != 2, &recorder.Explored(), visibleNow, vision == 2, (AutoMapSpoilerLevel)spoil, &recorder.Knowledge(),
                    &recorder.Items(), seenNow, &recorder.Cells(), &recorder.Notes(), &recorder.Damage(), vision == 1,
                    &HeroStats_Store().TilesWalked()};   // the page's WALKED switches the tint
  return AutoMapScene_BuildDrawList(s, fog);
}

static std::string JsonText(const std::string &s)
{
  std::string out = "\"";
  for (size_t i = 0; i < s.size(); i++)
  {
    unsigned char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c < 32) { char buf[8]; sprintf(buf, "\\u%04x", c); out += buf; }
    else if (c < 128) out += c;
  }
  return out + "\"";
}

static void Append(std::string &out, const char *format, ...)
{
  char small[512];
  va_list args;
  va_start(args, format);
  int n = vsnprintf(small, sizeof(small), format, args);
  va_end(args);
  if (n < 0) return;
  if ((size_t)n < sizeof(small))
  {
    out.append(small, (size_t)n);
    return;
  }
  std::vector<char> big((size_t)n + 1);
  va_start(args, format);
  vsnprintf(big.data(), big.size(), format, args);
  va_end(args);
  out.append(big.data(), (size_t)n);
}

// The pictures the draw lists name. Into a folder they go as .bmp files (CSB_MAP_EXPORT, for
// pack-data.py); into the web page as PNG data, where a picture that comes out the same on
// several levels is kept once, under the name it got first.
struct ExportPictures
{
  std::string dir;
  bool intoPage;
  int level;
  // The level's colour schemes, carried by each request in the page (no global change)
  int scheme9, scheme10;
  std::map<std::string, std::string> nameOf;     // name of a request -> name in the page
  std::map<std::string, std::string> nameOfPng;  // PNG bytes -> name in the page
  std::map<std::string, std::string> pngOfName;  // name in the page -> PNG bytes
  ExportPictures(void) : intoPage(false), level(0), scheme9(-1), scheme10(-1) {}
  std::string Save(AutoMapSpriteRequest r)
  {
    char name[128];
    if (r.hasTile) sprintf(name, "L%d_p%d_t%d_x%d_y%d%s.bmp", level, r.picture, r.transparentColour, r.tileX, r.tileY, r.portraitPalette ? "_pp" : "");
    else sprintf(name, "L%d_p%d_t%d.bmp", level, r.picture, r.transparentColour);
    std::string n = name;
    std::map<std::string, std::string>::const_iterator known = nameOf.find(n);
    if (known != nameOf.end()) return known->second;
    if (!intoPage)
    {
      nameOf[n] = AutoMapSprites_SaveBmp(r, (dir + "/" + n).c_str()) ? n : "";
      return nameOf[n];
    }
    if (scheme9 >= 0) r.scheme9PlusOne = scheme9 + 1;
    if (scheme10 >= 0) r.scheme10PlusOne = scheme10 + 1;
    std::vector<Uint8> rgba;
    int w = 0, h = 0;
    std::string png;
    if (AutoMapSprites_Pixels(r, &rgba, &w, &h)) png = AutoMapHtml_Png(rgba.data(), w, h);
    if (png.empty())
    {
      nameOf[n] = "";
      return "";
    }
    std::map<std::string, std::string>::const_iterator same = nameOfPng.find(png);
    if (same != nameOfPng.end()) n = same->second;
    else
    {
      nameOfPng[png] = n;
      pngOfName[n] = png;
    }
    nameOf[name] = n;
    return n;
  }
  std::string Plain(int picture, int tc) { if (picture < 0) return ""; AutoMapSpriteRequest r = {picture, tc, false, 0, 0, 0, 0, false, 0, 0}; return Save(r); }
  std::string Icon(const AutoMapItem &item)
  {
    if (!item.hasIcon) return "";
    AutoMapSpriteRequest r = {item.iconGraphic, -1, true, (item.iconTile & 15) * 16, (item.iconTile >> 4) * 16, 16, 16, true, 0, 0};
    return Save(r);
  }
  std::string Portrait(int p)
  {
    if (p < 0 || p >= 24) return "";
    AutoMapSpriteRequest r = {26, 1, true, (p & 7) * 32, (p >> 3) * 29, 32, 29, false, 0, 0};
    return Save(r);
  }
};

static std::string ItemArr(ExportPictures &sp, const AutoMapItem &item)
{
  const char *name = item.nameIndex >= 0 ? AutoMapLevelReader_ItemName(item.nameIndex) : NULL;
  // A potion's power rune goes first, as in the window's tooltip
  std::string named = name ? name : "";
  const char *rune = AutoMapScene_PotionPowerName(item.potionPower);
  if (rune[0] != '\0' && !named.empty()) named = std::string(rune) + " " + named;
  // The kind last, for the mockup's show choices (an AutoMapItemKind)
  return "[" + JsonText(sp.Plain(item.worldPicture, 10)) + "," + JsonText(sp.Icon(item)) + "," +
         JsonText(named) + "," + std::to_string((int)AutoMapScene_KindOfItem(item)) + "]";
}

// Tooltip lines as {"t":text,"s":style,"i":[item pictures]}; the text keeps its glyph codes
// (\x01\x01 party, \x02\x02 claw, \x03\x03 box, \x04<n> item n of "i", \x05<facing> turned arrow)
static std::string TipArr(ExportPictures &sp, const std::vector<AutoMapTipLine> &v)
{
  std::string s = "[";
  for (size_t i = 0; i < v.size(); i++)
  {
    s += (i ? "," : "") + std::string("{\"t\":") + JsonText(v[i].text) + ",\"s\":" + std::to_string((int)v[i].style);
    if (!v[i].items.empty())
    {
      s += ",\"i\":[";
      for (size_t k = 0; k < v[i].items.size(); k++) s += (k ? "," : "") + ItemArr(sp, v[i].items[k]);
      s += "]";
    }
    s += "}";
  }
  return s + "]";
}

static std::string LinkArr(const std::vector<AutoMapLink> &v)
{
  std::ostringstream o;
  o << "[";
  for (size_t i = 0; i < v.size(); i++)
  {
    const AutoMapLink &l = v[i];
    o << (i ? "," : "") << "[" << l.srcX << "," << l.srcY << "," << l.dstX << "," << l.dstY << "," << l.srcIsWall << ","
      << l.srcSide << "," << (int)l.signal << "," << l.onceOnly << "," << l.delay << "," << l.fromCounter << "]";
  }
  o << "]";
  return o.str();
}

static void WriteDrawList(std::string &f, ExportPictures &sp, const AutoMapDrawList &l, const AutoMapLevelSnapshot &snap,
                          const std::vector<AutoMapMonsterChoice> &types)
{
  Append(f, "{\"spoilers\":%d,\"party\":[%d,%d,%d,%d],\"cells\":[\n", (int)l.spoilers, l.party.x, l.party.y, (int)l.party.facing, l.partyIsOnShownLevel);
  for (size_t i = 0; i < l.cells.size(); i++)
  {
    const AutoMapShownCell &sc = l.cells[i];
    const AutoMapCellRecord &c = sc.cell;
    bool linked = false;
    for (size_t k = 0; k < l.linkedCells.size(); k++) if (l.linkedCells[k].x == sc.x && l.linkedCells[k].y == sc.y) linked = true;
    std::ostringstream o;
    o << "{\"x\":" << sc.x << ",\"y\":" << sc.y << ",\"k\":" << (int)c.kind;
    if (c.isOutsideLevel) o << ",\"out\":1";
    if (c.floorPicture != AUTOMAP_NO_PICTURE) o << ",\"fp\":" << JsonText(sp.Plain(c.floorPicture, 10)) << ",\"fpn\":" << c.floorPicture << ",\"flav\":" << AutoMapScene_FloorPictureIsFlavour(c.floorPicture);
    if (c.kind == AutoMapCellDoor)
      o << ",\"door\":[" << (int)c.doorState << "," << c.runsNorthSouth << "," << c.doorBreakableWithAxe << "," << c.doorBreakableWithFireball << ","
        << (l.spoilers == AutoMapSpoilersAll && AutoMapScene_DoorIsSealed(c, linked)) << "," << c.doorHasSwitch << "]";
    if (c.kind == AutoMapCellStairs) o << ",\"st\":[" << c.stairsLeadUp << "," << c.runsNorthSouth << "]";
    if (c.kind == AutoMapCellPit || c.pitIsClosedSwitchable) o << ",\"pit\":[" << c.pitIsHidden << "," << c.pitIsOpen << "," << c.pitIsClosedSwitchable << "]";
    if (c.kind == AutoMapCellTeleporter || c.kind == AutoMapCellInvisibleTeleporter || c.kind == AutoMapCellDormantTeleporter)
      o << ",\"tele\":[" << c.teleporterDestLevel << "," << c.teleporterDestX << "," << c.teleporterDestY << "," << c.teleporterScope << ","
        << c.teleporterRotation << "," << c.teleporterFacingMode << "," << c.hasTeleporterTarget << "," << sc.teleporterTargetVisible << "]";
    if (c.kind == AutoMapCellSpinner || c.kind == AutoMapCellDormantSpinner) o << ",\"spin\":[" << c.spinnerFacing << "," << c.spinnerRotation << "]";
    if (linked) o << ",\"linked\":1";
    if (sc.isHighlighted) o << ",\"hl\":1";
    if (sc.isWalked) o << ",\"wk\":1";
    if (sc.playerPit >= 0) o << ",\"ppit\":" << sc.playerPit;
    bool anySide = false;
    for (int s = 0; s < 4; s++) { const AutoMapWallSide &w = c.wallSides[s]; if (w.picture != AUTOMAP_NO_PICTURE || w.hasWriting || w.hidden || w.portrait != AUTOMAP_NO_PICTURE || w.functional || w.isAlcove) anySide = true; }
    if (anySide)
    {
      o << ",\"sides\":[";
      for (int s = 0; s < 4; s++)
      {
        const AutoMapWallSide &w = c.wallSides[s];
        o << (s ? "," : "") << "[" << JsonText(sp.Plain(w.picture, 10)) << "," << (w.picture != AUTOMAP_NO_PICTURE) << "," << w.hasWriting << "," << w.functional << ","
          << w.hidden << "," << w.isAlcove << "," << JsonText(sp.Portrait(w.portrait)) << "," << (int)w.feature << "," << w.isTorchHolder << "]";
      }
      o << "]";
    }
    if (!c.itemPictures.empty())
    {
      o << ",\"items\":[";
      for (size_t k = 0; k < c.itemPictures.size(); k++) o << (k ? "," : "") << ItemArr(sp, c.itemPictures[k]);
      o << "],\"more\":" << sc.hasMoreItems << ",\"all\":" << sc.allItemPictures.size() << ",\"allKinds\":[";
      for (size_t k = 0; k < sc.allItemPictures.size(); k++) o << (k ? "," : "") << (int)AutoMapScene_KindOfItem(sc.allItemPictures[k]);
      o << "]";
    }
    if (!c.alcoveContents.empty())
    {
      o << ",\"alc\":[";
      for (size_t k = 0; k < c.alcoveContents.size(); k++) o << (k ? "," : "") << "[" << c.alcoveContents[k].side << "," << ItemArr(sp, c.alcoveContents[k].item) << "]";
      o << "]";
    }
    if (!c.wallLocks.empty())
    {
      o << ",\"locks\":[";
      for (size_t k = 0; k < c.wallLocks.size(); k++) o << (k ? "," : "") << "[" << c.wallLocks[k].side << "," << c.wallLocks[k].required << "," << ItemArr(sp, c.wallLocks[k].object) << "]";
      o << "]";
    }
    if (!c.wallRelays.empty()) o << ",\"relays\":" << c.wallRelays.size();
    o << "}";
    Append(f, "%s%s\n", i ? "," : "", o.str().c_str());
  }
  Append(f, "],\"monsters\":[");
  for (size_t i = 0; i < l.monsters.size(); i++)
  {
    const AutoMapShownMonster &m = l.monsters[i];
    std::ostringstream o;
    o << "{\"x\":" << m.x << ",\"y\":" << m.y << ",\"name\":" << JsonText(m.name) << ",\"m\":[";
    for (int k = 0; k < m.memberCount; k++)
      o << (k ? "," : "") << "[" << JsonText(sp.Plain(m.memberPictures[k].picture, m.transparentColour)) << "," << m.memberPictures[k].isMirrored << "," << (int)m.memberFacings[k] << "]";
    o << "],\"size\":" << m.size << ",\"pos\":" << m.positions << "}";
    Append(f, "%s%s", i ? "," : "", o.str().c_str());
  }
  Append(f, "],\"missiles\":[");
  for (size_t i = 0; i < l.missiles.size(); i++)
    Append(f, "%s[%d,%d,%s,%.2f]", i ? "," : "", l.missiles[i].x, l.missiles[i].y, JsonText(sp.Plain(l.missiles[i].item.worldPicture, 10)).c_str(),
            l.missiles[i].sizeShowsRange ? AutoMapScene_MissileScale(l.missiles[i].rangeRemaining) : 1.0f);
  Append(f, "],\"clouds\":[");
  for (size_t i = 0; i < l.clouds.size(); i++)
    Append(f, "%s[%d,%d,%s]", i ? "," : "", l.clouds[i].x, l.clouds[i].y, JsonText(sp.Plain(l.clouds[i].picture, 10)).c_str());
  Append(f, "],\"acts\":[");
  for (size_t i = 0; i < l.actuators.size(); i++)
  {
    const AutoMapFloorActuator &a = l.actuators[i];
    std::ostringstream o;
    o << "[" << a.x << "," << a.y << "," << (int)a.kind << "," << (int)a.op << "," << a.requiredFacing << "," << ItemArr(sp, a.object) << ","
      << JsonText(a.hasGeneratedMonster ? sp.Plain(a.generatedKind.frontPicture, a.generatedKind.transparentColour) : "") << "," << JsonText(a.generatedMonsterName) << "]";
    Append(f, "%s%s", i ? "," : "", o.str().c_str());
  }
  Append(f, "],\"links\":%s,\"plinks\":%s,\"glimpses\":[", LinkArr(l.links).c_str(), LinkArr(l.playerLinks).c_str());
  for (size_t i = 0; i < l.glimpsedItems.size(); i++) Append(f, "%s[%d,%d]", i ? "," : "", l.glimpsedItems[i].x, l.glimpsedItems[i].y);
  Append(f, "],\"gfaces\":[");
  for (size_t i = 0; i < l.glimpsedFaces.size(); i++) Append(f, "%s[%d,%d,%d]", i ? "," : "", l.glimpsedFaces[i].x, l.glimpsedFaces[i].y, l.glimpsedFaces[i].side);
  // what hovering a cell shows: the tooltip as the window builds it, and the lines it draws
  Append(f, "],\n\"hover\":{");
  bool firstHover = true;
  for (size_t i = 0; i < l.cells.size(); i++)
  {
    int x = l.cells[i].x, y = l.cells[i].y;
    std::vector<AutoMapTipLine> tipLines, tipLinesFull;
    std::vector<AutoMapTooltipEntry> tip, tipFull;
    AutoMapWindow_TooltipFor(l, x, y, false, &tipLines, &tip);
    AutoMapWindow_TooltipFor(l, x, y, true, &tipLinesFull, &tipFull);
    std::string lines = TipArr(sp, tipLines), linesFull = TipArr(sp, tipLinesFull);
    std::vector<AutoMapLink> from = AutoMapScene_LinksFrom(l, x, y), to = AutoMapScene_LinksTo(l, x, y);
    std::ostringstream o;
    bool any = false;
    if (!tipLines.empty())
    {
      o << "\"tip\":" << lines;
      if (linesFull != lines) o << ",\"tipFull\":" << linesFull;
      o << ",\"icons\":[";
      for (size_t k = 0; k < tip.size(); k++)
        o << (k ? "," : "") << "[" << ItemArr(sp, tip[k].item) << "," << tip[k].count << "," << tip[k].nested << "," << tip[k].predicted << "," << tip[k].required << "," << tip[k].carried << "," << tip[k].side << "]";
      o << "]";
      any = true;
    }
    if (!from.empty()) { o << (any ? "," : "") << "\"from\":" << LinkArr(from); any = true; }
    if (!to.empty()) { o << (any ? "," : "") << "\"to\":" << LinkArr(to); any = true; }
    if (l.spoilers == AutoMapSpoilersAll)
    {
      std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(l, x, y);
      bool relayCell = !AutoMapScene_RelayBlock(l, x, y, false).empty();
      if (!sources.empty() || relayCell)
      {
        std::vector<AutoMapLink> chain = AutoMapScene_PressChainLinks(sources);
        o << (any ? "," : "") << "\"chain\":" << LinkArr(chain) << ",\"relay\":" << relayCell << ",\"goals\":[";
        bool first = true;
        std::vector<const AutoMapEffect *> goals = AutoMapScene_PressGoalsByCell(sources);
        for (size_t g = 0; g < goals.size(); g++)
        {
          std::string badge = AutoMapScene_GoalBadge(goals[g]->distance);
          AutoMapGoalStatus st = goals[g]->distance.status;
          bool good = st == AutoMapGoalOnThisPress || st == AutoMapGoalMore || st == AutoMapGoalAlreadyDone;
          if (badge.empty()) continue;
          o << (first ? "" : ",") << "[" << goals[g]->x << "," << goals[g]->y << "," << JsonText(badge) << "," << good << "]";
          first = false;
        }
        o << "]";
        any = true;
      }
      // what a direct link would leave a door like
      std::ostringstream ghosts;
      bool firstGhost = true;
      for (size_t k = 0; k < from.size(); k++)
      {
        for (size_t c = 0; c < l.cells.size(); c++)
        {
          if (l.cells[c].x != from[k].dstX || l.cells[c].y != from[k].dstY || l.cells[c].cell.kind != AutoMapCellDoor) continue;
          int predicted = AutoMapScene_PredictDoorState(l.cells[c].cell.doorState, from[k].signal);
          if (predicted >= 0) { ghosts << (firstGhost ? "" : ",") << "[" << from[k].dstX << "," << from[k].dstY << "," << predicted << "," << l.cells[c].cell.runsNorthSouth << "]"; firstGhost = false; }
        }
      }
      if (!firstGhost) { o << (any ? "," : "") << "\"ghost\":[" << ghosts.str() << "]"; any = true; }
    }
    if (!any) continue;
    Append(f, "%s\"%d,%d\":{%s}\n", firstHover ? "" : ",", x, y, o.str().c_str());
    firstHover = false;
  }
  Append(f, "}}\n");
}

static int LevelIsVisited(AutoMapRecorder &recorder, int level)
{
  if (level >= recorder.Explored().LevelCount()) return 0;
  const std::vector<uint8_t> &bits = recorder.Explored().BitsOfLevel(level);
  for (size_t i = 0; i < bits.size(); i++) if (bits[i]) return 1;
  return 0;
}

// One level's entry of index.json
static void AppendLevelIndex(std::string &idx, AutoMapRecorder &recorder, int level, const AutoMapLevelSnapshot &s,
                             const std::vector<AutoMapMonsterChoice> &types)
{
  Append(idx, "%s{\"level\":%d,\"w\":%d,\"h\":%d,\"offsetX\":%d,\"offsetY\":%d,\"visited\":%d,\"types\":[", level ? "," : "", level,
         s.width, s.height, s.offsetX, s.offsetY, LevelIsVisited(recorder, level));
  for (size_t i = 0; i < types.size(); i++) Append(idx, "%s%s", i ? "," : "", JsonText(types[i].name).c_str());
  Append(idx, "]}");
}

static void AppendChampions(std::string &idx)
{
  std::vector<AutoMapChampionLevels> champions = AutoMap_ChampionLevels();
  Append(idx, "],\"champions\":[");
  for (size_t i = 0; i < champions.size(); i++)
    Append(idx, "%s[%s,%d,%d,%d,%d]", i ? "," : "", JsonText(champions[i].name).c_str(), champions[i].level[0], champions[i].level[1],
           champions[i].level[2], champions[i].level[3]);
}

static bool WriteText(const std::string &path, const std::string &text)
{
  FILE *f = fopen(path.c_str(), "wb");
  if (f == NULL) return false;
  bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
  return fclose(f) == 0 && ok;
}

static void Export(const char *dir, AutoMapRecorder &recorder, int partyLevel)
{
  ExportPictures sp;
  sp.dir = dir;
  AutoMapSprites spritesForSchemes;
  int levelCount = (int)AutoMapLevelReader_DungeonLevelSizes().size();
  std::string idx;
  Append(idx, "{\"partyLevel\":%d,\"levels\":[", partyLevel);
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapLevelSnapshot s = AutoMapLevelReader_ReadLevel(level);
    spritesForSchemes.UseColourSchemes(s.paletteScheme9, s.paletteScheme10);
    sp.level = level;
    std::vector<AutoMapMonsterChoice> types = AutoMapLevelReader_MonsterTypesOfLevel(level);
    AppendLevelIndex(idx, recorder, level, s, types);
    AutoMapScene_RememberExploredLooks(s, recorder.Explored(), &recorder.Cells());
    for (int vision = 0; vision < 3; vision++)
      for (int spoil = 0; spoil < 3; spoil++)
      {
        AutoMapDrawList list = DrawListOf(recorder, partyLevel, s, vision, spoil);
        std::string json;
        WriteDrawList(json, sp, list, s, types);
        char name[64];
        sprintf(name, "/L%d_v%d_s%d.json", level, vision, spoil);
        WriteText(dir + std::string(name), json);
      }
    printf("export: level %d\n", level);
  }
  AppendChampions(idx);
  Append(idx, "]}\n");
  WriteText(dir + std::string("/index.json"), idx);
}

// ---------------- the web page of the map window's EXPORT ----------------

struct HtmlExport
{
  bool running;
  AutoMapRecorder *recorder;
  int partyLevel;
  AutoMapHtmlChoice choice;
  std::vector<std::pair<int, int> > views;
  int levelCount;
  int nextLevel;
  int levelsDone;
  std::set<int> chosen;
  ExportPictures pictures;
  std::string index;
  std::string drawLists;
  HtmlExport(void) : running(false), recorder(NULL), partyLevel(0), levelCount(0), nextLevel(0), levelsDone(0) {}
};
static HtmlExport *html = NULL;

void AutoMapExport_HtmlBegin(AutoMapRecorder &recorder, int partyLevel, const AutoMapHtmlChoice &choice)
{
  delete html;
  html = new HtmlExport();
  html->running = true;
  html->recorder = &recorder;
  html->partyLevel = partyLevel;
  html->choice = choice;
  html->views = AutoMapHtml_Views(choice.maxVision, choice.maxSpoilers);
  html->levelCount = (int)AutoMapLevelReader_DungeonLevelSizes().size();
  for (size_t i = 0; i < choice.levels.size(); i++)
    if (choice.levels[i] >= 0 && choice.levels[i] < html->levelCount) html->chosen.insert(choice.levels[i]);
  html->pictures.intoPage = true;
  Append(html->index, "{\"partyLevel\":%d,\"levels\":[", partyLevel);
}

int AutoMapExport_HtmlLevelsDone(void)
{
  return html ? html->levelsDone : 0;
}

int AutoMapExport_HtmlLevelsToDo(void)
{
  return html ? (int)html->chosen.size() : 0;
}

int AutoMapExport_HtmlNextLevel(void)
{
  if (html == NULL) return -1;
  for (int level = html->nextLevel; level < html->levelCount; level++)
    if (html->chosen.count(level)) return level;
  return -1;
}

bool AutoMapExport_HtmlStep(void)
{
  if (html == NULL || !html->running) return true;
  // The levels left out keep their entry, so that the page numbers the rest as the game does
  while (html->nextLevel < html->levelCount)
  {
    int level = html->nextLevel++;
    if (!html->chosen.count(level))
    {
      Append(html->index, "%s{\"level\":%d,\"absent\":1,\"w\":0,\"h\":0,\"offsetX\":0,\"offsetY\":0,\"visited\":%d,\"types\":[]}",
             level ? "," : "", level, LevelIsVisited(*html->recorder, level));
      continue;
    }
    AutoMapLevelSnapshot s = AutoMapLevelReader_ReadLevel(level);
    html->pictures.level = level;
    html->pictures.scheme9 = s.paletteScheme9;
    html->pictures.scheme10 = s.paletteScheme10;
    std::vector<AutoMapMonsterChoice> types = AutoMapLevelReader_MonsterTypesOfLevel(level);
    AppendLevelIndex(html->index, *html->recorder, level, s, types);
    for (size_t v = 0; v < html->views.size(); v++)
    {
      int vision = html->views[v].first, scene = html->views[v].second;
      AutoMapDrawList list = DrawListOf(*html->recorder, html->partyLevel, s, vision, scene);
      Append(html->drawLists, "%s\"L%d_v%d_s%d\":", html->drawLists.empty() ? "" : ",", level, vision, scene);
      WriteDrawList(html->drawLists, html->pictures, list, s, types);
    }
    html->levelsDone++;
    return false;
  }
  return true;
}

static int Clamp(int value, int low, int high)
{
  return value < low ? low : value > high ? high : value;
}

static const char *const VISION_NAMES[3] = {"fog", "explored", "all"};
static const char *const SPOILER_NAMES[3] = {"seen", "learned", "all"};

std::string AutoMapExport_HtmlPage(void)
{
  if (html == NULL) return std::string();
  while (!AutoMapExport_HtmlStep()) {}
  const AutoMapHtmlChoice &c = html->choice;
  int startLevel = html->chosen.count(c.level) ? c.level : html->chosen.empty() ? 0 : *html->chosen.begin();
  AppendChampions(html->index);
  // The page starts as the map window shows it, held inside what was chosen
  Append(html->index, "],\"export\":{\"maxVision\":%d,\"minSpoil\":%d,\"vision\":%d,\"spoil\":%d,\"level\":%d,\"bar\":%s}}",
         Clamp(c.maxVision, 0, 2), 2 - Clamp(c.maxSpoilers, 0, 2), Clamp(c.vision, 0, Clamp(c.maxVision, 0, 2)),
         2 - Clamp(c.spoilers, 0, Clamp(c.maxSpoilers, 0, 2)), startLevel, JsonText(c.barStyle).c_str());
  std::string save = "{\"index\":" + html->index + ",\"views\":{" + html->drawLists + "}}";
  html->index.clear();
  html->drawLists.clear();
  std::string data = "window.MAPDATA={\"saves\":{" + JsonText(c.saveName) + ":\"" + AutoMapHtml_Base64(AutoMapHtml_Gzip(save)) +
                     "\"},\"sprites\":{";
  save.clear();
  bool first = true;
  for (std::map<std::string, std::string>::const_iterator p = html->pictures.pngOfName.begin(); p != html->pictures.pngOfName.end(); ++p)
  {
    data += (first ? "" : ",") + JsonText(p->first) + ":\"data:image/png;base64," + AutoMapHtml_Base64(p->second) + "\"";
    first = false;
  }
  data += "}};";
  char when[64] = "";
  time_t now = time(NULL);
  strftime(when, sizeof(when), "%Y-%m-%d %H:%M", localtime(&now));
  char subtitle[256];
  sprintf(subtitle, "Exported from CSB on %s. %d of %d levels, the party on level %d. Vision up to %s, spoilers up to %s.", when,
          (int)html->chosen.size(), html->levelCount, html->partyLevel, VISION_NAMES[Clamp(c.maxVision, 0, 2)],
          SPOILER_NAMES[Clamp(c.maxSpoilers, 0, 2)]);
  std::string page = AutoMapHtml_Page(c.title, subtitle, data);
  AutoMapExport_HtmlCancel();
  return page;
}

void AutoMapExport_HtmlCancel(void)
{
  delete html;
  html = NULL;
}

// CSB_MAP_RENDER="<out.bmp>,<level>,<vision 0..2>,<spoilers as in csb.cfg: 0 seen, 1 learned, 2 all>,
// <cell size>,<x>,<y>[,shift|export|export-pick|export-busy|export-done]": the map window as it would look with the cell (x, y) of the level
// (draw-list coordinates, as the mockup's hh) hovered, 1200 x 900 pixels
static void Render(const char *spec, AutoMapRecorder &recorder, int partyLevel)
{
  char out[512] = "";
  int level = 0, vision = 0, spoilers = 2, cellSize = 40, x = 0, y = 0;
  char shift[16] = "";
  int fields = sscanf(spec, "%511[^,],%d,%d,%d,%d,%d,%d,%15s", out, &level, &vision, &spoilers, &cellSize, &x, &y, shift);
  if (fields < 7)
  {
    printf("CSB_MAP_RENDER: expected <out.bmp>,<level>,<vision>,<spoilers>,<cell>,<x>,<y>[,shift]\n");
    return;
  }
  AutoMapLevelSnapshot s = AutoMapLevelReader_ReadLevel(level);
  AutoMapSprites spritesForSchemes;
  spritesForSchemes.UseColourSchemes(s.paletteScheme9, s.paletteScheme10);
  AutoMapScene_RememberExploredLooks(s, recorder.Explored(), &recorder.Cells());
  AutoMapDrawList list = DrawListOf(recorder, partyLevel, s, vision, 2 - spoilers);
  // "export", "export-pick", "export-busy", "export-done": the export dialog open over the map
  std::string last = fields == 8 ? shift : "";
  if (last.compare(0, 6, "export") == 0) AutoMapExportDialog_OpenForPicture(last.size() > 7 ? last.substr(7) : "form");
  bool rendered = AutoMapWindow_RenderToFile(list, 1200, 900, cellSize, x, y, last == "shift", out);
  if (AutoMapExportDialog_IsOpen()) AutoMapExportDialog_Close();
  printf("CSB_MAP_RENDER: %s %s\n", out, rendered ? "written" : "failed");
}

// CSB_MAP_HTML="<out.html>,<max vision 0..2>,<max spoilers as in csb.cfg 0..2>[,<levels>]": the
// page the map window's EXPORT writes; levels is "all" (the default) or level numbers joined by +
static void Html(const char *spec, AutoMapRecorder &recorder, int partyLevel)
{
  char out[512] = "", levels[512] = "all";
  int maxVision = 2, maxSpoilers = 2;
  if (sscanf(spec, "%511[^,],%d,%d,%511s", out, &maxVision, &maxSpoilers, levels) < 3)
  {
    printf("CSB_MAP_HTML: expected <out.html>,<max vision>,<max spoilers>[,<levels>]\n");
    return;
  }
  AutoMapHtmlChoice choice;
  int levelCount = (int)AutoMapLevelReader_DungeonLevelSizes().size();
  if (strcmp(levels, "all") == 0) for (int level = 0; level < levelCount; level++) choice.levels.push_back(level);
  else for (const char *p = levels; *p; p++) if (p == levels || p[-1] == '+') choice.levels.push_back(atoi(p));
  choice.maxVision = maxVision;
  choice.maxSpoilers = maxSpoilers;
  choice.vision = 0;
  choice.spoilers = 1;
  choice.level = partyLevel;
  choice.saveName = "test";
  choice.title = "CSB map: test";
  choice.barStyle = AutoMapBar_StyleName();
  AutoMapExport_HtmlBegin(recorder, partyLevel, choice);
  std::string page = AutoMapExport_HtmlPage();
  printf("CSB_MAP_HTML: %s %s, %d bytes\n", out, WriteText(out, page) ? "written" : "failed", (int)page.size());
}

void AutoMapExport_Run(AutoMapRecorder &recorder, int partyLevel)
{
  if (getenv("CSB_MAP_HTML") != NULL) Html(getenv("CSB_MAP_HTML"), recorder, partyLevel);
  if (getenv("CSB_MAP_EXPORT") != NULL) Export(getenv("CSB_MAP_EXPORT"), recorder, partyLevel);
  if (getenv("CSB_MAP_RENDER") != NULL) Render(getenv("CSB_MAP_RENDER"), recorder, partyLevel);
}
