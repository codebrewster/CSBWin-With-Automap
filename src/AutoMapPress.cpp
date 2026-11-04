// AutoMapPress.cpp : what a press does down the relay chain, and the tooltip lines that
// tell it (AutoMapScene_PressEffects, PressBlock, RelayBlock and their helpers).
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
    // It keeps the relay it comes back to, for the route to name
    effect->kind = AutoMapEffectLoop;
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
  arrival.sent = signal;
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

// What a message carries: a held link's SET or CLEAR this time, else the link's signal
AutoMapSignal PressCarried(const AutoMapEffect &effect)
{
  return effect.via.signal == AutoMapSignalPressure ? effect.sent : effect.via.signal;
}

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
      if (PressCarried(relay) == AutoMapSignalSet) return "COUNTS UP";
    }
    PressRelayKey key = {relay.x, relay.y, relay.relay, relay.floorRelay};
    if ((relay.later || !relay.fires) && spentBy.spent.count(key) != 0) return "SPENT";
  }
  return NULL;
}

// The relay record behind an effect, NULL for goals, loops and an index out of range
const AutoMapRelay *EffectRelay(const AutoMapDrawList &drawList, const AutoMapEffect &effect)
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

// The press itself is held (its source sends CONSTPRESSURE), and the release takes back
// what the press sent all the way to the end of the path, so what it reaches holds only
// while pressed. That is so straight from the source, and through held gates (CONSTPRESSURE)
// that this press turns (match to no match or back) and the release, the opposite message
// on the same bit, turns back past their filter. Nothing else carries the release:
// - a plain gate sends only when it closes, so the release finds it silent;
// - a counter driven to 0 is dead (OperateCounterActuator ignores every later message), and
//   one above 0 sends the same both times, held or not;
// - a held relay further on, after a plain one, reports what it really sends.
// Such a goal is the press's own (THIS PRESS) or comes later; it does not hold.
bool PressPathHolds(const AutoMapDrawList &drawList, const PressPath &path)
{
  if (path.empty() || path[0]->via.signal != AutoMapSignalPressure) return false;
  for (size_t i = 0; i + 1 < path.size(); i++)
  {
    const AutoMapEffect &step = *path[i];
    const AutoMapRelay *relay = EffectRelay(drawList, step);
    if (step.kind != AutoMapEffectGate || relay == NULL || relay->signal != AutoMapSignalPressure
        || path[i + 1]->via.signal != AutoMapSignalPressure || !step.fires || step.later)
    {
      return false;
    }
    AutoMapSignal pressed = PressCarried(step);
    if (pressed != AutoMapSignalSet && pressed != AutoMapSignalClear) return false;
    AutoMapSignal released = pressed == AutoMapSignalSet ? AutoMapSignalClear : AutoMapSignalSet;
    bool closedNow = false, firesNow = false, closedBack = false, firesBack = false;
    GateReceives(*relay, step.before, step.via.targetPos, pressed, &closedNow, &firesNow);
    int back = GateReceives(*relay, step.after, step.via.targetPos, released, &closedBack,
                            &firesBack);
    if (back != step.before || !firesBack || closedBack == closedNow) return false;
  }
  return true;
}

// Any held link on the path, source or relay: a held relay's later messages swing with its
// state, which the gate search does not model, so such a path brings a gate no sure message
bool PressPathHasHeldLink(const PressPath &path)
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
      if (presses <= 0 || PressPathHasHeldLink(*path) || PressPathNeedsOthers(*path)
          || PressPathBlock(*path, state) != NULL)
      {
        presses = -1;
      }
      PressGateMessage message = {effect.x, effect.y, effect.relay, effect.via.targetPos,
                                  PressCarried(effect), presses};
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
        // The path behind waits on the gate's closed send (SET). A held gate also sends on a
        // message that passes its filter without closing it, but that is CLEAR, the side the
        // path does not wait on: it only moves the bits on. A plain gate fires closed anyway.
        if (fires && closed)
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

// What the other pads of the cell do on the step that fires this source (pads of one
// cell with different triggers are separate sources, AutoMapScene_PressEffects). Pads
// sure to fire on that step are walked with the source, in the cell's link order and on
// its state; pads that may fire on it, or may not, are walked alone, only to see what
// they reach. Neither kind's goals are the source's own.
struct PressRivals
{
  PressRivals(void) : all(false)
  {
  }
  // A rival walk ran out of budget: what the rivals reach is not known
  bool all;
  // Counters and gates, and goal cells, a pad that may fire reaches (any message)
  std::set<PressRelayKey> mayRelays;
  std::set<std::pair<int, int> > mayCells;
  // Counters and gates a pad sure to fire reaches (any message), and the goal cells its
  // messages reach on the step itself
  std::set<PressRelayKey> sureRelays;
  std::set<std::pair<int, int> > sureCellsNow;
};

// Whether a rival pad leaves a goal less certain than the source's own walk says. A goal
// cell a rival reaches on the step (a sure pad) or at all (a maybe pad) may get a second,
// other message. A counter or gate on the path that a maybe pad reaches may hold other
// bits or another count than the walk assumed. One a sure pad reaches is simulated
// exactly up to this goal's message on this step (the sure pads are walked on the same
// state), so an ON THIS PRESS or ALREADY goal keeps its word there; a held goal or one
// that waits on further presses does not, as the gate search and the presses count only
// this source's own messages. Through a DSA stays as it is, and so do the blocks no
// message undoes: a dead counter and a spent once-only relay.
bool PressRivalsContest(const PressRivals &rivals, const PressPath &path,
                        const AutoMapGoalDistance &distance)
{
  if (distance.status == AutoMapGoalThroughDSA || distance.status == AutoMapGoalUnknown) return false;
  if (distance.status == AutoMapGoalBlocked && distance.blocked != NULL
      && std::string(distance.blocked) != "COUNTS UP")
  {
    return false;
  }
  if (rivals.all) return true;
  const AutoMapEffect &goal = *path.back();
  std::pair<int, int> cell(goal.x, goal.y);
  if (rivals.mayCells.count(cell) != 0 || rivals.sureCellsNow.count(cell) != 0) return true;
  bool exact = distance.status == AutoMapGoalOnThisPress || distance.status == AutoMapGoalAlreadyDone;
  for (size_t i = 0; i + 1 < path.size(); i++)
  {
    const AutoMapEffect &step = *path[i];
    if (step.kind != AutoMapEffectCounter && step.kind != AutoMapEffectGate) continue;
    PressRelayKey key = {step.x, step.y, step.relay, step.floorRelay};
    if (rivals.mayRelays.count(key) != 0) return true;
    if (!exact && rivals.sureRelays.count(key) != 0) return true;
  }
  return false;
}

