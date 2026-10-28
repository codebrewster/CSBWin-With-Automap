// AutoMapPress.cpp : what a press does down the relay chain, and the tooltip lines that
// tell it (AutoMapScene_PressEffects, PressLines, RelayHoverLines and their helpers).
#include "AutoMapScene.h"
#include "AutoMapSceneInternal.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{

// Where a relay sits: its cell and its index in wallRelays, or in drawList.actuators
// for a floor cog.
struct PressRelayKey
{
  int x;
  int y;
  int relay;
  bool floorRelay;
};

bool operator<(const PressRelayKey &left, const PressRelayKey &right)
{
  if (left.x != right.x) return left.x < right.x;
  if (left.y != right.y) return left.y < right.y;
  if (left.relay != right.relay) return left.relay < right.relay;
  return left.floorRelay < right.floorRelay;
}

bool operator==(const PressRelayKey &left, const PressRelayKey &right)
{
  return !(left < right) && !(right < left);
}

// What the walk has changed so far: relay values (everything else reads its live
// value) and once-only relays that fired, which the engine turns into inert type 0
// (QueueSwitchAction).
struct PressState
{
  std::map<PressRelayKey, int> values;
  std::set<PressRelayKey> spent;
};

// Effect nodes one AutoMapScene_PressEffects call may build: a dense net of gates
// feeding each other has a path for every ordering of them.
const int PRESS_WALK_BUDGET = 1000;

struct PressWalk
{
  const AutoMapDrawList *drawList;
  // The relays between the source and the current message: meeting one again is a
  // loop. A path rather than a set, so two relays both feeding a third are no loop.
  std::vector<PressRelayKey> path;
  int nodes;
  // Set once the budget ran out
  bool truncated;
};

// One receiver of a cell after the message was applied, waiting to be followed
struct PressFollow
{
  size_t effect;
  PressRelayKey key;
  const AutoMapRelay *relay;
  bool closed;
  int sendTicks;
};

bool EarlierSend(const PressFollow &left, const PressFollow &right)
{
  return left.sendTicks < right.sendTicks;
}

// Room for one more effect node, or the walk ends here
bool TakeNode(PressWalk *walk)
{
  if (walk->nodes >= PRESS_WALK_BUDGET)
  {
    walk->truncated = true;
    return false;
  }
  walk->nodes++;
  return true;
}

void DeliverPressMessage(PressWalk *walk, const AutoMapLink &link, AutoMapSignal signal,
                         int delayBefore, bool later, PressState *state,
                         std::vector<AutoMapEffect> *effects);

int PressRelayValue(const PressState &state, const PressRelayKey &key, const AutoMapRelay &relay)
{
  std::map<PressRelayKey, int>::const_iterator found = state.values.find(key);
  return found == state.values.end() ? relay.value : found->second;
}

// What an actuator sends down its link when it fires: its action, which the link carries,
// except that a CONSTPRESSURE actuator reports its state, SET when closed and CLEAR when
// open (QueueSwitchAction gets swclosed ? 0 : 1 from counters, D5W ^ 1 from gates, and the
// same from a pressed switch or pad).
AutoMapSignal PressSentSignal(const AutoMapLink &link, bool closed)
{
  if (link.signal != AutoMapSignalPressure) return link.signal;
  return closed ? AutoMapSignalSet : AutoMapSignalClear;
}

// What a source sends when it is pressed (or, for a relay, when it fires): pressing closes a
// normally-open actuator and opens a normally-closed one (switchClosed = pressed ^ NC in
// PressurePadOrAlcove, the same in TouchWall), so a normally-closed CONSTPRESSURE alcove
// reports CLEAR and counts its counter down
AutoMapSignal PressSourceSignal(const AutoMapLink &link)
{
  return PressSentSignal(link, !link.sourceNormallyClosed);
}

// A message the engine treats as the cell's own effect, after the relays: doors and
// false walls open and close, pits open, teleporters and spinners switch, a generator
// on an open floor generates, and a stone cell fires the launcher on the side the
// message names (ProcessTT_STONEROOM compares the record position with targetPos).
// A pit, open or closed: a closed switchable pit draws as floor but opens like any pit
bool PressCellIsPit(const AutoMapCellRecord &cell)
{
  return cell.kind == AutoMapCellPit || (cell.kind == AutoMapCellFloor && cell.pitIsClosedSwitchable);
}

bool PressReachesGoal(const AutoMapDrawList &drawList, const AutoMapShownCell &cell,
                      const AutoMapLink &link)
{
  if (PressCellIsPit(cell.cell)) return true;
  switch (cell.cell.kind)
  {
  case AutoMapCellDoor:
  case AutoMapCellFalseWall:
  case AutoMapCellOpenFalseWall:
  case AutoMapCellPit:
  case AutoMapCellTeleporter:
  case AutoMapCellInvisibleTeleporter:
  case AutoMapCellDormantTeleporter:
  case AutoMapCellSpinner:
  case AutoMapCellDormantSpinner:
    return true;
  case AutoMapCellWall:
    for (size_t i = 0; i < cell.cell.wallLaunchers.size(); i++)
    {
      if (cell.cell.wallLaunchers[i].side == link.targetPos) return true;
    }
    return false;
  case AutoMapCellFloor:
    for (size_t i = 0; i < drawList.actuators.size(); i++)
    {
      const AutoMapFloorActuator &actuator = drawList.actuators[i];
      if (actuator.x == cell.x && actuator.y == cell.y
          && actuator.kind == AutoMapActuatorGenerator)
      {
        return true;
      }
    }
    return false;
  default:
    return false;
  }
}

// Follows what a relay sends: its links, or its local action, into effect->next. The
// relay is on the walk path while this runs.
void FollowPressRelay(PressWalk *walk, const PressRelayKey &key, const AutoMapRelay &relay,
                      bool closed, bool later, PressState *state, AutoMapEffect *effect)
{
  if (relay.localActionType >= 0)
  {
    // PerformLocalAction runs at once on the relay's own cell and sends no message:
    // QueueSwitchAction works out the delayed time but the local branch ignores it
    if (!TakeNode(walk)) return;
    AutoMapEffect local;
    local.kind = AutoMapEffectLocalAction;
    local.x = key.x;
    local.y = key.y;
    local.relay = key.relay;
    local.floorRelay = key.floorRelay;
    local.later = later;
    local.delayTicks = effect->delayTicks;
    effect->next.push_back(local);
    return;
  }
  // Records name their links; a hand-built relay without a record sends nothing
  if (relay.record == AUTOMAP_NO_RECORD) return;
  const std::vector<AutoMapLink> &links = walk->drawList->links;
  for (size_t i = 0; i < links.size() && !walk->truncated; i++)
  {
    const AutoMapLink &link = links[i];
    if (link.source != relay.record || link.srcX != key.x || link.srcY != key.y) continue;
    DeliverPressMessage(walk, link, PressSentSignal(link, closed), effect->delayTicks, later,
                        state, &effect->next);
  }
}

