#include "MonsterAttack.h"

#include <map>

#include "HeroAttack.h"

namespace calc {

double monsterAttackInterval(const MonsterType &m)
{
  // attack timer: attackTicks + STRandom0_3 - 1, long ones + (STRandom & 7) - 2;
  // then the pause after the blow: (word20 & 15) + STRandomBool
  double t = m.attackTicks + 0.5;
  if (m.attackTicks > 15) t += 1.5;
  t += m.attackPause + 0.5;
  return t < 1 ? 1 : t;
}

namespace {

// Which body parts the blow is aimed at: mask -> chance
std::map<int, double> aimTable(const MonsterType &m, const GameData &g)
{
  std::map<int, double> aim;
  aim[0] += 1.0 / 16;   // (D6 & 0x70) == 0: mask = D6 & 1
  aim[1] += 1.0 / 16;
  for (int n = 0; n < 16; n++) {
    int d5 = m.woundNibbles, d4 = 0;
    while (n > (d5 & 15) && d4 < 3) {
      d4++;
      d5 >>= 4;
    }
    aim[g.woundSlots[d4]] += 7.0 / 8 / 16;
  }
  return aim;
}

}  // namespace

MonsterAttackResult monsterAttack(const Hero &h, const Party &party, const Target &t,
                                  const GameData &g, const Rules &rules, double interval)
{
  MonsterAttackResult r;
  const MonsterType &m = t.type;
  Dice dice(rules.moduloBias);
  const int w2 = 2 * t.level;
  // ---- the hero's chance to get out of the way
  const int ml = eng::maxLoad(h, h.stamina);
  const auto &w8 = dice.roll(8);
  const auto &w32 = dice.roll(32);
  double slower = 0;   // P(quickness < needed)
  for (int a = 0; a < 8; a++)
    for (int b = 0; b < 8; b++)
      for (int c = 0; c < 8; c++) {
        int q = eng::quickness(h, ml, a, b, c);
        double pq = w8[size_t(a)] * w8[size_t(b)] * w8[size_t(c)];
        for (int n = 0; n < 32; n++)
          if (q < n + m.dexterity + w2 - 16) slower += pq * w32[size_t(n)];
      }
  double notDodged = slower + (1.0 - slower) * 0.25;
  // IsCharacterLucky(60)
  const auto &w100 = dice.roll(100);
  double first = 0;
  for (int v = 61; v < 100; v++) first += w100[size_t(v)];
  first *= 0.5;
  double second = 0;
  if (h.luck() > 0) {
    const auto &wl = dice.roll(h.luck());
    for (int v = 61; v < h.luck(); v++) second += wl[size_t(v)];
  }
  double lucky = first + (1.0 - first) * second;
  r.reachChance = notDodged * (1.0 - lucky);
  // ---- the blow
  Dist d7 = dice.dist(16, w2 + m.attack - 2 * h.mastery[SkillParry]);
  Dist start;
  for (size_t i = 0; i < d7.p.size(); i++) {
    int v = d7.lo + int(i);
    if (v > 1) {
      start.add(v, d7.p[i]);
    } else {   // a coin decides between nothing and a scratch
      for (int k = 0; k < 4; k++) start.add(k + 2, d7.p[i] * 0.5 * 0.25);
    }
  }
  Dist cur = mapValues(start, [](int v) { return v / 2; });
  cur = dice.plusRollOf(cur, [](int v) { return v; });   // + STRandom(D7)
  cur = dice.plusRoll(cur, 4);                           // + STRandom0_3
  cur = dice.plusRollOf(cur, [](int v) { return v; });
  cur = mapValues(cur, [](int v) { return v / 4; });
  cur = convolve(cur, dice.dist(4, 1));
  Dist raw;
  raw.add(cur, 0.5);
  raw.add(bind(cur, [&](int v, double w, Dist &out) {
            int n = v / 2 + 1;
            const auto &tt = dice.roll(n);
            for (int k = 0; k < n; k++) out.add(v - (k - 1), w * tt[size_t(k)]);
          }),
          0.5);
  r.rawDamage = raw.scaled(r.reachChance);
  // ---- armour, by where it lands
  const Hero bareHero = withoutArmour(h);
  Dist bare;
  double protSum = 0, protMass = 0;
  for (auto &kv : aimTable(m, g)) {
    int mask = kv.first;
    double pa = kv.second * r.reachChance;
    HarmResult hr = damageHero(raw, mask, m.attackKind, h, party, g, dice);
    r.damage.add(hr.damage, pa);
    r.woundChance += hr.woundChance * pa;
    for (int s = 0; s < 6; s++) {
      r.woundSlot[s] += hr.woundSlot[s] * pa;
      if (mask & (1 << s)) r.slotChance[s] += kv.second;
    }
    if (mask == 0) r.noSlotChance += kv.second;
    protSum += hr.protection.sum() * kv.second;
    protMass += hr.protection.mass() * kv.second;
    bare.add(damageHero(raw, mask, m.attackKind, bareHero, party, g, dice).damage, pa);
  }
  r.meanProtection = protMass > 0 ? protSum / protMass : 0;
  r.stats = summarize(r.damage);
  r.bare = summarize(bare);
  r.hitChance = r.stats.chance;
  // ---- poison: half of the blows that draw blood
  if (m.poison != 0) {
    r.poisonChance = r.hitChance * 0.5;
    r.poison = poisonCourse(scaleByAttribute(h.attr[Vitality], m.poison));
  }
  // ---- over time
  r.interval = interval > 0 ? interval : monsterAttackInterval(m);
  r.dps = r.stats.perTry * ticksPerSecond() / r.interval;
  r.attackers = std::max(1, t.count);
  r.groupDps = r.dps * r.attackers;
  if (h.hp > 0) {
    r.hpShare = r.stats.mean / h.hp;
    r.attacksToKill = meanTriesToReach(r.damage, h.hp);
    r.secondsToKill = r.attacksToKill * r.interval / ticksPerSecond() / r.attackers;
    if (r.hitChance > 0) {
      int cap = int(std::min(20000.0, r.attacksToKill * 6 + 50));
      std::vector<double> curve = killCurve(r.damage, h.hp, cap);
      auto when = [&](double q) {
        for (size_t n = 0; n < curve.size(); n++)
          if (curve[n] >= q) return int(n);
        return cap;
      };
      r.attacksToKillP05 = when(0.05);
      r.attacksToKillMedian = when(0.5);
      r.attacksToKillP95 = when(0.95);
    }
  }
  return r;
}

int sampleMonsterAttack(Draw &d, const Hero &h, const Party &party, const Target &t,
                        const GameData &g, const Rules &rules, int &luck, bool *poisoned,
                        int *wounds)
{
  const MonsterType &m = t.type;
  const int w2 = 2 * t.level;
  {
    const int ml = eng::maxLoad(h, h.stamina);
    int qa = d.rnd(8), qb = d.rnd(8), qc = d.rnd(8);
    int q = eng::quickness(h, ml, qa, qb, qc);
    int need = d.rnd(32) + m.dexterity + w2 - 16;
    if (q >= need && d.r03() != 0) return 0;
    // IsCharacterLucky(60)
    bool ok;
    if (d.bit() && d.rnd(100) > 60) {
      ok = true;
    } else {
      int roll = luck == 0 ? 0 : d.rnd(luck);
      ok = roll > 60;
      if (rules.luckDrift) {
        int v = luck + (ok ? -2 : 2);
        luck = v <= h.attrMin[Luck] ? h.attrMin[Luck] : (v > h.attrMax[Luck] ? h.attrMax[Luck] : v);
      }
    }
    if (ok) return 0;
  }
  int d6 = int(d.r.raw());
  int mask;
  if (d6 & 0x70) {
    d6 &= 15;
    int d5 = m.woundNibbles, d4 = 0;
    for (; d6 > (d5 & 15) && d4 < 3; d4++) d5 >>= 4;
    mask = g.woundSlots[d4];
  } else {
    mask = d6 & 1;
  }
  int d7 = w2 + d.rnd(16) + m.attack - 2 * h.mastery[SkillParry];
  if (d7 <= 1) {
    if (d.bit() != 0) return 0;
    d7 = d.r03() + 2;
  }
  d7 /= 2;
  int d0 = d.rnd(d7);
  int d1 = d.r03();
  d7 = d7 + d0 + d1;
  d7 = d7 + d.rnd(d7);
  d7 /= 4;
  d7 = d7 + d.r03() + 1;
  if (d.bit() != 0) d7 = d7 - (d.rnd(d7 / 2 + 1) - 1);
  d7 = sampleDamageHero(d, d7, mask, m.attackKind, h, party, g, wounds);
  if (d7 != 0 && m.poison != 0 && d.bit() != 0 && poisoned) *poisoned = true;
  return d7;
}

}  // namespace calc
