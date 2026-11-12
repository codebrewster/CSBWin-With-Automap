#include "Thrown.h"

#include "HeroAttack.h"

namespace calc {

namespace {

int16_t sw(int v) { return int16_t(v); }

// D5 of ThrowByCharacter: a weapon of class <= 12 adds its distance, anything else 1
int distanceTerm(const WeaponType &w) { return w.cls <= 12 ? w.distance : 1; }

// LaunchMissile: Smaller(ub(range), 255).  The & 0xff is deliberate: in release ub() is a plain
// (ui8) cast (CSBTypes.h:163), so a range above 255 wraps rather than stopping at 255.
int keepRange(int range) { return std::min(range & 0xff, 255); }

}  // namespace

Launch handLaunch(const Hero &h, const WeaponType &w, Dice &dice)
{
  Launch l;
  const int M = h.mastery[SkillThrow];
  const int ml = eng::maxLoad(h, h.stamina);
  for (int r = 0; r < 16; r++) {   // the STRandom(16) inside DetermineThrowingDistance
    int d7 = sw(eng::throwingDistance(h, w, 1, ml, h.stamina, r) + distanceTerm(w));
    const auto &t = dice.roll(16);
    for (int k = 0; k < 16; k++)   // d7 + STRandom(16) + d7/2 + M
      l.range.add(keepRange(sw(d7 + k + d7 / 2 + M)), t[size_t(r)] * t[size_t(k)]);
  }
  for (int low = 0; low < 32; low++)   // STRandom() & 31
    l.damage.add(std::max(40, std::min(8 * M + low, 200)), 1.0 / 32);
  l.decay = std::max(5, 11 - M);
  return l;
}

SampledLaunch sampleHandLaunch(Draw &d, const Hero &h, const WeaponType &w)
{
  SampledLaunch l;
  const int M = h.mastery[SkillThrow];
  const int ml = eng::maxLoad(h, h.stamina);
  int d7 = sw(eng::throwingDistance(h, w, 1, ml, h.stamina, d.rnd(16)) + distanceTerm(w));
  d7 = sw(d7 + d.rnd(16) + d7 / 2 + M);
  l.range = keepRange(d7);
  l.damage = std::max(40, std::min(8 * M + d.low(31), 200));
  l.decay = std::max(5, 11 - M);
  return l;
}

Dist arrivalStrength(const WeaponType &w, const MissileState &at, Dice &dice)
{
  return finishMagic(Dist::point(sw(w.distance + sw(w.weight) / 2)), at, dice);
}

int sampleArrivalStrength(Draw &d, const WeaponType &w, const MissileState &at)
{
  return sampleFinishMagic(d, sw(w.distance + sw(w.weight) / 2), at);
}

Dist damageOnMonster(const Launch &l, const WeaponType &w, const Target &t, int squares,
                     bool backRow, int shotFactor, Dice &dice)
{
  Dist out;
  const MonsterType &m = t.type;
  if (m.invincible || m.nonMaterial) return out;
  const int defense = m.defense == 0 ? 1 : m.defense;
  const int steps = flightSteps(squares, backRow);
  for (size_t i = 0; i < l.range.p.size(); i++) {
    if (l.range.p[i] == 0) continue;
    for (size_t j = 0; j < l.damage.p.size(); j++) {
      if (l.damage.p[j] == 0) continue;
      MissileState at = flyMissile(l.range.lo + int(i), l.damage.lo + int(j), l.decay, steps);
      if (!at.arrives) continue;
      Dist s = arrivalStrength(w, at, dice);
      double pw = l.range.p[i] * l.damage.p[j];
      for (size_t k = 0; k < s.p.size(); k++) {
        if (s.p[k] == 0) continue;
        int str = sw((s.lo + int(k)) * shotFactor);
        int hit = sw(((64 * (str & 0xffff)) & 0xffff) / defense);
        if (hit > 0) out.add(hit, pw * s.p[k]);
      }
    }
  }
  return out;
}

int sampleDamageOnMonster(Draw &d, SampledLaunch l, const WeaponType &w, const Target &t,
                          int squares, bool backRow, int shotFactor)
{
  const MonsterType &m = t.type;
  if (m.invincible || m.nonMaterial) return 0;
  const int defense = m.defense == 0 ? 1 : m.defense;
  MissileState at = flyMissile(l.range, l.damage, l.decay, flightSteps(squares, backRow));
  if (!at.arrives) return 0;
  int str = sw(sampleArrivalStrength(d, w, at) * shotFactor);
  return std::max(0, int(sw(((64 * (str & 0xffff)) & 0xffff) / defense)));
}

bool canShoot(const WeaponType &launcher, const WeaponType &ammo)
{
  return (launcher.cls >= 16 && launcher.cls <= 31 && ammo.cls == 10) ||
         (launcher.cls >= 32 && launcher.cls <= 47 && ammo.cls == 11);
}

// atk_SHOOT, Attack.cpp:1933-2005.  The strength is the launcher's class minus 16 (or 32), used for
// damage AND decay: Attack.cpp:1986 sets damage = 2 * (attribute + Shoot mastery), but line 1987
// overwrites it with D4W (= cls - 16/32) before LaunchObject(range, damage, damage), so the hero's
// Shoot skill does not count.  The original sources checked were 9.96.3, 11.0 and the 2023 archive,
// not 17.6: the same two lines, in the same order, stand in CSBwin 9.96.3 (CSBwin9963src.zip,
// Attack.cpp:1882-1883), 11.0 (CSBwin11_0src.zip, Attack.cpp:1896-1897) and SRC_20230608 (Attack.cpp:1904-1905), downloaded from dianneandpaul.net.
// So the port agrees with the original and there is no rule for it.  (The repo's own history only
// has the line from its first commit f3af94d, so it says nothing.  Hero and rules are not read.)
Launch shotLaunch(const WeaponType &launcher, const WeaponType &ammo, const Hero &, const Rules &)
{
  Launch l;
  const int power = launcher.cls - (launcher.cls >= 32 ? 32 : 16);
  // LaunchMissile wraps a range above 255 for shots too, as for throws.
  l.range = Dist::point(keepRange(launcher.distance + ammo.distance));
  l.damage = Dist::point(power);
  l.decay = power;
  return l;
}

SampledLaunch sampleShotLaunch(Draw &, const WeaponType &launcher, const WeaponType &ammo,
                               const Hero &, const Rules &)
{
  SampledLaunch l;
  l.decay = l.damage = launcher.cls - (launcher.cls >= 32 ? 32 : 16);
  l.range = keepRange(launcher.distance + ammo.distance);
  return l;
}

// TAG016382, Mouse.cpp:678-693:
//   D7 = weight / 2;  D6 = ApplyLimits(1, D7, 10);  while ((D7 -= 10) > 0) D6 += D7 / 2;
// The legacyThrowStamina branch (D6 = D7 / 2, before 11.024) is only for old playfiles.
int throwStamina(int weight)
{
  int d7 = sw(weight / 2);
  int d6 = std::max(1, std::min(int(d7), 10));
  while ((d7 = sw(d7 - 10)) > 0) d6 = sw(d6 + d7 / 2);
  return d6;
}

namespace {

// The skill level the object's action set asks for `action`; -1 = the set does not offer it
int levelFor(const GameData &g, const WeaponType &w, int action)
{
  if (w.actionSet < 0 || w.actionSet >= int(g.actionSets.size())) return -1;
  const ActionSet &set = g.actionSets[size_t(w.actionSet)];
  for (int i = 0; i < 3; i++)
    if (set.action[i] == action) return set.minMastery[i];
  return -1;
}

// AdjustSkills, Magic.cpp:244-273, for a skill of 4..11 (Throw 10, Shoot 11): halved when no monster
// attacked the party for 150 ticks, times the level's multiplier when it is not 0, doubled when a
// monster attacked within 25 ticks (d.LastMonsterAttackTime, set by Monster.cpp:992).  As heroAttack
// does, the party is taken to be in a fight: doubled, not halved.
double scaledExp(int experience, int level)
{
  if (level != 0) experience *= level;
  return experience * 2.0;
}

double arriveChance(const Launch &l, int steps)
{
  double p = 0;
  for (size_t i = 0; i < l.range.p.size(); i++) {
    if (l.range.p[i] == 0) continue;
    for (size_t j = 0; j < l.damage.p.size(); j++)
      if (l.damage.p[j] != 0 && flyMissile(l.range.lo + int(i), l.damage.lo + int(j), l.decay, steps).arrives)
        p += l.range.p[i] * l.damage.p[j];
  }
  return p;
}

void addNote(std::string &note, const std::string &line)
{
  if (!note.empty()) note += "; ";
  note += line;
}

}  // namespace

// What one throw or shot costs and gives.
//
// Hand throw.  With the THROW action (id 42) of the object's action set, _Attack reads the action's
// delay, stamina and experience (Attack.cpp:1790, 1797, 1806), then the atk_THROW case sets
// pParam->disableTime = -1 (Attack.cpp:2188) and calls ThrowByCharacter, which takes
// TAG016382(object) stamina (Mouse.cpp:748) and disables the hero for 4 ticks because disableTime < 0
// (Mouse.cpp:749).  Back in _Attack the throw sets pParam->disableTime = 0 (Attack.cpp:2233), so the
// DisableCharacterAction after the switch (Attack.cpp:2245) does not run: the action's own ticks
// (0 in Dungeon Master's table) never count, the delay is 4.  The action's energy + STRandomBool()
// is still taken (Attack.cpp:2249) and its experience given to its skill (Attack.cpp:2254), on top
// of ThrowByCharacter's own AdjustSkills(Throw, 8, + 4 + distance/4 for a weapon of class <= 12)
// (Mouse.cpp:750-764).  An object whose action set has no THROW can only be thrown from the cursor
// (ThrowObjectInHand, Mouse.cpp:798): ThrowByCharacter alone, with the same 4 ticks.
//
// Shot.  atk_SHOOT (Attack.cpp:1933) leaves disableTime, staminaCost and experienceGained as _Attack
// set them from the SHOOT action (id 32): its ticks, its energy + STRandomBool() and its experience
// (9; DM 1.1's 20 under the shootExperience20 rule, Attack.cpp:1807-1810, which the calculator has
// not got).  LaunchObject (Magic.cpp:481) takes no stamina, so there is no weight term.
//
// The clicks: swingPeriod adds 2 ticks of clicking that were measured for swings only.
ThrowResult throwAttack(const GameData &g, const Hero &hero, const WeaponType &object,
                        const WeaponType *launcher, const Target &target, const Rules &rules,
                        int squares, bool backRow)
{
  ThrowResult r;
  r.object = object.id;
  r.shot = launcher != nullptr;
  if (launcher) r.launcher = launcher->id;
  if (object.id < 0) {
    r.note = "Nothing to throw";
    return r;
  }
  if (launcher && !canShoot(*launcher, object)) {
    r.note = "Need ammo";   // Attack.cpp:1948-1980: d.attackDamageToDisplay = -2
    return r;
  }
  Dice dice(rules.moduloBias);
  Launch l;
  int delay = 4;
  double exp = 0;
  if (launcher) {
    const ActionType &a = g.actions.at(32);
    l = shotLaunch(*launcher, object, hero, rules);
    r.damage = damageOnMonster(l, object, target, squares, backRow, rules.shotDamageFactor, dice);
    int need = levelFor(g, *launcher, a.id);
    r.usable = need < 0 || hero.mastery[a.skill] >= need;
    delay = a.ticks;
    r.stamina = a.energy + 0.5;
    exp = scaledExp(a.exp, target.level);
  } else {
    l = handLaunch(hero, object, dice);
    r.damage = damageOnMonster(l, object, target, squares, backRow, 1, dice);
    // ThrowByCharacter's experience, Mouse.cpp:750-764 (the object is a weapon here)
    int d6 = 8 + 4;
    if (object.cls <= 12) d6 = sw(d6 + object.distance / 4);
    exp = scaledExp(d6, target.level);
    r.stamina = throwStamina(object.weight);
    int need = levelFor(g, object, 42);
    if (need >= 0) {
      const ActionType &a = g.actions.at(42);
      r.usable = hero.mastery[a.skill] >= need;
      r.stamina += a.energy + 0.5;
      exp += scaledExp(a.exp, target.level);
    } else {
      addNote(r.note, "thrown from the cursor: no Throw action");
    }
  }
  r.valid = true;
  r.arriveChance = arriveChance(l, flightSteps(squares, backRow));
  if (r.arriveChance == 0) addNote(r.note, "the missile fades before it gets there");
  else if (target.type.nonMaterial) addNote(r.note, "passes through a non-material monster");
  addNote(r.note, "unverified: the 2 ticks of clicking were measured for swings, not throws");
  Summary s = summarize(r.damage);
  r.perThrow = r.damage.sum();
  r.perHit = s.mean;
  r.minHit = s.min;
  r.maxHit = s.max;
  r.ticks = swingPeriod(delay);
  r.dps = r.ticks > 0 ? ticksPerSecond() * r.perThrow / r.ticks : 0;
  r.expPerThrow = exp;
  r.expPerSecond = r.ticks > 0 ? ticksPerSecond() * exp / r.ticks : 0;
  if (target.hp > 0) {
    r.killChance = r.damage.atLeast(target.hp);
    r.throwsToKill = meanTriesToReach(r.damage, target.hp);
    r.secondsToKill = r.throwsToKill * r.ticks / ticksPerSecond();
  }
  return r;
}

}  // namespace calc