// A gate's low nibble after a message on bit (the type-5 branch of ProcessTT_STONEROOM):
// *closed is the match, inverted when normally closed, and *fires says whether the
// message passes the andOrEdge filter and the gate queues its action. Shared by the walk
// and by the goal distance, which asks whether further messages could make it fire.
int GateReceives(const AutoMapRelay &relay, int before, int bit, AutoMapSignal signal,
                 bool *closed, bool *fires)
{
  int bitmask = bit >= 0 && bit <= 3 ? 1 << bit : 0;
  int pattern = (relay.value >> 4) & 15;
  int after = before;
  if (signal == AutoMapSignalToggle) after = before ^ bitmask;
  else if (signal == AutoMapSignalSet) after = before | bitmask;
  else after = before & ~bitmask;
  bool oldMatch = before == pattern;
  bool newMatch = after == pattern;
  *closed = newMatch != relay.normallyClosed;
  bool passes = true;
  switch (relay.edgeMode)
  {
  case 1: passes = before != after; break;
  case 2: passes = oldMatch != newMatch; break;
  case 3: passes = before != after && oldMatch == newMatch; break;
  case 4: passes = signal == AutoMapSignalSet; break;
  case 5: passes = signal == AutoMapSignalClear; break;
  case 6: passes = before != after && (before & bitmask) == 0; break;
  case 7: passes = before != after && (after & bitmask) == 0; break;
  default: break;
  }
  // A CONSTPRESSURE gate reports its state on every message that passes the filter
  *fires = passes && (relay.signal == AutoMapSignalPressure || *closed);
  return after;
}

// One relay receiving the message, after OperateCounterActuator and the type-5 branch
// of ProcessTT_STONEROOM. Returns false when it adds no effect (a spent once-only
// relay, or the budget ran out); *fires, *closed and *followLater say what follows:
// *followLater for a relay that can still fire later, and for a dead counter.
bool ReceivePressMessage(PressWalk *walk, const PressRelayKey &key, const AutoMapRelay &relay,
                         AutoMapSignal signal, const AutoMapEffect &arrival, PressState *state,
                         AutoMapEffect *effect, bool *closed, bool *followLater)
{
  *closed = false;
  *followLater = false;
  // Spent once-only relays are type 0 now: nothing in the cell reacts
  if (state->spent.count(key) != 0) return false;
  if (!TakeNode(walk)) return false;
  *effect = arrival;
  effect->relay = key.relay;
  effect->floorRelay = key.floorRelay;
  if (relay.kind == AutoMapRelayDSA)
  {
    // Not simulated: whether it passes the message on, and what it sends, stays open
    effect->kind = AutoMapEffectDSA;
    return true;
  }
  int before = PressRelayValue(*state, key, relay);
  // A dead counter cannot react, so meeting it again is no loop: it just reads 0
  bool dead = relay.kind == AutoMapRelayCounter && before == 0;
  if (!dead && std::find(walk->path.begin(), walk->path.end(), key) != walk->path.end())
  {
    effect->kind = AutoMapEffectLoop;
    effect->relay = -1;
    effect->floorRelay = false;
    return true;
  }
  int after = before;
  bool fires = false;
  if (relay.kind == AutoMapRelayCounter)
  {
    effect->kind = AutoMapEffectCounter;
    // A counter at 0 never changes again; SET counts up to 511, the rest count down
    if (!dead)
    {
      if (signal == AutoMapSignalSet) after = std::min(before + 1, 511);
      else after = before - 1;
      *closed = (after == 0) != relay.normallyClosed;
      // CONSTPRESSURE reports every message, the others fire on reaching 0
      fires = relay.signal == AutoMapSignalPressure || after == 0;
      *followLater = after > 0;
    }
    else
    {
      // It never fires again, but what it would send is still followed, so the goals
      // behind it can read BLOCKED. Only once per path: a dead counter is no loop.
      *followLater = std::find(walk->path.begin(), walk->path.end(), key) == walk->path.end();
    }
  }
  else
  {
    effect->kind = AutoMapEffectGate;
    // The message position is the bit
    before &= 15;
    after = GateReceives(relay, before, arrival.via.targetPos, signal, closed, &fires);
    *followLater = true;
  }
  // The state keeps the record's form of the value: a gate's pattern stays in the high nibble
  state->values[key] = relay.kind == AutoMapRelayGate ? (relay.value & ~15) | after : after;
  if (fires && relay.onceOnly) state->spent.insert(key);
  effect->before = before;
  effect->after = after;
  effect->fires = fires;
  return true;
}

// One message landing on the target of a link: every receiver of the cell in record
// order, then the cell's goal. Only then are the receivers that fired followed, the
// earliest send first, the way the engine queues their messages after the walk of the
// cell; then those that can fire later, and dead counters. Unexplored cells stay silent and
// are not followed.
void DeliverPressMessage(PressWalk *walk, const AutoMapLink &link, AutoMapSignal signal,
                         int delayBefore, bool later, PressState *state,
                         std::vector<AutoMapEffect> *effects)
{
  const AutoMapDrawList &drawList = *walk->drawList;
  const AutoMapShownCell *cell = FindShownCell(drawList, link.dstX, link.dstY);
  if (cell == NULL) return;
  AutoMapEffect arrival;
  arrival.x = link.dstX;
  arrival.y = link.dstY;
  arrival.via = link;
  arrival.later = later;
  arrival.delayTicks = delayBefore + link.delay;
  // Only stone and open floors walk their records (ProcessTT_STONEROOM,
  // ProcessTT_OPENROOM); doors, false walls, pits and teleporters do not. Gates
  // operate in stone only.
  std::vector<PressRelayKey> keys;
  std::vector<const AutoMapRelay *> relays;
  if (cell->cell.kind == AutoMapCellWall)
  {
    for (size_t i = 0; i < cell->cell.wallRelays.size(); i++)
    {
      PressRelayKey key = {link.dstX, link.dstY, (int)i, false};
      keys.push_back(key);
      relays.push_back(&cell->cell.wallRelays[i]);
    }
  }
  if (cell->cell.kind == AutoMapCellFloor)
  {
    for (size_t i = 0; i < drawList.actuators.size(); i++)
    {
      const AutoMapFloorActuator &cog = drawList.actuators[i];
      if (cog.x != link.dstX || cog.y != link.dstY || cog.kind != AutoMapActuatorCog) continue;
      if (cog.relay.kind == AutoMapRelayGate) continue;
      PressRelayKey key = {link.dstX, link.dstY, (int)i, true};
      keys.push_back(key);
      relays.push_back(&cog.relay);
    }
  }
  std::vector<PressFollow> fired;
  std::vector<PressFollow> waiting;
  bool afterDSA = false;
  for (size_t i = 0; i < keys.size(); i++)
  {
    arrival.afterDSA = afterDSA;
    if (relays[i]->kind == AutoMapRelayDSA) afterDSA = true;
    AutoMapEffect effect;
    bool closed = false;
    bool followLater = false;
    if (!ReceivePressMessage(walk, keys[i], *relays[i], signal, arrival, state, &effect, &closed,
                             &followLater))
    {
      continue;
    }
    PressFollow follow = {effects->size(), keys[i], relays[i], closed,
                          arrival.delayTicks + relays[i]->delay};
    if (effect.kind != AutoMapEffectLoop && effect.kind != AutoMapEffectDSA)
    {
      if (effect.fires) fired.push_back(follow);
      else if (followLater) waiting.push_back(follow);
    }
    effects->push_back(effect);
  }
  // A cell with no receiver and no goal is still where the message goes: a bare target
  bool goal = PressReachesGoal(drawList, *cell, link);
  if ((goal || keys.empty()) && TakeNode(walk))
  {
    arrival.kind = AutoMapEffectGoal;
    arrival.afterDSA = afterDSA;
    arrival.target = !goal;
    effects->push_back(arrival);
  }
  // The cell's effects are all in place now, so the followers fill them where they stand
  std::stable_sort(fired.begin(), fired.end(), EarlierSend);
  for (size_t i = 0; i < fired.size() && !walk->truncated; i++)
  {
    walk->path.push_back(fired[i].key);
    FollowPressRelay(walk, fired[i].key, *fired[i].relay, fired[i].closed, later, state,
                     &(*effects)[fired[i].effect]);
    walk->path.pop_back();
  }
  for (size_t i = 0; i < waiting.size() && !walk->truncated; i++)
  {
    // What it sends once further messages make it fire, on its own copy of the state:
    // those messages are not part of this press. A CONSTPRESSURE relay sends SET
    // then, being closed.
    PressState laterState = *state;
    walk->path.push_back(waiting[i].key);
    FollowPressRelay(walk, waiting[i].key, *waiting[i].relay, true, true, &laterState,
                     &(*effects)[waiting[i].effect]);
    walk->path.pop_back();
  }
}

