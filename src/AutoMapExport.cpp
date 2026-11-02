// AutoMapExport.cpp : writes what the map shows, per level, vision and spoiler level, for the
// reference mockup in docs/automap-mockup (pack-data.py turns the files into its data block).
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "AutoMapLevelReader.h"
#include "AutoMapRecorder.h"
#include "AutoMapScene.h"
#include "AutoMapSprites.h"
#include "AutoMapWindowTooltip.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include "AutoMapExport.h"

bool AutoMapExport_IsRequested(void)
{
  return getenv("CSB_MAP_EXPORT") != NULL || getenv("CSB_MAP_RENDER") != NULL;
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
                    &recorder.Items(), seenNow, &recorder.Cells(), &recorder.Notes(), &recorder.Damage(), vision == 1};
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

struct ExportPictures
{
  std::string dir;
  int level;
  std::set<std::string> done;
  std::string Save(const AutoMapSpriteRequest &r)
  {
    char name[128];
    if (r.hasTile) sprintf(name, "L%d_p%d_t%d_x%d_y%d%s.bmp", level, r.picture, r.transparentColour, r.tileX, r.tileY, r.portraitPalette ? "_pp" : "");
    else sprintf(name, "L%d_p%d_t%d.bmp", level, r.picture, r.transparentColour);
    std::string n = name;
    if (done.count(n)) return n;
    done.insert(n);
    if (!AutoMapSprites_SaveBmp(r, (dir + "/" + n).c_str())) return "";
    return n;
  }
  std::string Plain(int picture, int tc) { if (picture < 0) return ""; AutoMapSpriteRequest r = {picture, tc, false, 0, 0, 0, 0, false}; return Save(r); }
  std::string Icon(const AutoMapItem &item)
  {
    if (!item.hasIcon) return "";
    AutoMapSpriteRequest r = {item.iconGraphic, -1, true, (item.iconTile & 15) * 16, (item.iconTile >> 4) * 16, 16, 16, true};
    return Save(r);
  }
  std::string Portrait(int p)
  {
    if (p < 0 || p >= 24) return "";
    AutoMapSpriteRequest r = {26, 1, true, (p & 7) * 32, (p >> 3) * 29, 32, 29, false};
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

static std::string StrArr(const std::vector<std::string> &v)
{
  std::string s = "[";
  for (size_t i = 0; i < v.size(); i++) s += (i ? "," : "") + JsonText(v[i]);
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

static void WriteGoals(const std::vector<AutoMapEffect> &effects, std::ostringstream &o, bool &first)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind == AutoMapEffectGoal)
    {
      std::string badge = AutoMapScene_GoalBadge(effects[i].distance);
      AutoMapGoalStatus st = effects[i].distance.status;
      bool good = st == AutoMapGoalOnThisPress || st == AutoMapGoalMore || st == AutoMapGoalAlreadyDone;
      if (!badge.empty())
      {
        o << (first ? "" : ",") << "[" << effects[i].x << "," << effects[i].y << "," << JsonText(badge) << "," << good << "]";
        first = false;
      }
    }
    WriteGoals(effects[i].next, o, first);
  }
}