// A message that leaves its goal as it stands: SET into an open false wall, CLEAR into a
// closed one (ProcessTT_FALSEWALL), or a door already where the message puts it
bool PressGoalAlreadyDone(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  const AutoMapShownCell *cell = FindShownCell(drawList, goal.x, goal.y);
  if (cell == NULL || goal.target) return false;
  AutoMapSignal signal = PressCarried(goal);
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
                                      const std::vector<PressGateMessage> &messages,
                                      const PressRivals &rivals)
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
    // A normally-closed gate wants a mismatch, so its bits are never "off"
    int off = relay.normallyClosed ? 0 : (gate.after ^ (relay.value >> 4)) & 15;
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
  // A held message (CONSTPRESSURE) is taken back on release. It holds the goal only when
  // the release carries back to it (PressPathHolds); otherwise a goal of this press is
  // the press's own like any other. A goal that waits on further messages is not reached
  // by press and release swinging a held source between two states, so the source alone
  // is not known to get it there (it may under-promise, never over-promise).
  bool heldSource = !path.empty() && path[0]->via.signal == AutoMapSignalPressure;
  bool holds = PressPathHolds(drawList, path);
  if (afterDSA) distance.status = AutoMapGoalThroughDSA;
  else if (holds && !goal.later) distance.status = AutoMapGoalHolds;
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
    if (heldSource || fixPresses < 0 || PressPathNeedsOthers(path)) more = -1;
    else more = std::max(more, fixPresses);
    if (more > 0)
    {
      distance.status = AutoMapGoalMore;
      distance.more = more;
    }
    else distance.status = AutoMapGoalNeedsOthers;
  }
  if (PressRivalsContest(rivals, path, distance))
  {
    // Other pads of the cell have a say: this source alone is not known to get it there
    distance = AutoMapGoalDistance();
    distance.status = AutoMapGoalNeedsOthers;
  }
  return distance;
}

// When a counter or gate passes the signal on: the distance of a goal that would sit
// right behind it, reached by its first send. That goal comes now only when the step
// fires on this press, as every goal behind it does. A bare target, so the cell's own
// state (an open door) has no say.
AutoMapGoalDistance PressStepPasses(const AutoMapDrawList &drawList, const AutoMapEffect &step,
                                    const PressState &state,
                                    const std::vector<PressGateMessage> &messages,
                                    const PressRivals &rivals, PressPath *path)
{
  AutoMapEffect behind;
  behind.kind = AutoMapEffectGoal;
  behind.x = step.x;
  behind.y = step.y;
  behind.target = true;
  behind.later = !(step.fires && !step.later);
  behind.via = step.next.empty() ? step.via : step.next[0].via;
  behind.delayTicks = step.delayTicks;
  path->push_back(&behind);
  AutoMapGoalDistance passes = PressGoalDistance(drawList, *path, state, messages, rivals);
  path->pop_back();
  return passes;
}

void AssessPressGoals(const AutoMapDrawList &drawList, const PressState &state,
                      const std::vector<PressGateMessage> &messages, const PressRivals &rivals,
                      PressPath *path, std::vector<AutoMapEffect> *effects)
{
  for (size_t i = 0; i < effects->size(); i++)
  {
    AutoMapEffect &effect = (*effects)[i];
    path->push_back(&effect);
    if (effect.kind == AutoMapEffectGoal)
    {
      effect.distance = PressGoalDistance(drawList, *path, state, messages, rivals);
    }
    if (effect.kind == AutoMapEffectCounter || effect.kind == AutoMapEffectGate)
    {
      effect.passes = PressStepPasses(drawList, effect, state, messages, rivals, path);
    }
    AssessPressGoals(drawList, state, messages, rivals, path, &effect.next);
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
// state is what the whole press left, rivals what the cell's other pads do.
void AssessPressSource(const AutoMapDrawList &drawList, const PressState &state,
                       const PressRivals &rivals, AutoMapPressSource *source)
{
  std::set<PressRelayKey> fired;
  CollectFiredNow(source->effects, &fired);
  DropSupersededWaits(fired, &source->effects);
  PressPath path;
  std::vector<PressGateMessage> messages;
  CollectGateMessages(source->effects, state, &path, &messages);
  AssessPressGoals(drawList, state, messages, rivals, &path, &source->effects);
}

// The counters and gates a rival's tree reaches, any message, and its goal cells: all of
// them, or (now) only those reached on the step itself
void CollectRivalReach(const std::vector<AutoMapEffect> &effects, bool now,
                       std::set<PressRelayKey> *relays, std::set<std::pair<int, int> > *cells)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    const AutoMapEffect &effect = effects[i];
    if (effect.kind == AutoMapEffectCounter || effect.kind == AutoMapEffectGate)
    {
      PressRelayKey key = {effect.x, effect.y, effect.relay, effect.floorRelay};
      relays->insert(key);
    }
    if (effect.kind == AutoMapEffectGoal && !(now && effect.later))
    {
      cells->insert(std::make_pair(effect.x, effect.y));
    }
    CollectRivalReach(effect.next, now, relays, cells);
  }
}

// Who can set a pad off (PressurePadOrAlcove, open-room branch): the party, a monster
// group, a dropped or thrown item. An object pad compares the moved thing's basic type
// with its value, and a monster is a moved thing too, so it is not ruled out for one.
const int PAD_BY_PARTY = 1;
const int PAD_BY_MONSTER = 2;
const int PAD_BY_ITEM = 4;

int PadActors(AutoMapPadOperator op)
{
  switch (op)
  {
  case AutoMapPadAnything: return PAD_BY_PARTY | PAD_BY_MONSTER | PAD_BY_ITEM;
  case AutoMapPadPartyOrMonster: return PAD_BY_PARTY | PAD_BY_MONSTER;
  case AutoMapPadParty:
  case AutoMapPadPartyFacing:
  case AutoMapPadPossession: return PAD_BY_PARTY;
  case AutoMapPadMonster: return PAD_BY_MONSTER;
  case AutoMapPadObject: return PAD_BY_ITEM | PAD_BY_MONSTER;
  default: return PAD_BY_PARTY | PAD_BY_MONSTER | PAD_BY_ITEM;
  }
}

// The actors a pad answers whatever else is true: a PARTY OR MONSTER pad answers the party
// (no monster shares its square) and a monster group (it checks for monsters other than
// the one moving); a PARTY pad the party; a MONSTER pad a monster. ANYTHING keeps still
// while items lie on it, FACING wants a facing, CARRY a possession, OBJECT an object type:
// none of them is sure.
int PadSureActors(AutoMapPadOperator op)
{
  switch (op)
  {
  case AutoMapPadPartyOrMonster: return PAD_BY_PARTY | PAD_BY_MONSTER;
  case AutoMapPadParty: return PAD_BY_PARTY;
  case AutoMapPadMonster: return PAD_BY_MONSTER;
  default: return 0;
  }
}

enum PadCoFire
{
  PadCoFireNever,
  PadCoFireMay,
  PadCoFireSure
};

// Whether pad q (sending link qLink) fires on the step that fires pad p (sending pLink).
// A plain normally-open pad fires on the step on, a plain normally-closed one on the step
// off, a held pad on both (switchClosed = pressed ^ NC, PressurePadOrAlcove). Sure means q
// answers every actor that sets p off, on p's step, sending what the walk gives it:
// PressSourceSignal on the step on; on the step off only a plain normally-closed pad sends
// what the walk gives it, and a plain normally-closed PARTY pad may be the place-only kind
// (type 9) that never fires. A pad the draw list does not know may do anything.
PadCoFire PadsCoFire(const AutoMapFloorActuator *p, const AutoMapLink &pLink,
                     const AutoMapFloorActuator *q, const AutoMapLink &qLink)
{
  if (p == NULL || q == NULL) return PadCoFireMay;
  int actors = PadActors(p->op);
  if ((actors & PadActors(q->op)) == 0) return PadCoFireNever;
  bool pHeld = pLink.signal == AutoMapSignalPressure;
  bool qHeld = qLink.signal == AutoMapSignalPressure;
  bool stepOn = pHeld || !pLink.sourceNormallyClosed;
  bool qOn = qHeld || !qLink.sourceNormallyClosed;
  bool qOff = qHeld || qLink.sourceNormallyClosed;
  if (stepOn ? !qOn : !qOff) return PadCoFireNever;
  if ((PadSureActors(q->op) & actors) != actors) return PadCoFireMay;
  if (stepOn) return PadCoFireSure;
  return !qHeld && q->op != AutoMapPadParty ? PadCoFireSure : PadCoFireMay;
}

}