// A path of the press tree: the receivers from the source's first one down to the
// effect at its end
typedef std::vector<const AutoMapEffect *> PressPath;

// Why the relays on the path keep its end from ever happening, or NULL. The last entry
// is the end itself and is not looked at. A relay the path waits on (it fires later, or
// not on this message) that this press spent, being once-only, is inert by then.
const char *PressPathBlock(const PressPath &path, const PressState &spentBy)
{
  for (size_t i = 0; i + 1 < path.size(); i++)
  {
    const AutoMapEffect &relay = *path[i];
    if (relay.kind == AutoMapEffectCounter)
    {
      if (relay.before == 0) return "COUNTER AT 0";
      if (relay.via.signal == AutoMapSignalSet) return "COUNTS UP";
    }
    PressRelayKey key = {relay.x, relay.y, relay.relay, relay.floorRelay};
    if ((relay.later || !relay.fires) && spentBy.spent.count(key) != 0) return "SPENT";
  }
  return NULL;
}

// A message through a CONSTPRESSURE source or relay is taken back on release
bool PressPathHolds(const PressPath &path)
{
  for (size_t i = 0; i < path.size(); i++)
  {
    if (path[i]->via.signal == AutoMapSignalPressure) return true;
  }
  return false;
}

// A counter past the first on the path that needs more than one message. Fed by a
// counter, it cannot be finished by this source: the feeder is dead at 0, so it delivers
// at most one message. Fed by anything else (a gate), how often the feeder fires over
// further presses is not modelled, so the line stays conservative: it may under-promise,
// never over-promise.
bool PressPathNeedsOthers(const PressPath &path)
{
  for (size_t i = 1; i + 1 < path.size(); i++)
  {
    if (path[i]->kind == AutoMapEffectCounter && path[i]->before > 1) return true;
  }
  return false;
}

// One message the press tree delivers to a gate, and how many presses of the source
// bring it again: the first counter's remaining count when a counter fed by the source
// starts the path, 1 for any other path, -1 when it never comes again
struct PressGateMessage
{
  int x;
  int y;
  int relay;
  int bit;
  AutoMapSignal signal;
  int presses;
};

void CollectGateMessages(const std::vector<AutoMapEffect> &effects, const PressState &state,
                         PressPath *path, std::vector<PressGateMessage> *messages)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    const AutoMapEffect &effect = effects[i];
    path->push_back(&effect);
    if (effect.kind == AutoMapEffectGate)
    {
      const AutoMapEffect &first = *path->front();
      int presses = 1;
      if (path->size() > 1 && first.kind == AutoMapEffectCounter) presses = first.after;
      if (presses <= 0 || PressPathHolds(*path) || PressPathNeedsOthers(*path)
          || PressPathBlock(*path, state) != NULL)
      {
        presses = -1;
      }
      PressGateMessage message = {effect.x, effect.y, effect.relay, effect.via.targetPos,
                                  effect.via.signal, presses};
      messages->push_back(message);
    }
    CollectGateMessages(effect.next, state, path, messages);
    path->pop_back();
  }
}

// The presses after which the tree's messages to a gate that did not fire on the path
// make it fire, starting from the bits this press leaves, or -1 when no order of them
// does. The gate's own rules decide (GateReceives: normally closed, andOrEdge), so a
// match it ignores, or a TOGGLE into a SET-only gate, counts for nothing. A search over
// its 16 bit states, where a route costs the most presses any of its messages takes.
int PressesToFireGate(const std::vector<PressGateMessage> &messages, const AutoMapEffect &gate,
                      const AutoMapRelay &relay)
{
  std::vector<const PressGateMessage *> own;
  for (size_t i = 0; i < messages.size(); i++)
  {
    const PressGateMessage &message = messages[i];
    if (message.x != gate.x || message.y != gate.y || message.relay != gate.relay) continue;
    if (message.presses >= 0) own.push_back(&message);
  }
  int best[16];
  for (int bits = 0; bits < 16; bits++) best[bits] = -1;
  best[gate.after & 15] = 0;
  int fire = -1;
  bool changed = true;
  for (int round = 0; round < 16 && changed; round++)
  {
    changed = false;
    for (int bits = 0; bits < 16; bits++)
    {
      if (best[bits] < 0) continue;
      for (size_t i = 0; i < own.size(); i++)
      {
        bool closed = false;
        bool fires = false;
        int next = GateReceives(relay, bits, own[i]->bit, own[i]->signal, &closed, &fires);
        int cost = std::max(best[bits], own[i]->presses);
        if (fires)
        {
          if (fire < 0 || cost < fire) fire = cost;
        }
        else if (best[next] < 0 || cost < best[next])
        {
          best[next] = cost;
          changed = true;
        }
      }
    }
  }
  return fire;
}

// A message that leaves its goal as it stands: SET into an open false wall, CLEAR into a
// closed one (ProcessTT_FALSEWALL), or a door already where the message puts it
bool PressGoalAlreadyDone(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  const AutoMapShownCell *cell = FindShownCell(drawList, goal.x, goal.y);
  if (cell == NULL || goal.target) return false;
  AutoMapSignal signal = goal.via.signal;
  switch (cell->cell.kind)
  {
  case AutoMapCellDoor: return AutoMapScene_PredictDoorState(cell->cell.doorState, signal) < 0;
  case AutoMapCellOpenFalseWall: return signal == AutoMapSignalSet;
  case AutoMapCellFalseWall: return signal == AutoMapSignalClear;
  default: break;
  }
  if (!PressCellIsPit(cell->cell)) return false;
  bool open = cell->cell.kind == AutoMapCellPit && cell->cell.pitIsOpen;
  return signal == (open ? AutoMapSignalSet : AutoMapSignalClear);
}

