// AutoMapTooltip.cpp : the machinery part of the tile tooltip, one group per thing on the tile
#include "AutoMapTooltip.h"

#include <string>

#include "AutoMapSceneInternal.h"

static const char *const SHIFT_HINT = "SHIFT: ROUTE";

// Who a teleporter takes: the words get the map badge's glyphs in front. A glyph takes the
// place of two characters, \x01 party, \x02 claw, \x03 box; the font draws nothing for them.
static std::string WithBadgeGlyphs(const std::string &line)
{
  static const char *const PREFIXES[] = {"TAKES: ", "WOULD TAKE: "};
  for (int p = 0; p < 2; p++)
  {
    std::string prefix = PREFIXES[p];
    if (line.compare(0, prefix.size(), prefix) != 0) continue;
    std::string what = line.substr(prefix.size());
    if (what == "PARTY+OBJECTS") return prefix + "\x01\x01 PARTY \x03\x03 OBJECTS";
    if (what == "OBJECTS") return prefix + "\x03\x03 OBJECTS";
    if (what == "MONSTERS") return prefix + "\x02\x02 MONSTERS";
  }
  return line;
}

static AutoMapTipLine Indented(const AutoMapTipLine &line, size_t spaces)
{
  AutoMapTipLine indented = line;
  indented.text.insert(0, spaces, ' ');
  return indented;
}

// A header and its lines, two spaces in
static void AddGroup(std::vector<AutoMapTipLine> *out, const std::string &header,
                     AutoMapTipStyle style, const std::vector<AutoMapTipLine> &children)
{
  out->push_back(AutoMapScene_TipLine(header, style));
  for (size_t i = 0; i < children.size(); i++) out->push_back(Indented(children[i], 2));
}

static bool SameLines(const std::vector<AutoMapTipLine> &a, const std::vector<AutoMapTipLine> &b)
{
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
  {
    if (a[i].text != b[i].text || a[i].style != b[i].style || !(a[i].items == b[i].items))
    {
      return false;
    }
  }
  return true;
}

// The press block of one source (AutoMapScene_PressBlock), as the lines under its pad's or
// switch's header. The SHIFT hint is tile-wide: a block with steps between the press and a
// result asks for it, and the window puts it last.
static void AddPressBlock(const AutoMapDrawList &drawList, const AutoMapPressSource &source,
                          bool shift, std::vector<AutoMapTipLine> *children, bool *hint)
{
  std::vector<AutoMapTipLine> block = AutoMapScene_PressBlock(drawList, source, shift);
  if (block.empty()) return;
  children->insert(children->end(), block.begin(), block.end());
  if (AutoMapScene_PressHasSteps(source)) *hint = true;
}

static std::string SwitchHeader(const AutoMapPressSource &source)
{
  if (source.srcSide < AutoMapNorth || source.srcSide > AutoMapWest) return "SWITCH";
  return std::string("SWITCH (") + FacingInitial(source.srcSide) + ")";
}

static void AddTeleporterGroup(const AutoMapDrawList &drawList, const AutoMapShownCell &shown,
                               std::vector<AutoMapTipLine> *out)
{
  const char *header = NULL;
  bool dormant = false;
  switch (shown.cell.kind)
  {
  case AutoMapCellTeleporter:
  case AutoMapCellInvisibleTeleporter: header = "TELEPORTER"; break;
  case AutoMapCellDormantTeleporter:
    header = "DORMANT TELEPORTER";
    dormant = true;
    break;
  case AutoMapCellSpinner: header = "SPINNER"; break;
  case AutoMapCellDormantSpinner:
    header = "DORMANT SPINNER";
    dormant = true;
    break;
  default: return;
  }
  std::vector<AutoMapTipLine> children = AutoMapScene_TeleporterLines(drawList, shown.x, shown.y);
  for (size_t i = 0; i < children.size(); i++) children[i].text = WithBadgeGlyphs(children[i].text);
  AddGroup(out, header, dormant ? AutoMapTipDormantHeader : AutoMapTipHeader, children);
}

static void AddPitGroup(const AutoMapDrawList &drawList, const AutoMapShownCell &shown,
                        std::vector<AutoMapTipLine> *out)
{
  std::vector<AutoMapTipLine> children = AutoMapScene_PitLines(drawList, shown.x, shown.y);
  if (children.empty()) return;
  bool hidden = shown.cell.kind == AutoMapCellPit && shown.cell.pitIsHidden;
  AddGroup(out, hidden ? "HIDDEN PIT" : "PIT", AutoMapTipHeader, children);
}