// A link a relay of (x, y) sends: a stone relay's or a floor cog's. Hovering the relay
// cell presses nothing, so these are no press of it; RelayBlock lists them.
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

// A link sent from the face (or, on a floor, a pad with the same trigger) a press source
// stands for. One step fires every pad of the cell that wants what stepped on it, so pads
// with one trigger are one press; a pad the draw list does not know is its own.
static bool SamePressFace(const AutoMapDrawList &drawList, int x, int y,
                          const AutoMapPressSource &source, const AutoMapLink &link)
{
  if (source.srcIsWall != link.srcIsWall) return false;
  if (link.srcIsWall) return source.srcSide == link.srcSide;
  if (source.source == link.source) return true;
  const AutoMapFloorActuator *first = AutoMapScene_PadOfRecord(drawList, x, y, source.source);
  const AutoMapFloorActuator *other = AutoMapScene_PadOfRecord(drawList, x, y, link.source);
  return first != NULL && other != NULL && AutoMapScene_SamePadTrigger(*first, *other);
}

std::vector<AutoMapPressSource> AutoMapScene_PressEffects(const AutoMapDrawList &drawList,
                                                          int x, int y)
{
  std::vector<AutoMapPressSource> sources;
  // Below All the relay machinery is stripped and the links are the learned ones
  if (drawList.spoilers != AutoMapSpoilersAll) return sources;
  // One click fires every actuator on the touched face (TouchWall walks them all), so a
  // face is one press: its links share one state in link order. The pads of a cell are
  // one press per trigger, as pads react to different things (party, monster, object);
  // pads with the same trigger fire together. Each press starts from the live state.
  const AutoMapShownCell *own = FindShownCell(drawList, x, y);
  PressWalk walk;
  walk.drawList = &drawList;
  walk.nodes = 0;
  walk.truncated = false;
  std::vector<AutoMapLink> outgoing = AutoMapScene_LinksFrom(drawList, x, y);
  // The source each link belongs to, -1 for a link no press sends
  std::vector<int> groupOf(outgoing.size(), -1);
  for (size_t i = 0; i < outgoing.size(); i++)
  {
    const AutoMapLink &link = outgoing[i];
    if (FindShownCell(drawList, link.dstX, link.dstY) == NULL) continue;
    if (SentByOwnRelay(drawList, own, x, y, link)) continue;
    size_t group = 0;
    while (group < sources.size() && !SamePressFace(drawList, x, y, sources[group], link)) group++;
    if (group == sources.size())
    {
      AutoMapPressSource source;
      source.source = link.source;
      source.srcIsWall = link.srcIsWall;
      source.srcSide = link.srcSide;
      sources.push_back(source);
    }
    groupOf[i] = (int)group;
    sources[group].links++;
    if (link.onceOnly) sources[group].onceOnlyLinks++;
  }
  for (size_t g = 0; g < sources.size(); g++)
  {
    PressState state;
    PressRivals rivals;
    // The rivals get their own budget, so they never cut the source's own walk short
    PressWalk rivalWalk = walk;
    rivalWalk.nodes = 0;
    rivalWalk.truncated = false;
    std::vector<AutoMapEffect> sureEffects;
    std::vector<AutoMapEffect> mayEffects;
    for (size_t i = 0; i < outgoing.size(); i++)
    {
      const AutoMapLink &link = outgoing[i];
      if (groupOf[i] < 0) continue;
      if (groupOf[i] == (int)g)
      {
        if (walk.truncated)
        {
          sources[g].truncated = true;
          continue;
        }
        // A press closes the source: a CONSTPRESSURE switch or pad sends SET
        // (Code11f52.cpp, Mouse.cpp queue swclosed ? 0 : 1)
        DeliverPressMessage(&walk, link, PressSourceSignal(link), 0, false, &state,
                            &sources[g].effects);
        if (walk.truncated) sources[g].truncated = true;
        continue;
      }
      // Another face of a wall is not touched; another pad of the floor may be stepped on too
      if (sources[g].srcIsWall || link.srcIsWall) continue;
      // Sure only when sure for every pad of the source, never only when never for all
      const AutoMapFloorActuator *pad = AutoMapScene_PadOfRecord(drawList, x, y, link.source);
      bool sure = true, never = true;
      for (size_t m = 0; m < outgoing.size(); m++)
      {
        if (groupOf[m] != (int)g) continue;
        PadCoFire co = PadsCoFire(AutoMapScene_PadOfRecord(drawList, x, y, outgoing[m].source),
                                  outgoing[m], pad, link);
        sure = sure && co == PadCoFireSure;
        never = never && co == PadCoFireNever;
      }
      if (never) continue;
      if (sure)
      {
        DeliverPressMessage(&rivalWalk, link, PressSourceSignal(link), 0, false, &state,
                            &sureEffects);
      }
      else
      {
        // Whatever it finds, from the live state: a fresh state leaves no relay spent
        PressState alone;
        DeliverPressMessage(&rivalWalk, link, PressSourceSignal(link), 0, false, &alone,
                            &mayEffects);
      }
    }
    rivals.all = rivalWalk.truncated;
    CollectRivalReach(sureEffects, true, &rivals.sureRelays, &rivals.sureCellsNow);
    CollectRivalReach(mayEffects, false, &rivals.mayRelays, &rivals.mayCells);
    AssessPressSource(drawList, state, rivals, &sources[g]);
  }
  return sources;
}

// Lower is more imminent
static int GoalImminence(const AutoMapGoalDistance &d)
{
  switch (d.status)
  {
  case AutoMapGoalOnThisPress: return 0;
  case AutoMapGoalHolds: return 1;
  case AutoMapGoalAlreadyDone: return 2;
  case AutoMapGoalMore: return 3;
  case AutoMapGoalThroughDSA: return 4;
  case AutoMapGoalNeedsOthers: return 5;
  case AutoMapGoalBlocked: return 6;
  default: return 7;
  }
}

static bool GoalMoreImminent(const AutoMapGoalDistance &a, const AutoMapGoalDistance &b)
{
  int ra = GoalImminence(a), rb = GoalImminence(b);
  if (ra != rb) return ra < rb;
  return a.status == AutoMapGoalMore && a.more < b.more;
}

static void CollectGoalsByCell(const std::vector<AutoMapEffect> &effects,
                               std::vector<const AutoMapEffect *> *goals)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    const AutoMapEffect &effect = effects[i];
    if (effect.kind == AutoMapEffectGoal)
    {
      size_t g = 0;
      while (g < goals->size() && ((*goals)[g]->x != effect.x || (*goals)[g]->y != effect.y)) g++;
      if (g == goals->size()) goals->push_back(&effect);
      else if (GoalMoreImminent(effect.distance, (*goals)[g]->distance)) (*goals)[g] = &effect;
    }
    CollectGoalsByCell(effect.next, goals);
  }
}