AutoMapGoalDistance PressGoalDistance(const AutoMapDrawList &drawList, const PressPath &path,
                                      const PressState &state,
                                      const std::vector<PressGateMessage> &messages)
{
  AutoMapGoalDistance distance;
  const AutoMapEffect &goal = *path.back();
  // The gate bits this press leaves wrong, and the presses it takes this source to make
  // each gate on the path that did not fire fire after all; -1 once one is beyond it
  int fixPresses = 0;
  for (size_t i = 0; i + 1 < path.size(); i++)
  {
    const AutoMapEffect &gate = *path[i];
    if (gate.kind != AutoMapEffectGate) continue;
    const AutoMapShownCell *cell = FindShownCell(drawList, gate.x, gate.y);
    if (cell == NULL || gate.relay < 0 || gate.relay >= (int)cell->cell.wallRelays.size()) continue;
    const AutoMapRelay &relay = cell->cell.wallRelays[gate.relay];
    int off = (gate.after ^ (relay.value >> 4)) & 15;
    for (int bit = 0; bit < 4; bit++)
    {
      if ((off & (1 << bit)) != 0) distance.bitsOff++;
    }
    if (gate.fires || fixPresses < 0) continue;
    int presses = PressesToFireGate(messages, gate, relay);
    fixPresses = presses < 0 ? -1 : std::max(fixPresses, presses);
  }
  bool afterDSA = false;
  for (size_t i = 0; i < path.size(); i++) afterDSA = afterDSA || path[i]->afterDSA;
  const char *blocked = PressPathBlock(path, state);
  if (PressPathHolds(path)) distance.status = AutoMapGoalHolds;
  else if (afterDSA) distance.status = AutoMapGoalThroughDSA;
  else if (!goal.later)
  {
    // Every relay on the way fired: the goal happens now, unless a door or false wall
    // already stands the way the message puts it (a destroyed door ignores every message)
    distance.status = PressGoalAlreadyDone(drawList, goal) ? AutoMapGoalAlreadyDone
                                                            : AutoMapGoalOnThisPress;
  }
  else if (blocked != NULL)
  {
    distance.status = AutoMapGoalBlocked;
    distance.blocked = blocked;
  }
  else
  {
    // The first counter fed by the source counts this source's presses; any other
    // first relay passes every press on
    int more = path[0]->kind == AutoMapEffectCounter ? path[0]->after : 1;
    if (fixPresses < 0 || PressPathNeedsOthers(path)) more = -1;
    else more = std::max(more, fixPresses);
    if (more > 0)
    {
      distance.status = AutoMapGoalMore;
      distance.more = more;
    }
    else distance.status = AutoMapGoalNeedsOthers;
  }
  return distance;
}

void AssessPressGoals(const AutoMapDrawList &drawList, const PressState &state,
                      const std::vector<PressGateMessage> &messages, PressPath *path,
                      std::vector<AutoMapEffect> *effects)
{
  for (size_t i = 0; i < effects->size(); i++)
  {
    AutoMapEffect &effect = (*effects)[i];
    path->push_back(&effect);
    if (effect.kind == AutoMapEffectGoal)
    {
      effect.distance = PressGoalDistance(drawList, *path, state, messages);
    }
    AssessPressGoals(drawList, state, messages, path, &effect.next);
    path->pop_back();
  }
}

// The relays this press makes fire, on the press itself rather than later
void CollectFiredNow(const std::vector<AutoMapEffect> &effects, std::set<PressRelayKey> *fired)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    const AutoMapEffect &effect = effects[i];
    if (effect.later) continue;
    if (effect.fires && (effect.kind == AutoMapEffectCounter || effect.kind == AutoMapEffectGate))
    {
      PressRelayKey key = {effect.x, effect.y, effect.relay, effect.floorRelay};
      fired->insert(key);
    }
    CollectFiredNow(effect.next, fired);
  }
}

// A relay that waits after one message of the press and fires on another of the same
// press (two actuators of one face on two gate bits) does not wait for further presses:
// what it would send later is what it sends now, under the message that made it fire.
void DropSupersededWaits(const std::set<PressRelayKey> &fired, std::vector<AutoMapEffect> *effects)
{
  for (size_t i = 0; i < effects->size(); i++)
  {
    AutoMapEffect &effect = (*effects)[i];
    if (effect.later) continue;
    PressRelayKey key = {effect.x, effect.y, effect.relay, effect.floorRelay};
    if (!effect.fires && fired.count(key) != 0) effect.next.clear();
    DropSupersededWaits(fired, &effect.next);
  }
}

// Fills the distance of every goal of one source's finished tree. It runs once the
// walk is done because a gate's wrong bit may be put right by any message of the tree,
// and a once-only relay may be spent by a message after the path that waits on it;
// state is what the whole press left.
void AssessPressSource(const AutoMapDrawList &drawList, const PressState &state,
                       AutoMapPressSource *source)
{
  std::set<PressRelayKey> fired;
  CollectFiredNow(source->effects, &fired);
  DropSupersededWaits(fired, &source->effects);
  PressPath path;
  std::vector<PressGateMessage> messages;
  CollectGateMessages(source->effects, state, &path, &messages);
  AssessPressGoals(drawList, state, messages, &path, &source->effects);
}

}

// A link a relay of (x, y) sends: a stone relay's or a floor cog's. Hovering the relay
// cell presses nothing, so these are no press of it; RelayHoverLines lists them.
static bool SentByOwnRelay(const AutoMapDrawList &drawList, const AutoMapShownCell *own,
                           int x, int y, const AutoMapLink &link)
{
  if (link.source == AUTOMAP_NO_RECORD) return false;
  for (size_t r = 0; own != NULL && r < own->cell.wallRelays.size(); r++)
  {
    if (own->cell.wallRelays[r].record == link.source) return true;
  }
  for (size_t a = 0; a < drawList.actuators.size(); a++)
  {
    const AutoMapFloorActuator &cog = drawList.actuators[a];
    if (cog.x != x || cog.y != y || cog.kind != AutoMapActuatorCog) continue;
    if (cog.relay.record == link.source) return true;
  }
  return false;
}

// A link sent from the face (or, on a floor, the cell) a press source stands for
static bool SamePressFace(const AutoMapPressSource &source, const AutoMapLink &link)
{
  if (source.srcIsWall != link.srcIsWall) return false;
  return !link.srcIsWall || source.srcSide == link.srcSide;
}

std::vector<AutoMapPressSource> AutoMapScene_PressEffects(const AutoMapDrawList &drawList,
                                                          int x, int y)
{
  std::vector<AutoMapPressSource> sources;
  // Below All the relay machinery is stripped and the links are the learned ones
  if (drawList.spoilers != AutoMapSpoilersAll) return sources;
  // One click fires every actuator on the touched face (TouchWall walks them all) and a
  // step fires every pad of the cell, so a face or the pads are one press: their links
  // share one state in link order. Each press starts from the live state.
  std::vector<PressState> states;
  const AutoMapShownCell *own = FindShownCell(drawList, x, y);
  PressWalk walk;
  walk.drawList = &drawList;
  walk.nodes = 0;
  walk.truncated = false;
  std::vector<AutoMapLink> outgoing = AutoMapScene_LinksFrom(drawList, x, y);
  for (size_t i = 0; i < outgoing.size(); i++)
  {
    const AutoMapLink &link = outgoing[i];
    if (FindShownCell(drawList, link.dstX, link.dstY) == NULL) continue;
    if (SentByOwnRelay(drawList, own, x, y, link)) continue;
    size_t group = 0;
    while (group < sources.size() && !SamePressFace(sources[group], link)) group++;
    if (group == sources.size())
    {
      AutoMapPressSource source;
      source.source = link.source;
      source.srcIsWall = link.srcIsWall;
      source.srcSide = link.srcSide;
      sources.push_back(source);
      states.push_back(PressState());
    }
    if (walk.truncated)
    {
      sources[group].truncated = true;
      continue;
    }
    // A press closes the source: a CONSTPRESSURE switch or pad sends SET
    // (Code11f52.cpp, Mouse.cpp queue swclosed ? 0 : 1)
    DeliverPressMessage(&walk, link, PressSourceSignal(link), 0, false, &states[group],
                        &sources[group].effects);
    if (walk.truncated) sources[group].truncated = true;
  }
  for (size_t i = 0; i < sources.size(); i++) AssessPressSource(drawList, states[i], &sources[i]);
  return sources;
}

// ---- Tooltip lines of a press ----

static std::string PressLocation(const AutoMapDrawList &drawList, int x, int y)
{
  char where[64];
  std::snprintf(where, sizeof(where), "L%d (%d, %d)", drawList.level, x + drawList.offsetX,
                y + drawList.offsetY);
  return std::string(where);
}