std::vector<AutoMapTipLine> AutoMapTooltip_Machinery(const AutoMapDrawList &drawList, int x, int y,
                                                     bool shift, AutoMapItemNameFn nameForIndex,
                                                     std::vector<AutoMapTipLine> *hint)
{
  std::vector<AutoMapTipLine> out;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return out;
  bool wantsHint = false;
  AddTeleporterGroup(drawList, *shown, &out);
  AddPitGroup(drawList, *shown, &out);
  // Doors, false walls, launchers: the scene groups those itself
  std::vector<AutoMapTipLine> relay = AutoMapScene_RelayLines(drawList, x, y);
  out.insert(out.end(), relay.begin(), relay.end());
  // Every pad is its own press (AutoMapScene_PressEffects groups floor links per pad record)
  std::vector<AutoMapPressSource> sources = AutoMapScene_PressEffects(drawList, x, y);
  std::vector<bool> placed(sources.size(), false);
  std::vector<std::vector<AutoMapTipLine> > generators;
  std::vector<std::string> generatorHeaders;
  for (size_t a = 0; a < drawList.actuators.size(); a++)
  {
    const AutoMapFloorActuator &actuator = drawList.actuators[a];
    if (actuator.x != x || actuator.y != y || actuator.kind == AutoMapActuatorCog) continue;
    // Pads with one trigger are one press (AutoMapScene_PressEffects): one group, said once
    bool pad = actuator.kind == AutoMapActuatorPad;
    bool said = false;
    for (size_t e = 0; e < a && pad && !said; e++)
    {
      const AutoMapFloorActuator &earlier = drawList.actuators[e];
      said = earlier.x == x && earlier.y == y && AutoMapScene_SamePadTrigger(earlier, actuator);
    }
    if (said) continue;
    std::vector<AutoMapTipLine> children =
        AutoMapScene_ActuatorLines(drawList, actuator, nameForIndex);
    bool pressed = false;
    for (size_t s = 0; s < sources.size(); s++)
    {
      if (placed[s] || sources[s].srcIsWall) continue;
      // A source is its first pad's record; a pad without one claims the source without one
      bool mine = sources[s].source == actuator.record
          && (pad || actuator.record != AUTOMAP_NO_RECORD);
      const AutoMapFloorActuator *first =
          AutoMapScene_PadOfRecord(drawList, x, y, sources[s].source);
      if (!mine && pad && first != NULL) mine = AutoMapScene_SamePadTrigger(*first, actuator);
      if (!mine) continue;
      AddPressBlock(drawList, sources[s], shift, &children, &wantsHint);
      placed[s] = true;
      pressed = true;
    }
    switch (actuator.kind)
    {
    case AutoMapActuatorPad:
      // Below All a pad shows no trigger, and so no group
      if (!children.empty()) AddGroup(&out, "PAD", AutoMapTipHeader, children);
      break;
    case AutoMapActuatorDormantPad:
      AddGroup(&out, "DORMANT PAD", AutoMapTipDormantHeader, children);
      break;
    default:
    {
      // Generators come after the switches; the same generator twice is said once
      std::string header = actuator.kind == AutoMapActuatorGenerator ? "GENERATOR" : "SPENT GENERATOR";
      bool known = false;
      for (size_t g = 0; g < generators.size() && !known && !pressed; g++)
      {
        known = generatorHeaders[g] == header && SameLines(generators[g], children);
      }
      if (!known)
      {
        generatorHeaders.push_back(header);
        generators.push_back(children);
      }
      break;
    }
    }
  }
  // A floor source no actuator of the cell claims still gets its own pad group
  for (size_t s = 0; s < sources.size(); s++)
  {
    if (placed[s] || sources[s].srcIsWall) continue;
    std::vector<AutoMapTipLine> children;
    AddPressBlock(drawList, sources[s], shift, &children, &wantsHint);
    if (!children.empty()) AddGroup(&out, "PAD", AutoMapTipHeader, children);
  }
  // One group per switch face: one click fires every actuator of the face
  for (size_t s = 0; s < sources.size(); s++)
  {
    if (!sources[s].srcIsWall) continue;
    std::vector<AutoMapTipLine> children;
    AddPressBlock(drawList, sources[s], shift, &children, &wantsHint);
    if (!children.empty()) AddGroup(&out, SwitchHeader(sources[s]), AutoMapTipHeader, children);
  }
  for (size_t g = 0; g < generators.size(); g++)
  {
    bool spent = generatorHeaders[g] == "SPENT GENERATOR";
    AddGroup(&out, generatorHeaders[g], spent ? AutoMapTipDormantHeader : AutoMapTipHeader,
             generators[g]);
  }
  // The relays of the cell, as today's hover block until they get headers of their own
  std::vector<std::string> relays = AutoMapScene_RelayHoverLines(drawList, x, y, shift);
  for (size_t i = 0; i < relays.size(); i++)
  {
    out.push_back(AutoMapScene_TipLine(relays[i], AutoMapTipPlain));
  }
  if (hint != NULL && wantsHint && !shift)
  {
    hint->push_back(AutoMapScene_TipLine(SHIFT_HINT, AutoMapTipHint));
  }
  return out;
}