std::vector<const AutoMapEffect *> AutoMapScene_PressGoalsByCell(
    const std::vector<AutoMapPressSource> &sources)
{
  std::vector<const AutoMapEffect *> goals;
  for (size_t i = 0; i < sources.size(); i++) CollectGoalsByCell(sources[i].effects, &goals);
  return goals;
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
  return EffectRelay(drawList, effect);
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

// A bare target has nothing to open, so it takes the signal's own verb. A toggle on a pit
// says which way the pit goes, since a closed one looks like floor and TOGGLES tells nothing.
static const char *PressGoalVerb(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  AutoMapSignal signal = PressCarried(goal);
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

// How far a goal is, from its distance: the RESULT wording
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
    return PressCarried(goal) == AutoMapSignalClear ? "ALREADY CLOSED" : "ALREADY OPEN";
  default: return verb;
  }
}

static const char *PressLocalActionWord(int localActionType)
{
  if (localActionType == 10) return "SKILL BONUS";
  if (localActionType == 1 || localActionType == 2) return "ROTATES ACTUATORS";
  return "LOCAL ACTION";
}

std::string AutoMapScene_GateFilterWords(int edgeMode)
{
  // GateReceives' andOrEdge cases, in the same order
  switch (edgeMode)
  {
  case 1: return ", ONLY WHEN A BIT CHANGES";
  case 2: return ", ONLY WHEN THE MATCH CHANGES";
  case 3: return ", ONLY WHEN A BIT CHANGES BUT THE MATCH DOES NOT";
  case 4: return ", ONLY ON SET";
  case 5: return ", ONLY ON CLEAR";
  case 6: return ", ONLY WHEN THE BIT TURNS ON";
  case 7: return ", ONLY WHEN THE BIT TURNS OFF";
  default: return "";
  }
}

// When a gate sends, without the colon: on a match, or on no match when normally closed,
// then its filter. One wording for the route and the relay hover.
static std::string GateConditionWords(const AutoMapRelay &relay)
{
  return std::string(relay.normallyClosed ? "ON NO MATCH" : "ON MATCH")
         + AutoMapScene_GateFilterWords(relay.edgeMode);
}

// A held relay (CONSTPRESSURE) reports on every message it takes: SET while closed, CLEAR
// otherwise (PressSentSignal). Closed is a counter at 0 or a gate whose bits match, the
// other way round when normally closed. The words for one side, by what it sends: AT 0 or
// ABOVE 0 for a counter, WHILE MATCHING or WHILE NOT MATCHING for a gate.
static std::string HeldSideWords(const AutoMapRelay &relay, bool sendsSet)
{
  bool atZeroOrMatching = sendsSet != relay.normallyClosed;
  if (relay.kind == AutoMapRelayGate)
  {
    return atZeroOrMatching ? "WHILE MATCHING" : "WHILE NOT MATCHING";
  }
  return atZeroOrMatching ? "AT 0" : "ABOVE 0";
}

// A held relay's holding words in the state lines
static const char *HeldReportsWords(const AutoMapRelay &relay)
{
  return relay.kind == AutoMapRelayGate ? "HOLD: REPORTS EVERY MESSAGE" : "HOLD: REPORTS EVERY COUNT";
}

// What sends a link: the relay of its source record (COUNTER, GATE, DSA), else a PAD on the
// floor or a SWITCH on a wall, with the face it sits on when known: SWITCH (N)
static std::string PressFeederName(const AutoMapDrawList &drawList, const AutoMapLink &link)
{
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
  if (!name.empty()) return name;
  if (!link.srcIsWall) return "PAD";
  if (link.srcSide < AutoMapNorth || link.srcSide > AutoMapWest) return "SWITCH";
  return std::string("SWITCH (") + FacingInitial(link.srcSide) + ")";
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

// What a relay hover says a goal does, after its place
static std::string PressGoalTreeText(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  std::string noun = PressGoalNoun(drawList, goal);
  if (goal.distance.status == AutoMapGoalAlreadyDone)
  {
    return noun + (PressCarried(goal) == AutoMapSignalClear ? " ALREADY CLOSED" : " ALREADY OPEN");
  }
  return noun + " " + PressGoalVerb(drawList, goal);
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
  AutoMapTipStyle style;
};

typedef std::vector<PressLine> PressLines;

static PressLine MakePressLine(const std::string &text, bool header, bool hint)
{
  PressLine line = {text, header, hint, false, AutoMapTipPlain};
  return line;
}

static bool operator==(const PressLine &a, const PressLine &b)
{
  return a.text == b.text && a.header == b.header && a.hint == b.hint && a.tail == b.tail
      && a.style == b.style;
}

// Identical blocks fold into one with a count: "3X " goes after the indent of the first line
static void FoldStyledBlocks(const std::vector<PressLines> &blocks, PressLines *out)
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
    const PressLines &block = blocks[firsts[f]];
    for (size_t l = 0; l < block.size(); l++)
    {
      PressLine line = block[l];
      if (l == 0 && counts[firsts[f]] > 1)
      {
        size_t indent = line.text.find_first_not_of(' ');
        char count[16];
        std::snprintf(count, sizeof(count), "%dX ", counts[firsts[f]]);
        line.text.insert(indent == std::string::npos ? 0 : indent, count);
      }
      out->push_back(line);
    }
  }
}

// FoldStyledBlocks for plain one-style blocks
static void FoldPressBlocks(const std::vector<std::vector<std::string> > &blocks,
                            std::vector<PressLine> *out)
{
  std::vector<PressLines> styled(blocks.size());
  for (size_t i = 0; i < blocks.size(); i++)
  {
    for (size_t l = 0; l < blocks[i].size(); l++)
    {
      styled[i].push_back(MakePressLine(blocks[i][l], false, false));
    }
  }
  FoldStyledBlocks(styled, out);
}

// The cap counts every line but the headers and the tails. The hint counts as one and
// goes first; past the cap a line says how many were left out (hint style), before the
// next tail. uncapped (Shift held) lifts the cap: every line stays and no +N MORE is said.
static PressLines CapStyledLines(const PressLines &lines, const char *moreSuffix, bool uncapped)
{
  PressLines result;
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
  PressLine more = MakePressLine("", false, false);
  more.style = AutoMapTipHint;
  char moreText[32];
  std::snprintf(moreText, sizeof(moreText), "+%d MORE%s", (int)(body - shown), moreSuffix);
  more.text = moreText;
  PressLines pendingHeaders;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (lines[i].hint)
    {
      if (hint) result.push_back(lines[i]);
      continue;
    }
    if (lines[i].header)
    {
      pendingHeaders.push_back(lines[i]);
      continue;
    }
    if (lines[i].tail || taken == shown)
    {
      if (!moreSaid && taken == shown)
      {
        result.push_back(more);
        moreSaid = true;
      }
      if (lines[i].tail) result.push_back(lines[i]);
      continue;
    }
    result.insert(result.end(), pendingHeaders.begin(), pendingHeaders.end());
    pendingHeaders.clear();
    result.push_back(lines[i]);
    taken++;
  }
  if (!moreSaid) result.push_back(more);
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

static bool EffectsHaveSteps(const std::vector<AutoMapEffect> &effects)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind != AutoMapEffectGoal || effects[i].afterDSA) return true;
    if (EffectsHaveSteps(effects[i].next)) return true;
  }
  return false;
}