// The relay record behind an effect, NULL for goals and loops
static const AutoMapRelay *PressEffectRelay(const AutoMapDrawList &drawList,
                                            const AutoMapEffect &effect)
{
  if (effect.relay < 0) return NULL;
  if (effect.floorRelay)
  {
    if (effect.relay >= (int)drawList.actuators.size()) return NULL;
    return &drawList.actuators[effect.relay].relay;
  }
  const AutoMapShownCell *cell = FindShownCell(drawList, effect.x, effect.y);
  if (cell == NULL || effect.relay >= (int)cell->cell.wallRelays.size()) return NULL;
  return &cell->cell.wallRelays[effect.relay];
}

// What a goal cell is called: the noun the tooltip puts before or after its place
static const char *PressGoalNoun(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  const AutoMapShownCell *cell = FindShownCell(drawList, goal.x, goal.y);
  if (cell == NULL || goal.target) return "TARGET";
  if (PressCellIsPit(cell->cell)) return "PIT";
  switch (cell->cell.kind)
  {
  case AutoMapCellDoor: return "DOOR";
  case AutoMapCellFalseWall:
  case AutoMapCellOpenFalseWall: return "FALSE WALL";
  case AutoMapCellPit: return "PIT";
  case AutoMapCellTeleporter:
  case AutoMapCellInvisibleTeleporter:
  case AutoMapCellDormantTeleporter: return "TELEPORTER";
  case AutoMapCellSpinner:
  case AutoMapCellDormantSpinner: return "SPINNER";
  case AutoMapCellWall: return "LAUNCHER";
  case AutoMapCellFloor: return "GENERATOR";
  default: return "TARGET";
  }
}

// What the message does to the goal, the way the arrows and RelayLines word it
static std::string PressGoalWord(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  const AutoMapShownCell *cell = FindShownCell(drawList, goal.x, goal.y);
  bool opens = cell != NULL && !goal.target && (cell->cell.kind == AutoMapCellDoor
                                || cell->cell.kind == AutoMapCellFalseWall
                                || cell->cell.kind == AutoMapCellOpenFalseWall
                                || PressCellIsPit(cell->cell));
  return opens ? DoorEffectWord(goal.via.signal) : SignalWord(goal.via.signal);
}

// A bare target has nothing to open, so it takes the signal's own verb. A toggle on a pit
// says which way the pit goes, since a closed one looks like floor and TOGGLES tells nothing.
static const char *PressGoalVerb(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  AutoMapSignal signal = goal.via.signal;
  if (signal == AutoMapSignalClear) return goal.target ? "CLEARS" : "CLOSES";
  if (signal == AutoMapSignalToggle)
  {
    const AutoMapShownCell *cell = FindShownCell(drawList, goal.x, goal.y);
    if (cell == NULL || goal.target || !PressCellIsPit(cell->cell)) return "TOGGLES";
    return cell->cell.kind == AutoMapCellPit && cell->cell.pitIsOpen ? "CLOSES" : "OPENS";
  }
  return goal.target ? "SETS" : "OPENS";
}

static std::string PressBitsOff(int bits)
{
  char text[32];
  std::snprintf(text, sizeof(text), "%d %s OFF", bits, bits == 1 ? "BIT" : "BITS");
  return std::string(text);
}

// How far a goal is, from its distance: the PUZZLE wording
static std::string PressDistanceWord(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  const AutoMapGoalDistance &distance = goal.distance;
  const char *verb = PressGoalVerb(drawList, goal);
  char more[32];
  switch (distance.status)
  {
  case AutoMapGoalOnThisPress: return std::string(verb) + " ON THIS PRESS";
  case AutoMapGoalMore:
    std::snprintf(more, sizeof(more), "%d MORE", distance.more);
    if (distance.bitsOff > 0) return PressBitsOff(distance.bitsOff) + ", " + more;
    return std::string(verb) + " IN " + more;
  case AutoMapGoalNeedsOthers:
    if (distance.bitsOff > 0) return PressBitsOff(distance.bitsOff) + ", NEEDS OTHER SOURCES";
    return "NEEDS OTHER SOURCES";
  case AutoMapGoalBlocked:
    if (distance.blocked != NULL) return std::string("BLOCKED: ") + distance.blocked;
    return "BLOCKED";
  case AutoMapGoalHolds: return "HOLDS WHILE PRESSED";
  case AutoMapGoalThroughDSA: return "THROUGH DSA";
  case AutoMapGoalAlreadyDone:
    return goal.via.signal == AutoMapSignalClear ? "ALREADY CLOSED" : "ALREADY OPEN";
  default: return verb;
  }
}

static const char *PressEdgeWord(int edgeMode)
{
  switch (edgeMode)
  {
  case 1: return " ON INPUT CHANGE";
  case 2: return " ON OUTPUT CHANGE";
  case 3: return " ON INPUT CHANGE ONLY";
  case 4: return " ON SET";
  case 5: return " ON CLEAR";
  case 6: return " ON BIT SET";
  case 7: return " ON BIT CLEAR";
  default: return "";
  }
}

static const char *PressLocalActionWord(int localActionType)
{
  if (localActionType == 10) return "SKILL BONUS";
  if (localActionType == 1 || localActionType == 2) return "ROTATES ACTUATORS";
  return "LOCAL ACTION";
}

// What a stone cell or an open floor hands messages to: the relays in the order the
// walk gives them the message
struct HoverReceiver
{
  const AutoMapRelay *relay;
  PressRelayKey key;
};

static void HoverReceivers(const AutoMapDrawList &drawList, const AutoMapShownCell &shown,
                           std::vector<HoverReceiver> *receivers)
{
  if (shown.cell.kind == AutoMapCellWall)
  {
    for (size_t i = 0; i < shown.cell.wallRelays.size(); i++)
    {
      HoverReceiver receiver = {&shown.cell.wallRelays[i], {shown.x, shown.y, (int)i, false}};
      receivers->push_back(receiver);
    }
  }
  if (shown.cell.kind == AutoMapCellFloor)
  {
    for (size_t i = 0; i < drawList.actuators.size(); i++)
    {
      const AutoMapFloorActuator &cog = drawList.actuators[i];
      if (cog.x != shown.x || cog.y != shown.y || cog.kind != AutoMapActuatorCog) continue;
      if (cog.relay.kind == AutoMapRelayGate) continue;
      HoverReceiver receiver = {&cog.relay, {shown.x, shown.y, (int)i, true}};
      receivers->push_back(receiver);
    }
  }
}

static std::string HoverLetter(size_t index)
{
  return std::string(1, index < 26 ? (char)('A' + index) : '?');
}

// The receivers of one kind a message reaches, by the letters the cell's own hover gives
// them: "GATE A", or "GATES A/B" since every gate of the cell takes the message. "" when the
// cell holds none of that kind.
static std::string HoverReceiverNames(const std::vector<HoverReceiver> &receivers,
                                      AutoMapRelayKind kind, const char *singular,
                                      const char *plural)
{
  std::string letters;
  int count = 0;
  for (size_t i = 0; i < receivers.size(); i++)
  {
    if (receivers[i].relay->kind != kind) continue;
    letters += (count == 0 ? "" : "/") + HoverLetter(i);
    count++;
  }
  if (count == 0) return "";
  return std::string(count == 1 ? singular : plural) + " " + letters;
}