static void WriteDrawList(FILE *f, ExportPictures &sp, const AutoMapDrawList &l, const AutoMapLevelSnapshot &snap,
                          const std::vector<AutoMapMonsterChoice> &types)
{
  fprintf(f, "{\"spoilers\":%d,\"party\":[%d,%d,%d,%d],\"cells\":[\n", (int)l.spoilers, l.party.x, l.party.y, (int)l.party.facing, l.partyIsOnShownLevel);
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
    fprintf(f, "%s%s\n", i ? "," : "", o.str().c_str());
  }
  fprintf(f, "],\"monsters\":[");
  for (size_t i = 0; i < l.monsters.size(); i++)
  {
    const AutoMapShownMonster &m = l.monsters[i];
    std::ostringstream o;
    o << "{\"x\":" << m.x << ",\"y\":" << m.y << ",\"name\":" << JsonText(m.name) << ",\"m\":[";
    for (int k = 0; k < m.memberCount; k++)
      o << (k ? "," : "") << "[" << JsonText(sp.Plain(m.memberPictures[k].picture, m.transparentColour)) << "," << m.memberPictures[k].isMirrored << "," << (int)m.memberFacings[k] << "]";
    o << "]}";
    fprintf(f, "%s%s", i ? "," : "", o.str().c_str());
  }
  fprintf(f, "],\"missiles\":[");
  for (size_t i = 0; i < l.missiles.size(); i++)
    fprintf(f, "%s[%d,%d,%s,%.2f]", i ? "," : "", l.missiles[i].x, l.missiles[i].y, JsonText(sp.Plain(l.missiles[i].item.worldPicture, 10)).c_str(),
            l.missiles[i].sizeShowsRange ? AutoMapScene_MissileScale(l.missiles[i].rangeRemaining) : 1.0f);
  fprintf(f, "],\"clouds\":[");
  for (size_t i = 0; i < l.clouds.size(); i++)
    fprintf(f, "%s[%d,%d,%s]", i ? "," : "", l.clouds[i].x, l.clouds[i].y, JsonText(sp.Plain(l.clouds[i].picture, 10)).c_str());
  fprintf(f, "],\"acts\":[");
  for (size_t i = 0; i < l.actuators.size(); i++)
  {
    const AutoMapFloorActuator &a = l.actuators[i];
    std::ostringstream o;
    o << "[" << a.x << "," << a.y << "," << (int)a.kind << "," << (int)a.op << "," << a.requiredFacing << "," << ItemArr(sp, a.object) << ","
      << JsonText(a.hasGeneratedMonster ? sp.Plain(a.generatedKind.frontPicture, a.generatedKind.transparentColour) : "") << "," << JsonText(a.generatedMonsterName) << "]";
    fprintf(f, "%s%s", i ? "," : "", o.str().c_str());
  }
  fprintf(f, "],\"links\":%s,\"plinks\":%s,\"glimpses\":[", LinkArr(l.links).c_str(), LinkArr(l.playerLinks).c_str());
  for (size_t i = 0; i < l.glimpsedItems.size(); i++) fprintf(f, "%s[%d,%d]", i ? "," : "", l.glimpsedItems[i].x, l.glimpsedItems[i].y);
  fprintf(f, "],\"gfaces\":[");
  for (size_t i = 0; i < l.glimpsedFaces.size(); i++) fprintf(f, "%s[%d,%d,%d]", i ? "," : "", l.glimpsedFaces[i].x, l.glimpsedFaces[i].y, l.glimpsedFaces[i].side);
  // what hovering a cell shows: the tooltip as the window builds it, and the lines it draws
  fprintf(f, "],\n\"hover\":{");
  bool firstHover = true;
  for (size_t i = 0; i < l.cells.size(); i++)
  {
    int x = l.cells[i].x, y = l.cells[i].y;
    std::vector<std::string> lines, linesFull;
    std::vector<AutoMapTooltipEntry> tip, tipFull;
    AutoMapWindow_TooltipFor(l, x, y, false, &lines, &tip);
    AutoMapWindow_TooltipFor(l, x, y, true, &linesFull, &tipFull);
    std::vector<AutoMapLink> from = AutoMapScene_LinksFrom(l, x, y), to = AutoMapScene_LinksTo(l, x, y);
    std::ostringstream o;
    bool any = false;
    if (!lines.empty())
    {
      o << "\"tip\":" << StrArr(lines);
      if (linesFull != lines) o << ",\"tipFull\":" << StrArr(linesFull);
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
      bool relayCell = !AutoMapScene_RelayHoverLines(l, x, y, false).empty();
      if (!sources.empty() || relayCell)
      {
        std::vector<AutoMapLink> chain = AutoMapScene_PressChainLinks(sources);
        o << (any ? "," : "") << "\"chain\":" << LinkArr(chain) << ",\"relay\":" << relayCell << ",\"goals\":[";
        bool first = true;
        for (size_t s = 0; s < sources.size(); s++) WriteGoals(sources[s].effects, o, first);
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
    fprintf(f, "%s\"%d,%d\":{%s}\n", firstHover ? "" : ",", x, y, o.str().c_str());
    firstHover = false;
  }
  fprintf(f, "}}\n");
}

static int LevelIsVisited(AutoMapRecorder &recorder, int level)
{
  if (level >= recorder.Explored().LevelCount()) return 0;
  const std::vector<uint8_t> &bits = recorder.Explored().BitsOfLevel(level);
  for (size_t i = 0; i < bits.size(); i++) if (bits[i]) return 1;
  return 0;
}

static void Export(const char *dir, AutoMapRecorder &recorder, int partyLevel)
{
  ExportPictures sp;
  sp.dir = dir;
  AutoMapSprites spritesForSchemes;
  int levelCount = (int)AutoMapLevelReader_DungeonLevelSizes().size();
  char path[512];
  sprintf(path, "%s/index.json", dir);
  FILE *idx = fopen(path, "w");
  fprintf(idx, "{\"partyLevel\":%d,\"levels\":[", partyLevel);
  std::vector<AutoMapChampionLevels> champions = AutoMap_ChampionLevels();
  for (int level = 0; level < levelCount; level++)
  {
    AutoMapLevelSnapshot s = AutoMapLevelReader_ReadLevel(level);
    spritesForSchemes.UseColourSchemes(s.paletteScheme9, s.paletteScheme10);
    sp.level = level;
    std::vector<AutoMapMonsterChoice> types = AutoMapLevelReader_MonsterTypesOfLevel(level);
    fprintf(idx, "%s{\"level\":%d,\"w\":%d,\"h\":%d,\"offsetX\":%d,\"offsetY\":%d,\"visited\":%d,\"types\":[", level ? "," : "", level, s.width, s.height,
            s.offsetX, s.offsetY, LevelIsVisited(recorder, level));
    for (size_t i = 0; i < types.size(); i++) fprintf(idx, "%s%s", i ? "," : "", JsonText(types[i].name).c_str());
    fprintf(idx, "]}");
    AutoMapScene_RememberExploredLooks(s, recorder.Explored(), &recorder.Cells());
    for (int vision = 0; vision < 3; vision++)
      for (int spoil = 0; spoil < 3; spoil++)
      {
        AutoMapDrawList list = DrawListOf(recorder, partyLevel, s, vision, spoil);
        sprintf(path, "%s/L%d_v%d_s%d.json", dir, level, vision, spoil);
        FILE *f = fopen(path, "w");
        WriteDrawList(f, sp, list, s, types);
        fclose(f);
      }
    printf("export: level %d\n", level);
  }
  fprintf(idx, "],\"champions\":[");
  for (size_t i = 0; i < champions.size(); i++) fprintf(idx, "%s[%s,%d,%d,%d,%d]", i ? "," : "", JsonText(champions[i].name).c_str(), champions[i].level[0], champions[i].level[1], champions[i].level[2], champions[i].level[3]);
  fprintf(idx, "]}\n");
  fclose(idx);
}

// CSB_MAP_RENDER="<out.bmp>,<level>,<vision 0..2>,<spoilers as in csb.cfg: 0 seen, 1 learned, 2 all>,
// <cell size>,<x>,<y>[,shift]": the map window as it would look with the cell (x, y) of the level
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
  bool rendered = AutoMapWindow_RenderToFile(list, 1200, 900, cellSize, x, y, fields == 8, out);
  printf("CSB_MAP_RENDER: %s %s\n", out, rendered ? "written" : "failed");
}

void AutoMapExport_Run(AutoMapRecorder &recorder, int partyLevel)
{
  if (getenv("CSB_MAP_EXPORT") != NULL) Export(getenv("CSB_MAP_EXPORT"), recorder, partyLevel);
  if (getenv("CSB_MAP_RENDER") != NULL) Render(getenv("CSB_MAP_RENDER"), recorder, partyLevel);
}