bool AutoMapScene_PressHasSteps(const AutoMapPressSource &source)
{
  return EffectsHaveSteps(source.effects);
}

std::string AutoMapScene_WhenNote(const AutoMapGoalDistance &distance)
{
  std::string off = distance.bitsOff > 0 ? PressBitsOff(distance.bitsOff) + ", " : "";
  char more[32];
  switch (distance.status)
  {
  case AutoMapGoalOnThisPress: return "(THIS PRESS)";
  case AutoMapGoalMore:
    if (distance.more == 1) return "(" + off + "NEXT PRESS)";
    std::snprintf(more, sizeof(more), "IN %d MORE)", distance.more);
    return "(" + off + more;
  case AutoMapGoalNeedsOthers: return "(" + off + "NEEDS OTHER SOURCES)";
  case AutoMapGoalBlocked:
    if (distance.blocked != NULL) return std::string("(BLOCKED: ") + distance.blocked + ")";
    return "(BLOCKED)";
  case AutoMapGoalHolds: return "(EVERY PRESS)";
  case AutoMapGoalThroughDSA: return "(IF DSA PASSES)";
  default: return "";
  }
}

// A goal's RESULT line, one space in: noun, place, how far, and its delay after a comma
static std::string PressResultLine(const AutoMapDrawList &drawList, const AutoMapEffect &goal)
{
  std::string line = std::string(" ") + PressGoalNoun(drawList, goal) + " "
      + PressLocation(drawList, goal.x, goal.y) + " " + PressDistanceWord(drawList, goal);
  std::string delay = DelayWord(goal.delayTicks);
  if (!delay.empty()) line += "," + delay;
  return line;
}

// ---- The route (Shift held) ----
// A line one step further in only happens when the line above lets the signal through:
// under a counter or gate its state lines, then its condition, then, one further in, what
// it sends. Lines at one depth all get the same message, in the walk's order.

// A route line at depth (one space per depth here; the block doubles it)
static PressLine RouteLine(int depth, const std::string &text, AutoMapTipStyle style)
{
  PressLine line = MakePressLine(std::string((size_t)depth, ' ') + text, false, false);
  line.style = style;
  return line;
}

// What a step line ends in: its own message's delay after a comma (the wait after the step
// above lets it through; RESULT gives the total), and the DSA before it in its cell
static std::string RouteStepTail(const AutoMapEffect &effect, int parentDelay)
{
  std::string tail;
  int delay = effect.delayTicks - parentDelay;
  if (delay > 0) tail += "," + DelayWord(delay);
  if (effect.afterDSA) tail += " (IF DSA PASSES)";
  return tail;
}

// A held link as the message it carries: SET from a closed sender, CLEAR from an open one
static AutoMapLink PressedVia(AutoMapLink via, bool closed)
{
  via.signal = PressSentSignal(via, closed);
  return via;
}

// A goal as the route names it: noun, place and what the message does, or that it stands
// that way already. A held message (CONSTPRESSURE) reads as what it carries this time.
static std::string RouteGoalText(const AutoMapDrawList &drawList, AutoMapEffect goal,
                                 bool closed)
{
  if (goal.via.signal == AutoMapSignalPressure) goal.via = PressedVia(goal.via, closed);
  std::string text = std::string(PressGoalNoun(drawList, goal)) + " "
      + PressLocation(drawList, goal.x, goal.y) + " ";
  if (goal.distance.status == AutoMapGoalAlreadyDone)
  {
    return text + (goal.via.signal == AutoMapSignalClear ? "ALREADY CLOSED" : "ALREADY OPEN");
  }
  return text + PressGoalVerb(drawList, goal);
}

// A goal a held relay feeds, with both sides: DOOR L9 (16, 4) OPENS AT 0, CLOSES ABOVE 0, or
// OPENS WHILE MATCHING, CLOSES OTHERWISE for a gate. The verbs are those of the SET and the
// CLEAR the held link carries.
static std::string HeldGoalText(const AutoMapDrawList &drawList, const AutoMapEffect &goal,
                                const AutoMapRelay &relay, bool closed)
{
  if (goal.via.signal != AutoMapSignalPressure || goal.distance.status == AutoMapGoalAlreadyDone)
  {
    return RouteGoalText(drawList, goal, closed);
  }
  AutoMapEffect set = goal;
  set.via.signal = AutoMapSignalSet;
  AutoMapEffect clear = goal;
  clear.via.signal = AutoMapSignalClear;
  return std::string(PressGoalNoun(drawList, goal)) + " " + PressLocation(drawList, goal.x, goal.y)
         + " " + PressGoalVerb(drawList, set) + " " + HeldSideWords(relay, true) + ", "
         + PressGoalVerb(drawList, clear) + " "
         + (relay.kind == AutoMapRelayGate ? "OTHERWISE" : HeldSideWords(relay, false));
}

// What a message did to a gate's bit. A held message (CONSTPRESSURE) arrives as SET or
// CLEAR, which the bit it left tells.
static const char *RouteGateVerb(const AutoMapEffect &gate)
{
  int bit = gate.via.targetPos;
  switch (gate.via.signal)
  {
  case AutoMapSignalSet: return "SETS";
  case AutoMapSignalClear: return "CLEARS";
  case AutoMapSignalToggle: return "TOGGLES";
  default: break;
  }
  if (bit < 0 || bit > 3) return "SENDS";
  return (gate.after & (1 << bit)) != 0 ? "SETS" : "CLEARS";
}

// What the route walks with: the map, and every link the route already shows
struct RouteContext
{
  const AutoMapDrawList *drawList;
  std::vector<AutoMapLink> links;
};

// The links the route shows: the message of every step and goal. Unlike the chain the map
// lights, a hand-built link without a source record counts too; a local action has none.
static void RouteLinks(const std::vector<AutoMapEffect> &effects, std::vector<AutoMapLink> *links)
{
  for (size_t i = 0; i < effects.size(); i++)
  {
    if (effects[i].kind != AutoMapEffectLocalAction) links->push_back(effects[i].via);
    RouteLinks(effects[i].next, links);
  }
}

static void RouteSteps(const RouteContext &route, const std::vector<AutoMapEffect> &effects,
                       int depth, int parentDelay, bool upstreamBlocked, bool closed,
                       PressLines *out);

// Whether a relay that took its message is closed, as ReceivePressMessage works it out: a
// counter at 0, a gate whose bits match, inverted when normally closed
static bool RouteStepClosed(const AutoMapEffect &step, const AutoMapRelay *relay)
{
  bool nc = relay != NULL && relay->normallyClosed;
  if (step.kind == AutoMapEffectCounter) return (step.after == 0) != nc;
  int pattern = relay != NULL ? (relay->value >> 4) & 15 : 0;
  return ((step.after & 15) == pattern) != nc;
}

static bool LowerTargetPos(const AutoMapLink &a, const AutoMapLink &b)
{
  return a.targetPos < b.targetPos;
}