// A receiver's kind with the letter the relay hover gives it, so a cell holding several
// counters says which one a press counts ("COUNTER B"); the bare kind when the cell is unseen
static std::string PressReceiverName(const AutoMapDrawList &drawList, const AutoMapEffect &effect,
                                     const char *kind)
{
  const AutoMapShownCell *shown = FindShownCell(drawList, effect.x, effect.y);
  if (shown == NULL) return kind;
  std::vector<HoverReceiver> receivers;
  HoverReceivers(drawList, *shown, &receivers);
  for (size_t i = 0; i < receivers.size(); i++)
  {
    if (receivers[i].key.relay == effect.relay && receivers[i].key.floorRelay == effect.floorRelay)
    {
      return std::string(kind) + " " + HoverLetter(i);
    }
  }
  return kind;
}

// The state a receiver went through, without the marks only the tree carries
static std::string PressStateText(const AutoMapDrawList &drawList, const AutoMapEffect &effect)
{
  const AutoMapRelay *relay = PressEffectRelay(drawList, effect);
  char text[64];
  switch (effect.kind)
  {
  case AutoMapEffectCounter:
  {
    std::string name = PressReceiverName(drawList, effect, "COUNTER");
    if (effect.before == 0) return name + " 0 (DEAD)";
    std::snprintf(text, sizeof(text), " %d->%d", effect.before, effect.after);
    return name + text;
  }
  case AutoMapEffectGate:
  {
    int pattern = relay != NULL ? (relay->value >> 4) & 15 : 0;
    return PressReceiverName(drawList, effect, "GATE") + " " + GateBits(effect.before) + "->"
           + GateBits(effect.after) + " (NEEDS " + GateBits(pattern) + ") "
           + ((effect.after & 15) == pattern ? "MATCH" : "NO MATCH");
  }
  case AutoMapEffectDSA: return PressReceiverName(drawList, effect, "DSA") + ": NOT FOLLOWED";
  case AutoMapEffectLocalAction:
    return PressLocalActionWord(relay != NULL ? relay->localActionType : -1);
  case AutoMapEffectLoop: return "LOOPS BACK";
  default: return "";
  }
}

// ONCE and NC marks, and the edge filter of a gate
static std::string PressRelayMarks(const AutoMapRelay *relay, bool gate)
{
  std::string marks;
  if (relay == NULL) return marks;
  if (gate) marks += PressEdgeWord(relay->edgeMode);
  if (relay->onceOnly) marks += " ONCE";
  if (relay->normallyClosed) marks += " NC";
  return marks;
}

// The line a goal takes in the tree, after its place
static std::string PressGoalTreeText(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  std::string noun = PressGoalNoun(drawList, goal);
  if (goal.distance.status == AutoMapGoalAlreadyDone)
  {
    return noun + (goal.via.signal == AutoMapSignalClear ? " ALREADY CLOSED" : " ALREADY OPEN");
  }
  return noun + PressGoalWord(drawList, goal);
}

// One effect and what follows it, a space of indent per depth on top of the block's one.
// condition is what lets the parent's message through ("AT 0: ", "MATCH: "). A gate takes
// two lines, the message that reaches it and the state it ends in, so its children sit
// one level deeper.
static void PressTreeLines(const AutoMapDrawList &drawList, const AutoMapEffect &effect,
                           int depth, const std::string &condition,
                           std::vector<std::string> *lines)
{
  std::string indent((size_t)(1 + depth), ' ');
  std::string line = indent + condition + PressLocation(drawList, effect.x, effect.y) + " ";
  const AutoMapRelay *relay = PressEffectRelay(drawList, effect);
  std::string after;
  if (effect.kind != AutoMapEffectLoop) after += DelayWord(effect.delayTicks);
  if (effect.afterDSA) after += " (IF DSA PASSES)";
  int childDepth = depth + 1;
  std::string childCondition;
  switch (effect.kind)
  {
  case AutoMapEffectGoal:
    lines->push_back(line + PressGoalTreeText(drawList, effect) + after);
    return;
  case AutoMapEffectGate:
  {
    char bit[32] = "";
    if (effect.via.targetPos >= 0 && effect.via.targetPos <= 3)
    {
      std::snprintf(bit, sizeof(bit), " BIT %d", effect.via.targetPos);
    }
    lines->push_back(line + PressReceiverName(drawList, effect, "GATE") + bit
                     + SignalWord(effect.via.signal) + after);
    lines->push_back(std::string((size_t)(2 + depth), ' ') + PressStateText(drawList, effect)
                     + PressRelayMarks(relay, true));
    childDepth = depth + 2;
    childCondition = relay != NULL && relay->normallyClosed ? "NO MATCH: " : "MATCH: ";
    break;
  }
  case AutoMapEffectCounter:
    lines->push_back(line + PressStateText(drawList, effect) + PressRelayMarks(relay, false)
                     + after);
    // Children of any counter (a CONSTPRESSURE one too) and of a gate (NO MATCH too) take
    // the same prefix by design: it names the usual case, the tree stays readable
    childCondition = "AT 0: ";
    break;
  default:
    lines->push_back(line + PressStateText(drawList, effect) + after);
    break;
  }
  for (size_t i = 0; i < effect.next.size(); i++)
  {
    PressTreeLines(drawList, effect.next[i], childDepth, childCondition, lines);
  }
}

// The compact view lists a goal wherever it sits in the tree
static void PressGoals(const std::vector<AutoMapEffect> &effects,
                       std::vector<const AutoMapEffect *> *goals)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind == AutoMapEffectGoal) goals->push_back(&effects[i]);
    PressGoals(effects[i].next, goals);
  }
}

// A line of the tooltip's press block; headers stand outside the line cap
struct PressLine
{
  std::string text;
  bool header;
  bool hint;
  // A warning that outranks the cap: always shown, never counted, after the +N MORE
  bool tail;
};

static PressLine MakePressLine(const std::string &text, bool header, bool hint)
{
  PressLine line = {text, header, hint, false};
  return line;
}

// Identical blocks fold into one with a count: "3X " goes after the indent of the first line
static void FoldPressBlocks(const std::vector<std::vector<std::string> > &blocks,
                            std::vector<PressLine> *out)
{
  std::vector<int> counts(blocks.size(), 0);
  std::vector<size_t> firsts;
  for (size_t i = 0; i < blocks.size(); i++)
  {
    size_t same = firsts.size();
    for (size_t f = 0; f < firsts.size(); f++)
    {
      if (blocks[firsts[f]] == blocks[i])
      {
        same = f;
        break;
      }
    }
    if (same == firsts.size()) firsts.push_back(i);
    counts[firsts[same]]++;
  }
  for (size_t f = 0; f < firsts.size(); f++)
  {
    const std::vector<std::string> &block = blocks[firsts[f]];
    for (size_t l = 0; l < block.size(); l++)
    {
      std::string text = block[l];
      if (l == 0 && counts[firsts[f]] > 1)
      {
        size_t indent = text.find_first_not_of(' ');
        char count[16];
        std::snprintf(count, sizeof(count), "%dX ", counts[firsts[f]]);
        text.insert(indent == std::string::npos ? 0 : indent, count);
      }
      out->push_back(MakePressLine(text, false, false));
    }
  }
}

// What a source is called when several share the cell: the face of a switch, or the pad
static std::string PressSourceHeader(const AutoMapPressSource &source)
{
  if (!source.srcIsWall) return "PAD:";
  if (source.srcSide < AutoMapNorth || source.srcSide > AutoMapWest) return "SWITCH:";
  return std::string("SWITCH (") + FacingInitial(source.srcSide) + "):";
}

