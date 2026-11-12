#include "Thrown.h"

#include "HeroAttack.h"

namespace calc {

namespace {

int16_t sw(int v) { return int16_t(v); }

// D5 of ThrowByCharacter: a weapon of class <= 12 adds its distance, anything else 1
int distanceTerm(const WeaponType &w) { return w.cls <= 12 ? w.distance : 1; }

// LaunchMissile: Smaller(ub(range), 255)
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

}  // namespace calc