// A gate's other inputs, by bit: one line per link into its cell that the route does not show
// already, on another bit or on the same one from another source (BIT 2 FROM: SWITCH (W)
// L11 (50, 24)). Every gate of the cell takes each of them.
static void RouteGateInputs(const RouteContext &route, const AutoMapEffect &gate, int depth,
                            PressLines *out)
{
  const AutoMapDrawList &drawList = *route.drawList;
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(drawList, gate.x, gate.y);
  // By bit, then in link order
  std::stable_sort(incoming.begin(), incoming.end(), LowerTargetPos);
  std::vector<std::string> said;
  for (size_t i = 0; i < incoming.size(); i++)
  {
    const AutoMapLink &link = incoming[i];
    bool onRoute = false;
    for (size_t j = 0; j < route.links.size() && !onRoute; j++)
    {
      onRoute = SamePressLink(route.links[j], link);
    }
    if (onRoute) continue;
    std::string text = link.targetPos >= 0 && link.targetPos <= 3
        ? "BIT " + std::to_string(link.targetPos) + " FROM: " : std::string("INPUT FROM: ");
    text += PressFeederName(drawList, link) + " " + PressLocation(drawList, link.srcX, link.srcY);
    if (std::find(said.begin(), said.end(), text) != said.end()) continue;
    said.push_back(text);
    out->push_back(RouteLine(depth, text, AutoMapTipState));
  }
}

// One step of the route and all that follows from it. parentDelay: the delay of the step
// above, which this one's own delay comes after. upstreamBlocked: the step above already
// reads BLOCKED, so a block on this one may be that one's. closed: whether its sender was
// closed, which says what a held message carries.
static void RouteStep(const RouteContext &route, const AutoMapEffect &effect, int depth,
                      int parentDelay, bool upstreamBlocked, bool closed, PressLines *out)
{
  const AutoMapDrawList &drawList = *route.drawList;
  const AutoMapRelay *relay = PressEffectRelay(drawList, effect);
  switch (effect.kind)
  {
  case AutoMapEffectGoal:
    out->push_back(RouteLine(depth, RouteGoalText(drawList, effect, closed)
                                        + RouteStepTail(effect, parentDelay),
                             AutoMapTipResult));
    return;
  case AutoMapEffectLoop:
    out->push_back(RouteLine(depth, "LOOPS BACK TO " + PressReceiverName(drawList, effect,
        relay != NULL && relay->kind == AutoMapRelayGate ? "GATE" : "COUNTER")
        + " " + PressLocation(drawList, effect.x, effect.y), AutoMapTipPlain));
    return;
  case AutoMapEffectLocalAction:
    out->push_back(RouteLine(depth, PressLocalActionWord(relay != NULL ? relay->localActionType : -1),
                             AutoMapTipPlain));
    return;
  case AutoMapEffectDSA:
    out->push_back(RouteLine(depth, PressReceiverName(drawList, effect, "DSA") + " "
                                        + PressLocation(drawList, effect.x, effect.y)
                                        + RouteStepTail(effect, parentDelay),
                             AutoMapTipPlain));
    out->push_back(RouteLine(depth + 1, "IF ITS PROGRAM PASSES IT ON (NOT FOLLOWED)",
                             AutoMapTipCondition));
    return;
  default:
    break;
  }
  bool gate = effect.kind == AutoMapEffectGate;
  std::string step = PressReceiverName(drawList, effect, gate ? "GATE" : "COUNTER") + " "
      + PressLocation(drawList, effect.x, effect.y);
  if (gate)
  {
    step += std::string(", ") + RouteGateVerb(effect);
    if (effect.via.targetPos >= 0 && effect.via.targetPos <= 3)
    {
      step += " BIT " + std::to_string(effect.via.targetPos);
    }
  }
  out->push_back(RouteLine(depth, step + RouteStepTail(effect, parentDelay), AutoMapTipPlain));
  // State: what this message does to it, then fixed facts
  std::string state;
  char text[64];
  if (gate)
  {
    int pattern = relay != NULL ? (relay->value >> 4) & 15 : 0;
    state = "BITS " + GateBits(effect.before) + " -> " + GateBits(effect.after) + ", NEEDS "
            + GateBits(pattern);
  }
  else if (effect.before == 0) state = "AT 0, DEAD";
  else
  {
    std::snprintf(text, sizeof(text), "%s %d -> %d",
                  effect.after > effect.before || effect.after == 511 ? "COUNTS UP" : "COUNTS",
                  effect.before,
                  effect.after);
    state = text;
  }
  out->push_back(RouteLine(depth + 1, state, AutoMapTipState));
  if (gate) RouteGateInputs(route, effect, depth + 1, out);
  // A once-only relay this press already spent, the path waiting on it, is inert by then.
  // When the step above is blocked, the block may be that one's: then only ONCE ONLY.
  bool blocked = effect.passes.status == AutoMapGoalBlocked;
  if (relay != NULL && relay->onceOnly)
  {
    bool spent = blocked && !upstreamBlocked && effect.passes.blocked != NULL
                 && std::string(effect.passes.blocked) == "SPENT";
    out->push_back(RouteLine(depth + 1, spent ? "SPENT" : "ONCE ONLY", AutoMapTipState));
  }
  // A CONSTPRESSURE relay reports on every message it takes, whatever its count or bits
  bool held = relay != NULL && relay->signal == AutoMapSignalPressure;
  if (held) out->push_back(RouteLine(depth + 1, HeldReportsWords(*relay), AutoMapTipState));
  if (effect.next.empty()) return;
  // Condition: when it passes the message on, then what it sends, one further in. A gate's
  // filter holds for a held gate too: it reports only the messages that pass it.
  std::string filter = gate && relay != NULL ? AutoMapScene_GateFilterWords(relay->edgeMode) : "";
  std::string note = AutoMapScene_WhenNote(effect.passes);
  // What it sends now, or (waiting for more messages) once it fires: closed then
  bool closedNow = !effect.fires || RouteStepClosed(effect, relay);
  if (held)
  {
    bool goalsOnly = true;
    for (size_t i = 0; i < effect.next.size(); i++)
    {
      goalsOnly = goalsOnly && effect.next[i].kind == AutoMapEffectGoal;
    }
    if (goalsOnly)
    {
      // Every message reaches the goals, so each goal line names both sides
      std::string condition = gate ? "EACH MESSAGE" + filter + ":" : "EACH COUNT:";
      out->push_back(RouteLine(depth + 1, note.empty() ? condition : condition + " " + note,
                               AutoMapTipCondition));
      std::vector<PressLines> goals(effect.next.size());
      for (size_t i = 0; i < effect.next.size(); i++)
      {
        const AutoMapEffect &goal = effect.next[i];
        goals[i].push_back(RouteLine(depth + 2, HeldGoalText(drawList, goal, *relay, closedNow)
                                                    + RouteStepTail(goal, effect.delayTicks),
                                     AutoMapTipResult));
      }
      FoldStyledBlocks(goals, out);
      return;
    }
    // Feeding a counter or gate, the walk follows one side, the one this message gives
    // (or, when the relay waits on further messages, the closed one): that side's
    // condition and what it sends, then the other side, said not to be followed. A side
    // that comes on this press holds only for this press: the next message may turn it.
    std::string side = HeldSideWords(*relay, closedNow);
    if (!gate && side == "AT 0") side = "WHILE AT 0";
    if (effect.passes.status == AutoMapGoalHolds) note = "(THIS PRESS)";
    out->push_back(RouteLine(depth + 1, side + filter + ":" + (note.empty() ? "" : " " + note),
                             AutoMapTipCondition));
    RouteSteps(route, effect.next, depth + 2, effect.delayTicks, blocked, closedNow, out);
    std::string other = HeldSideWords(*relay, !closedNow);
    if (!gate && other == "AT 0") other = "WHILE AT 0";
    out->push_back(RouteLine(depth + 1, other + filter + ": SENDS "
                                            + (closedNow ? "CLEAR" : "SET") + " (NOT FOLLOWED)",
                             AutoMapTipCondition));
    return;
  }
  std::string condition = gate ? (relay != NULL ? GateConditionWords(*relay) : "ON MATCH") + ":"
                               : "AT 0:";
  if (!note.empty()) condition += " " + note;
  out->push_back(RouteLine(depth + 1, condition, AutoMapTipCondition));
  RouteSteps(route, effect.next, depth + 2, effect.delayTicks, blocked, closedNow, out);
}