// The cap counts every line but the headers and the tails. The hint counts as one and
// goes first; past the cap a line says how many were left out, before the next tail.
// uncapped (Shift held) lifts the cap: every line stays and no +N MORE is said.
static std::vector<std::string> CapPressLines(const std::vector<PressLine> &lines,
                                              const char *moreSuffix, bool uncapped)
{
  std::vector<std::string> result;
  size_t body = 0;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (!lines[i].header && !lines[i].hint && !lines[i].tail) body++;
  }
  size_t limit = uncapped ? body + 1 : AUTOMAP_PRESS_MAX_LINES;
  bool hint = body < limit;
  size_t shown = body > limit ? limit - 1 : body;
  size_t taken = 0;
  bool moreSaid = body <= shown;
  std::vector<std::string> pendingHeaders;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].hint)
    {
      if (hint) result.push_back(lines[i].text);
      continue;
    }
    if (lines[i].header)
    {
      pendingHeaders.push_back(lines[i].text);
      continue;
    }
    if (lines[i].tail || taken == shown)
    {
      if (!moreSaid && taken == shown)
      {
        char more[32];
        std::snprintf(more, sizeof(more), "+%d MORE%s", (int)(body - shown), moreSuffix);
        result.push_back(std::string(more));
        moreSaid = true;
      }
      if (lines[i].tail) result.push_back(lines[i].text);
      continue;
    }
    result.insert(result.end(), pendingHeaders.begin(), pendingHeaders.end());
    pendingHeaders.clear();
    result.push_back(lines[i].text);
    taken++;
  }
  if (!moreSaid)
  {
    char more[32];
    std::snprintf(more, sizeof(more), "+%d MORE%s", (int)(body - shown), moreSuffix);
    result.push_back(std::string(more));
  }
  return result;
}

// A link is one actuator's message to one cell; the same link reached by two paths (both
// counters of a cell sit behind the lever's one link) is drawn once.
static bool SamePressLink(const AutoMapLink &a, const AutoMapLink &b)
{
  return a.source == b.source && a.srcX == b.srcX && a.srcY == b.srcY && a.dstX == b.dstX
      && a.dstY == b.dstY && a.srcIsWall == b.srcIsWall && a.srcSide == b.srcSide;
}

static void CollectChainLinks(const std::vector<AutoMapEffect> &effects,
                              std::vector<AutoMapLink> *links)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    const AutoMapEffect &effect = effects[i];
    // A local action has no link: its via keeps the default, without a source record
    if (effect.via.source != AUTOMAP_NO_RECORD)
    {
      bool known = false;
      for (size_t j = 0; j < links->size() && !known; j++)
      {
        known = SamePressLink((*links)[j], effect.via);
      }
      if (!known) links->push_back(effect.via);
    }
    CollectChainLinks(effect.next, links);
  }
}

std::vector<AutoMapLink> AutoMapScene_PressChainLinks(const std::vector<AutoMapPressSource> &sources)
{
  std::vector<AutoMapLink> links;
  for (size_t i = 0; i < sources.size(); i++) CollectChainLinks(sources[i].effects, &links);
  return links;
}

std::string AutoMapScene_GoalBadge(const AutoMapGoalDistance &distance)
{
  char text[32];
  switch (distance.status)
  {
  case AutoMapGoalOnThisPress: return "NOW";
  case AutoMapGoalMore:
    if (distance.bitsOff > 0) return PressBitsOff(distance.bitsOff);
    std::snprintf(text, sizeof(text), "%d MORE", distance.more);
    return std::string(text);
  case AutoMapGoalNeedsOthers:
    return distance.bitsOff > 0 ? PressBitsOff(distance.bitsOff) : std::string("NEEDS OTHERS");
  case AutoMapGoalBlocked: return "BLOCKED";
  case AutoMapGoalHolds: return "HOLD";
  case AutoMapGoalThroughDSA: return "DSA";
  case AutoMapGoalAlreadyDone: return "DONE";
  default: return "";
  }
}

std::vector<std::string> AutoMapScene_PressLines(const AutoMapDrawList &drawList,
                                                 const std::vector<AutoMapPressSource> &sources,
                                                 bool fullChain)
{
  std::vector<PressLine> lines;
  bool several = sources.size() > 1;
  for (size_t s = 0; s < sources.size(); s++)
  {
    const AutoMapPressSource &source = sources[s];
    if (several) lines.push_back(MakePressLine(PressSourceHeader(source), true, false));
    if (fullChain)
    {
      if (!several) lines.push_back(MakePressLine("PRESS:", true, false));
      std::vector<std::vector<std::string> > blocks;
      for (size_t e = 0; e < source.effects.size(); e++)
      {
        blocks.push_back(std::vector<std::string>());
        PressTreeLines(drawList, source.effects[e], 0, "", &blocks.back());
      }
      FoldPressBlocks(blocks, &lines);
    }
    else
    {
      std::vector<std::vector<std::string> > pressed;
      for (size_t e = 0; e < source.effects.size(); e++)
      {
        const AutoMapEffect &effect = source.effects[e];
        if (effect.kind == AutoMapEffectGoal) continue;
        pressed.push_back(std::vector<std::string>(
            1, " " + PressLocation(drawList, effect.x, effect.y) + " "
               + PressStateText(drawList, effect)));
      }
      if (!pressed.empty())
      {
        lines.push_back(MakePressLine("THIS PRESS:", true, false));
        FoldPressBlocks(pressed, &lines);
      }
      std::vector<const AutoMapEffect *> goals;
      PressGoals(source.effects, &goals);
      std::vector<std::vector<std::string> > puzzle;
      for (size_t g = 0; g < goals.size(); g++)
      {
        puzzle.push_back(std::vector<std::string>(
            1, std::string(" ") + PressGoalNoun(drawList, *goals[g]) + " "
               + PressLocation(drawList, goals[g]->x, goals[g]->y) + " "
               + PressDistanceWord(drawList, *goals[g])));
      }
      if (!puzzle.empty())
      {
        lines.push_back(MakePressLine("PUZZLE:", true, false));
        FoldPressBlocks(puzzle, &lines);
      }
    }
    if (source.truncated)
    {
      lines.push_back(MakePressLine(" CHAIN CUT SHORT", false, false));
      lines.back().tail = true;
    }
  }
  if (lines.empty()) return std::vector<std::string>();
  if (!fullChain) lines.push_back(MakePressLine("SHIFT: FULL CHAIN", false, true));
  return CapPressLines(lines, fullChain ? "" : " (SHIFT)", fullChain);
}

static std::string HoverCount(int count, const char *singular, const char *plural)
{
  char text[48];
  std::snprintf(text, sizeof(text), "%d %s", count, count == 1 ? singular : plural);
  return std::string(text);
}