// Siblings of the route in the walk's order; identical ones fold as 2X
static void RouteSteps(const RouteContext &route, const std::vector<AutoMapEffect> &effects,
                       int depth, int parentDelay, bool upstreamBlocked, bool closed,
                       PressLines *out)
{
  std::vector<PressLines> blocks(effects.size());
  for (size_t i = 0; i < effects.size(); i++)
  {
    // From the source itself: pressing closes it, or opens a normally-closed one
    bool sender = depth == 1 ? !effects[i].via.sourceNormallyClosed : closed;
    RouteStep(route, effects[i], depth, parentDelay, upstreamBlocked, sender, &blocks[i]);
  }
  FoldStyledBlocks(blocks, out);
}

std::vector<AutoMapTipLine> AutoMapScene_PressBlock(const AutoMapDrawList &drawList,
                                                    const AutoMapPressSource &source, bool shift)
{
  // Built one space per depth as the cap knows it, then two spaces per depth
  PressLines lines;
  // The source itself goes inert once it fired, whatever it reaches
  if (source.links > 0 && source.onceOnlyLinks > 0)
  {
    lines.push_back(MakePressLine(source.onceOnlyLinks == source.links ? "ONCE ONLY"
                                                                       : "PARTLY ONCE ONLY",
                                  false, false));
    lines.back().style = AutoMapTipState;
  }
  if (shift && AutoMapScene_PressHasSteps(source))
  {
    lines.push_back(MakePressLine("ROUTE:", true, false));
    RouteContext route;
    route.drawList = &drawList;
    RouteLinks(source.effects, &route.links);
    RouteSteps(route, source.effects, 1, 0, false, true, &lines);
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
    std::vector<PressLines> results;
    for (size_t g = 0; g < goals.size(); g++)
    {
      PressLine result = MakePressLine(PressResultLine(drawList, *goals[g]), false, false);
      result.style = AutoMapTipResult;
      results.push_back(PressLines(1, result));
    }
    if (!results.empty())
    {
      lines.push_back(MakePressLine("RESULT:", true, false));
      FoldStyledBlocks(results, &lines);
    }
  }
  if (source.truncated)
  {
    lines.push_back(MakePressLine(" CHAIN CUT SHORT", false, false));
    lines.back().tail = true;
    lines.back().style = AutoMapTipHint;
  }
  std::vector<AutoMapTipLine> block;
  if (lines.empty()) return block;
  PressLines capped = CapStyledLines(lines, shift ? "" : " (SHIFT)", shift);
  for (size_t i = 0; i < capped.size(); i++)
  {
    const std::string &line = capped[i].text;
    size_t depth = line.find_first_not_of(' ');
    if (depth == std::string::npos) continue;
    block.push_back(AutoMapScene_TipLine(std::string(2 * depth, ' ') + line.substr(depth),
                                         capped[i].style));
  }
  return block;
}

// What a link's message does in the cell it reaches: the relays that take it, then the goal.
// signal is what the message carries (a held relay's SET or CLEAR).
static std::string HoverTarget(const AutoMapDrawList &drawList, const AutoMapLink &link,
                               AutoMapSignal signal)
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
    parts.push_back(gates + bit + SignalWord(signal));
  }
  if (!counters.empty()) parts.push_back(counters + (signal == AutoMapSignalSet ? " +1" : " -1"));
  if (!dsas.empty()) parts.push_back(dsas);
  bool reaches = PressReachesGoal(drawList, *dst, link);
  if (reaches || receivers.empty())
  {
    AutoMapEffect goal;
    goal.x = link.dstX;
    goal.y = link.dstY;
    goal.via = link;
    goal.via.signal = signal;
    goal.target = !reaches;
    parts.push_back(PressGoalTreeText(drawList, goal));
  }
  std::string text = PressLocation(drawList, link.dstX, link.dstY);
  for (size_t i = 0; i < parts.size(); i++) text += (i == 0 ? " " : ", ") + parts[i];
  return text;
}

// A relay line of the block at depth (one space per depth here; the block doubles it)
static PressLine RelayBlockLine(int depth, const std::string &text, AutoMapTipStyle style)
{
  PressLine line = MakePressLine(std::string((size_t)depth, ' ') + text, false, false);
  line.style = style;
  return line;
}

// One lettered relay: its header, its state lines, then what it sends, one line per link
// (two for a held relay, one per side); identical sends fold as 2X
static void RelayBlockOf(const AutoMapDrawList &drawList, int x, int y, const AutoMapRelay &relay,
                         const std::string &letter, PressLines *out)
{
  const char *kind = relay.kind == AutoMapRelayCounter ? "COUNTER"
                     : relay.kind == AutoMapRelayGate ? "GATE" : "DSA";
  PressLines children;
  if (relay.kind == AutoMapRelayCounter)
  {
    children.push_back(RelayBlockLine(1, "COUNT " + std::to_string(relay.value), AutoMapTipState));
  }
  else if (relay.kind == AutoMapRelayGate)
  {
    children.push_back(RelayBlockLine(1, "BITS " + GateBits(relay.value & 15) + ", NEEDS "
                                             + GateBits((relay.value >> 4) & 15),
                                      AutoMapTipState));
  }
  bool held = relay.kind != AutoMapRelayDSA && relay.signal == AutoMapSignalPressure;
  if (relay.onceOnly) children.push_back(RelayBlockLine(1, "ONCE ONLY", AutoMapTipState));
  if (held) children.push_back(RelayBlockLine(1, HeldReportsWords(relay), AutoMapTipState));
  if (relay.kind == AutoMapRelayGate)
  {
    std::string filter = AutoMapScene_GateFilterWords(relay.edgeMode);
    if (!filter.empty()) children.push_back(RelayBlockLine(1, filter.substr(2), AutoMapTipState));
    if (relay.normallyClosed)
    {
      children.push_back(RelayBlockLine(1, "NORMALLY CLOSED", AutoMapTipState));
    }
  }
  std::vector<std::vector<std::string> > sends;
  if (relay.localActionType >= 0)
  {
    std::string delay = DelayWord(relay.delay);
    sends.push_back(std::vector<std::string>(
        1, " -> " + std::string(PressLocalActionWord(relay.localActionType))
           + (delay.empty() ? "" : "," + delay)));
  }
  const std::vector<AutoMapLink> &links = drawList.links;
  for (size_t l = 0; relay.record != AUTOMAP_NO_RECORD && l < links.size(); l++)
  {
    const AutoMapLink &link = links[l];
    if (link.source != relay.record || link.srcX != x || link.srcY != y) continue;
    std::string delay = DelayWord(link.delay);
    if (!delay.empty()) delay = "," + delay;
    if (held && link.signal == AutoMapSignalPressure)
    {
      // What it sends while closed, then what it sends otherwise
      for (int set = 1; set >= 0; set--)
      {
        std::string target = HoverTarget(drawList, link,
                                          set ? AutoMapSignalSet : AutoMapSignalClear);
        if (target.empty()) continue;
        sends.push_back(std::vector<std::string>(
            1, " -> " + target + " " + HeldSideWords(relay, set != 0) + delay));
      }
      continue;
    }
    std::string target = HoverTarget(drawList, link, PressSourceSignal(link));
    if (target.empty()) continue;
    if (relay.kind == AutoMapRelayCounter) target += " AT 0";
    else if (relay.kind == AutoMapRelayGate) target += " " + GateConditionWords(relay);
    sends.push_back(std::vector<std::string>(1, " -> " + target + delay));
  }
  FoldPressBlocks(sends, &children);
  // A header with nothing under it counts as a line, so the cap never drops it alone
  PressLine header = MakePressLine(std::string(kind) + " " + letter, !children.empty(), false);
  header.style = AutoMapTipHeader;
  out->push_back(header);
  out->insert(out->end(), children.begin(), children.end());
}

// What a feeder does to one relay of the cell: A-1, A+1, A BIT 2 TOGGLE, A? for a DSA. A held
// relay feeding it sends SET or CLEAR by its own state: A+1/-1, A BIT 2 SET/CLEAR.
static std::string FeederDoing(const AutoMapDrawList &drawList, const AutoMapLink &link,
                               const AutoMapRelay &relay, const std::string &letter)
{
  std::string name = PressFeederName(drawList, link);
  bool heldRelay = link.signal == AutoMapSignalPressure
      && (name == "COUNTER" || name == "GATE" || name == "DSA");
  AutoMapSignal signal = PressSourceSignal(link);
  std::string doing = letter;
  if (relay.kind == AutoMapRelayCounter)
  {
    if (heldRelay) return doing + "+1/-1";
    return doing + (signal == AutoMapSignalSet ? "+1" : "-1");
  }
  if (relay.kind != AutoMapRelayGate) return doing + "?";
  if (link.targetPos >= 0 && link.targetPos <= 3) doing += " BIT " + std::to_string(link.targetPos);
  if (heldRelay) return doing + " SET/CLEAR";
  return doing + SignalWord(signal);
}

std::vector<AutoMapTipLine> AutoMapScene_RelayBlock(const AutoMapDrawList &drawList, int x, int y,
                                                    bool full)
{
  std::vector<AutoMapTipLine> block;
  if (drawList.spoilers != AutoMapSpoilersAll) return block;
  const AutoMapShownCell *shown = FindShownCell(drawList, x, y);
  if (shown == NULL) return block;
  std::vector<HoverReceiver> receivers;
  HoverReceivers(drawList, *shown, &receivers);
  if (receivers.empty()) return block;
  PressLines lines;
  for (size_t r = 0; r < receivers.size(); r++)
  {
    RelayBlockOf(drawList, x, y, *receivers[r].relay, HoverLetter(r), &lines);
  }
  // The sources that reach the cell, and what a press does to which letter: every relay
  // of the cell takes their message, so they are the cell's, after the relays
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(drawList, x, y);
  std::vector<std::string> feeders;
  std::vector<std::string> doings;
  for (size_t i = 0; i < incoming.size(); i++)
  {
    const AutoMapLink &link = incoming[i];
    std::string feeder = PressLocation(drawList, link.srcX, link.srcY) + " "
                         + PressFeederName(drawList, link);
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
      std::string doing = FeederDoing(drawList, link, *receivers[r].relay, HoverLetter(r));
      doings[found] += (doings[found].empty() ? "" : " ") + doing;
    }
  }
  std::vector<std::vector<std::string> > blocks;
  for (size_t f = 0; f < feeders.size(); f++)
  {
    blocks.push_back(std::vector<std::string>(1, " " + feeders[f] + " " + doings[f]));
  }
  if (!blocks.empty()) lines.push_back(MakePressLine("FED BY:", true, false));
  FoldPressBlocks(blocks, &lines);
  // The cap counts the lines under the headers, as the press block's does
  PressLines capped = CapStyledLines(lines, "", full);
  for (size_t i = 0; i < capped.size(); i++)
  {
    const std::string &line = capped[i].text;
    size_t depth = line.find_first_not_of(' ');
    if (depth == std::string::npos) continue;
    block.push_back(AutoMapScene_TipLine(std::string(2 * depth, ' ') + line.substr(depth),
                                         capped[i].style));
  }
  return block;
}

// Plural of a feeder kind: PAD -> PADS, SWITCH (E) -> SWITCHES (E), COUNTER -> COUNTERS
static std::string FeederPlural(const std::string &name)
{
  size_t word = name.find(' ');
  std::string head = name.substr(0, word), rest = word == std::string::npos ? "" : name.substr(word);
  if (head == "SWITCH") return "SWITCHES" + rest;
  if (head == "DSA") return name;
  return head + "S" + rest;
}

std::vector<AutoMapTipLine> AutoMapScene_ActedOnBy(const AutoMapDrawList &drawList, int x, int y,
                                                   bool openable)
{
  std::vector<AutoMapTipLine> lines;
  if (drawList.spoilers != AutoMapSpoilersAll) return lines;
  if (AutoMapScene_LinksFrom(drawList, x, y).empty()) return lines;
  if (!AutoMapScene_RelayBlock(drawList, x, y, false).empty()) return lines;
  std::vector<AutoMapLink> incoming = AutoMapScene_LinksTo(drawList, x, y);
  static const char *const OPENABLE[] = {"OPENED BY: ", "CLOSED BY: ", "TOGGLED BY: ", "HELD BY: "};
  static const char *const SIGNALLED[] = {"SET BY: ", "CLEARED BY: ", "TOGGLED BY: ", "HELD BY: "};
  static const AutoMapSignal ORDER[] = {AutoMapSignalSet, AutoMapSignalClear, AutoMapSignalToggle,
                                        AutoMapSignalPressure};
  for (int v = 0; v < 4; v++)
  {
    // Kinds in the order they first appear, each with its places
    std::vector<std::string> names;
    std::vector<std::vector<std::string> > places;
    for (size_t i = 0; i < incoming.size(); i++)
    {
      const AutoMapLink &link = incoming[i];
      if (link.signal != ORDER[v]) continue;
      std::string name = PressFeederName(drawList, link);
      char where[32];
      std::snprintf(where, sizeof(where), "(%d, %d)", link.srcX + drawList.offsetX,
                    link.srcY + drawList.offsetY);
      size_t n = 0;
      while (n < names.size() && names[n] != name) n++;
      if (n == names.size())
      {
        names.push_back(name);
        places.push_back(std::vector<std::string>());
      }
      if (std::find(places[n].begin(), places[n].end(), where) == places[n].end())
      {
        places[n].push_back(where);
      }
    }
    if (names.empty()) continue;
    std::string text = openable ? OPENABLE[v] : SIGNALLED[v];
    for (size_t n = 0; n < names.size(); n++)
    {
      char level[16];
      std::snprintf(level, sizeof(level), " L%d ", drawList.level);
      text += (n == 0 ? "" : ", ") + (places[n].size() > 1 ? FeederPlural(names[n]) : names[n]) + level;
      for (size_t p = 0; p < places[n].size(); p++) text += (p == 0 ? "" : ", ") + places[n][p];
    }
    lines.push_back(AutoMapScene_TipLine(text, AutoMapTipFeeder));
  }
  return lines;
}