// What a link's message does in the cell it reaches: the relays that take it, then the goal
static std::string HoverTarget(const AutoMapDrawList &drawList, const AutoMapLink &link)
{
  const AutoMapShownCell *dst = FindShownCell(drawList, link.dstX, link.dstY);
  if (dst == NULL) return "";
  std::vector<HoverReceiver> receivers;
  HoverReceivers(drawList, *dst, &receivers);
  std::string gates = HoverReceiverNames(receivers, AutoMapRelayGate, "GATE", "GATES");
  std::string counters = HoverReceiverNames(receivers, AutoMapRelayCounter, "COUNTER", "COUNTERS");
  std::string dsas = HoverReceiverNames(receivers, AutoMapRelayDSA, "DSA", "DSAS");
  std::vector<std::string> parts;
  if (!gates.empty())
  {
    char bit[32] = "";
    if (link.targetPos >= 0 && link.targetPos <= 3)
    {
      std::snprintf(bit, sizeof(bit), " BIT %d", link.targetPos);
    }
    parts.push_back(gates + bit + SignalWord(PressSourceSignal(link)));
  }
  if (!counters.empty())
  {
    parts.push_back(counters + (PressSourceSignal(link) == AutoMapSignalSet ? " +1" : " -1"));
  }
  if (!dsas.empty()) parts.push_back(dsas);
  bool reaches = PressReachesGoal(drawList, *dst, link);
  if (reaches || receivers.empty())
  {
    AutoMapEffect goal;
    goal.x = link.dstX;
    goal.y = link.dstY;
    goal.via = link;
    goal.target = !reaches;
    parts.push_back(PressGoalTreeText(drawList, goal));
  }
  std::string text = PressLocation(drawList, link.dstX, link.dstY);
  for (size_t i = 0; i < parts.size(); i++) text += (i == 0 ? " " : ", ") + parts[i];
  return text;
}

std::vector<std::string> AutoMapScene_RelayHoverLines(const AutoMapDrawList &drawList, int x, int y,
                                                     bool full)
{
  std::vector<std::string> lines;
  if (drawList.spoilers != AutoMapSpoilersAll) return lines;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return lines;
  std::vector<HoverReceiver> receivers;
  HoverReceivers(drawList, *shown, &receivers);
  if (receivers.empty()) return lines;
  int counters = 0;
  int gates = 0;
  int dsas = 0;
  for (size_t i = 0; i < receivers.size(); i++)
  {
    if (receivers[i].relay->kind == AutoMapRelayCounter) counters++;
    else if (receivers[i].relay->kind == AutoMapRelayGate) gates++;
    else dsas++;
  }
  std::string header = shown->cell.kind == AutoMapCellWall ? "STONE: " : "FLOOR: ";
  std::string kinds;
  if (counters > 0) kinds += HoverCount(counters, "COUNTER", "COUNTERS");
  if (gates > 0) kinds += (kinds.empty() ? "" : ", ") + HoverCount(gates, "GATE", "GATES");
  if (dsas > 0) kinds += (kinds.empty() ? "" : ", ") + HoverCount(dsas, "DSA", "DSAS");
  lines.push_back(header + kinds);
  // Each lettered relay with what it feeds: a line per target
  const std::vector<AutoMapLink> &links = drawList.links;
  for (size_t r = 0; r < receivers.size(); r++)
  {
    const AutoMapRelay &relay = *receivers[r].relay;
    std::string state;
    char text[48];
    if (relay.kind == AutoMapRelayCounter)
    {
      std::snprintf(text, sizeof(text), "COUNT %d", relay.value);
      state = text;
    }
    else if (relay.kind == AutoMapRelayGate)
    {
      state = "GATE " + GateBits(relay.value & 15) + " (NEEDS " + GateBits((relay.value >> 4) & 15)
              + ")";
    }
    else state = "DSA";
    std::string head = " " + HoverLetter(r) + " " + state;
    size_t before = lines.size();
    if (relay.localActionType >= 0)
    {
      lines.push_back(head + " -> " + PressLocalActionWord(relay.localActionType)
                      + DelayWord(relay.delay));
    }
    for (size_t l = 0; relay.record != AUTOMAP_NO_RECORD && l < links.size(); l++)
    {
      const AutoMapLink &link = links[l];
      if (link.source != relay.record || link.srcX != x || link.srcY != y) continue;
      std::string target = HoverTarget(drawList, link);
      if (target.empty()) continue;
      if (relay.kind == AutoMapRelayCounter && link.signal != AutoMapSignalPressure)
      {
        target += " AT 0";
      }
      else if (relay.kind == AutoMapRelayGate && link.signal != AutoMapSignalPressure)
      {
        target += " ON MATCH";
      }
      lines.push_back(head + " -> " + target + DelayWord(link.delay));
    }
    if (lines.size() == before) lines.push_back(head);
  }
  // The sources that reach the cell, and what a press does to which letter
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(drawList, x, y);
  std::vector<std::string> feeders;
  std::vector<std::string> doings;
  for (size_t i = 0; i < incoming.size(); i++)
  {
    const AutoMapLink &link = incoming[i];
    std::string name;
    const AutoMapShownCell *from = FindShownCell(drawList, link.srcX, link.srcY);
    if (from != NULL && link.source != AUTOMAP_NO_RECORD)
    {
      for (size_t r = 0; r < from->cell.wallRelays.size(); r++)
      {
        if (from->cell.wallRelays[r].record != link.source) continue;
        const AutoMapRelay &relay = from->cell.wallRelays[r];
        name = relay.kind == AutoMapRelayCounter ? "COUNTER"
               : relay.kind == AutoMapRelayGate ? "GATE" : "DSA";
      }
      for (size_t a = 0; a < drawList.actuators.size(); a++)
      {
        const AutoMapFloorActuator &cog = drawList.actuators[a];
        if (cog.x != link.srcX || cog.y != link.srcY || cog.relay.record != link.source) continue;
        if (cog.kind != AutoMapActuatorCog) continue;
        name = cog.relay.kind == AutoMapRelayCounter ? "COUNTER" : "DSA";
      }
    }
    if (name.empty())
    {
      if (!link.srcIsWall) name = "PAD";
      else if (link.srcSide < AutoMapNorth || link.srcSide > AutoMapWest) name = "SWITCH";
      else name = std::string("SWITCH (") + FacingInitial(link.srcSide) + ")";
    }
    std::string feeder = PressLocation(drawList, link.srcX, link.srcY) + " " + name;
    size_t found = feeders.size();
    for (size_t f = 0; f < feeders.size(); f++)
    {
      if (feeders[f] == feeder && link.source != AUTOMAP_NO_RECORD)
      {
        found = f;
        break;
      }
    }
    if (found == feeders.size())
    {
      feeders.push_back(feeder);
      doings.push_back("");
    }
    for (size_t r = 0; r < receivers.size(); r++)
    {
      const AutoMapRelay &relay = *receivers[r].relay;
      std::string doing = HoverLetter(r);
      if (relay.kind == AutoMapRelayCounter)
      {
        doing += PressSourceSignal(link) == AutoMapSignalSet ? "+1" : "-1";
      }
      else if (relay.kind == AutoMapRelayGate)
      {
        if (link.targetPos >= 0 && link.targetPos <= 3)
        {
          char bit[32];
          std::snprintf(bit, sizeof(bit), " BIT %d", link.targetPos);
          doing += bit;
        }
        doing += SignalWord(PressSourceSignal(link));
      }
      else doing += "?";
      doings[found] += (doings[found].empty() ? "" : " ") + doing;
    }
  }
  // Fold identical lines, then cap like the press block; the two headers stay outside it
  std::vector<PressLine> capped;
  capped.push_back(MakePressLine(lines[0], true, false));
  std::vector<std::vector<std::string> > blocks;
  for (size_t i = 1; i < lines.size(); i++) blocks.push_back(std::vector<std::string>(1, lines[i]));
  FoldPressBlocks(blocks, &capped);
  blocks.clear();
  for (size_t f = 0; f < feeders.size(); f++)
  {
    blocks.push_back(std::vector<std::string>(1, " " + feeders[f] + " " + doings[f]));
  }
  if (!blocks.empty()) capped.push_back(MakePressLine("FED BY:", true, false));
  FoldPressBlocks(blocks, &capped);
  return CapPressLines(capped, "", full);
}
