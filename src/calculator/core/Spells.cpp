#include "Spells.h"

namespace calc {

const std::vector<int> &damageMissiles()
{
  static const std::vector<int> v = {MissileFireball, MissileLightning, MissilePoisonBolt,
                                     MissilePoisonCloud, MissileDispell};
  return v;
}

const char *missileName(int missile)
{
  switch (missile) {
    case MissileFireball: return "Fireball";
    case MissilePoison: return "Poison spit";
    case MissileLightning: return "Lightning Bolt";
    case MissileDispell: return "Harm Non-Material";
    case MissileZo: return "Open Door";
    case MissilePoisonBolt: return "Poison Bolt";
    case MissilePoisonCloud: return "Poison Cloud";
  }
  return "?";
}

int spellForMissile(const GameData &data, int missile)
{
  for (const SpellType &s : data.spells)
    if (s.kind == SpellMissile && s.missile == missile) return s.id;
  return -1;
}

MissileState flyMissile(int range, int damage, int decay, int steps)
{
  MissileState m;
  m.range = range;
  m.damage = damage;
  for (int i = 0; i < steps; i++) {   // Timer.cpp:286
    if (m.range <= decay) {
      m.arrives = false;
      m.range = 0;
      return m;
    }
    m.range -= decay;
    m.damage = m.damage < decay ? 0 : m.damage - decay;
  }
  return m;
}

int flightSteps(int squares, bool backRow)
{
  // The first move leaves the caster's square without fading; from then on
  // every half square fades once, and the encounter is checked before it.
  int steps = 2 * (std::max(1, squares) - 1);
  return steps + (backRow ? 1 : 0);
}

Cast castSpell(const GameData &g, const Hero &h, int missile, int power)
{
  Cast c;
  c.power = power;
  c.spell = spellForMissile(g, missile);
  if (c.spell < 0 || power < 1 || power > 6) return c;
  c.known = true;
  const SpellType &s = g.spells[size_t(c.spell)];
  // TAG01b990: the power rune as it stands, every other rune times the power's factor
  c.mana = g.runeCost[0][power - 1];
  for (int r = 0; r < 3; r++)
    if (s.runes[r]) c.mana += (g.runeCost[r + 1][s.runes[r] - 1] * g.powerFactor[power]) >> 3;
  c.levelNeeded = s.difficulty + power;
  c.level = s.skill >= 0 && s.skill < SkillCount ? h.mastery[s.skill] : 0;
  if (c.level < c.levelNeeded) {
    // once per missing level: (STRandom & 127) > min(Wisdom + 15, 115) fails
    int limit = std::min(h.attr[Wisdom] + 15, 115);
    double ok = std::min(1.0, (limit + 1) / 128.0);
    c.success = std::pow(ok, c.levelNeeded - c.level);
  }
  int level = missile == MissileZo ? 2 * c.level : c.level;
  int range = (2 * level + 4) * (power + 2);
  c.range = range <= 21 ? 21 : (range > 255 ? 255 : range);
  c.decay = 10 - std::min(8, h.maxMana / 8);
  if (c.range < 4 * c.decay) {
    c.range += 3;
    c.decay -= 1;
  }
  c.damage = 90;
  return c;
}

namespace {

int16_t sw(int v) { return int16_t(v); }

// The tail of DetermineMagicDamage, from the base roll on
Dist finishMagic(const Dist &base, const MissileState &at, Dice &dice)
{
  Dist d6 = mapValues(base, [&](int v) { return sw((v + at.range) / 16 + 1); });
  d6 = dice.plusRollOf(d6, [](int v) { return v / 2 + 1; });
  d6 = dice.plusRoll(d6, 4);
  int d1 = 32 - at.damage / 8;
  return mapValues(d6, [&](int v) { return std::max(v / 2, v - d1); });
}

int sampleFinishMagic(Draw &d, int d6, const MissileState &at)
{
  d6 = sw((d6 + at.range) / 16 + 1);
  int d0 = d.rnd(d6 / 2 + 1);
  int d1 = d.r03();
  d6 = sw(d6 + d1 + d0);
  d1 = 32 - at.damage / 8;
  return std::max(d6 / 2, d6 - d1);
}

Dist fireBase(Dice &dice, int factor)
{
  Dist b = convolve(dice.dist(16), dice.dist(16, 10));
  return factor == 1 ? b : mapValues(b, [&](int v) { return sw(factor * v); });
}

// DamageMonsterGroup: what each monster of the group gets out of `damage`
Dist groupShare(const Dist &damage, Dice &dice)
{
  return bind(damage, [&](int v, double w, Dist &out) {
    if (v <= 0) return;
    int d6 = v / 8 + 1;
    int d7 = v - d6;
    d6 *= 2;
    const auto &t = dice.roll(d6);
    for (int r = 0; r < d6; r++) out.add(d7 + r, w * t[size_t(r)]);
  });
}
int sampleGroupShare(Draw &d, int damage)
{
  if (damage <= 0) return 0;
  int d6 = damage / 8 + 1;
  int d7 = damage - d6;
  return d7 + d.rnd(2 * d6);
}

// CreateCloud: the blast of a fireball or a lightning bolt of this size
Dist blast(int missile, int size, Dice &dice)
{
  int e = sw(size / 2 + 1);
  Dist d = dice.dist(e, e);
  if (missile != MissileFireball) d = mapValues(d, [](int v) { return v / 2; });
  return d;
}
int sampleBlast(Draw &d, int missile, int size)
{
  int e = sw(size / 2 + 1);
  e = sw(e + d.rnd(e));
  if (missile != MissileFireball) e /= 2;
  return e;
}

// The size of the cloud a missile leaves (Code11f52.cpp:2201)
int cloudSize(int missile, int range) { return missile == MissileLightning ? range / 2 : range; }

// ProcessTT_25: strength of a poison cloud of this value, for one tick
Dist poisonTick(int value, Dice &)
{
  Dist d;
  int base = std::min(value / 32, 4);
  d.add(std::max(1, base), 0.5);
  d.add(std::max(1, base + 1), 0.5);
  return d;
}

// DeterminePoisonDamage
Dist poisonOnMonster(const Dist &strength, const MonsterType &m, Dice &)
{
  Dist out;
  if (m.poisonResistance == 15) return out;
  for (size_t i = 0; i < strength.p.size(); i++) {
    int v = strength.lo + int(i);
    if (strength.p[i] == 0 || v == 0) continue;
    for (int r = 0; r < 4; r++)
      out.add(sw((8 * (v + r)) / (m.poisonResistance + 1)), strength.p[i] / 4);
  }
  return out;
}
int samplePoisonOnMonster(Draw &d, int strength, const MonsterType &m)
{
  if (strength == 0 || m.poisonResistance == 15) return 0;
  return sw((8 * (strength + d.r03())) / (m.poisonResistance + 1));
}

Dist positive(const Dist &d)
{
  Dist out;
  for (size_t i = 0; i < d.p.size(); i++)
    if (d.lo + int(i) > 0) out.add(d.lo + int(i), d.p[i]);
  return out;
}

// The sum of two parts where a missing part counts as 0
Dist sumWithZero(Dist a, Dist b)
{
  a = positive(a);
  b = positive(b);
  a.add(0, std::max(0.0, 1.0 - a.mass()));
  b.add(0, std::max(0.0, 1.0 - b.mass()));
  return positive(convolve(a, b));
}

// A cloud that shrinks by 3 a tick until it is below 6
template <class F>
void eachCloudTick(int value, F f)
{
  for (int v = value;; v -= 3) {
    f(v);
    if (v < 6) break;
  }
}

}  // namespace

Dist magicDamage(int missile, const MissileState &at, Dice &dice)
{
  switch (missile) {
    case MissileFireball: return finishMagic(fireBase(dice, 1), at, dice);
    case MissileLightning: return finishMagic(fireBase(dice, 5), at, dice);
    case MissilePoison: return finishMagic(convolve(dice.dist(16), dice.dist(32)), at, dice);
    case MissilePoisonBolt: return Dist::point(1);
    default: return Dist::point(0);   // the other spells strike with nothing of their own
  }
}

int sampleMagicDamage(Draw &d, int missile, const MissileState &at, int *poison)
{
  if (poison) *poison = 0;
  int d6;
  switch (missile) {
    case MissilePoison:
      d6 = d.low(15);
      if (poison) *poison = d6 + 10;
      d6 = sw(d6 + d.rnd(32));
      break;
    case MissilePoisonBolt:
      if (poison) *poison = at.range;
      return 1;
    case MissileFireball:
    case MissileLightning: {
      int a = d.rnd(16), b = d.rnd(16);
      d6 = sw(a + b + 10);
      if (missile == MissileLightning) d6 = sw(5 * d6);
      break;
    }
    default: return 0;
  }
  return sampleFinishMagic(d, d6, at);
}

MonsterHit missileOnMonster(int missile, const MissileState &at, const Target &t, Dice &dice)
{
  MonsterHit r;
  const MonsterType &m = t.type;
  r.arrives = at.arrives;
  if (!at.arrives) {
    r.note = "fades before it gets there";
    return r;
  }
  if (m.invincible) {
    r.noEffect = true;
    r.note = "cannot be harmed";
    return r;
  }
  const int defense = m.defense == 0 ? 1 : m.defense;
  auto direct = [&](const Dist &magic) {   // 64 * damage / defence
    return mapValues(magic, [&](int v) { return sw(((64 * (v & 0xffff)) & 0xffff) / defense); });
  };
  switch (missile) {
    case MissileFireball:
    case MissileLightning: {
      if (m.nonMaterial) {
        r.noEffect = true;
        r.note = "passes through non-material beings";
        return r;
      }
      Dist hit = positive(direct(magicDamage(missile, at, dice)));
      Dist cloud;
      int size = cloudSize(missile, at.range);
      if (size > 0 && m.fireResistance != 15) {
        Dist e = blast(missile, size, dice);
        e = bind(e, [&](int v, double w, Dist &out) {
          if (v == 0) return;
          int n = 2 * m.fireResistance + 1;
          const auto &tt = dice.roll(n);
          for (int k = 0; k < n; k++)
            if (v - k > 0) out.add(v - k, w * tt[size_t(k)]);
        });
        cloud = groupShare(e, dice);
      } else if (m.fireResistance == 15) {
        r.note = "immune to the blast; only the missile itself counts";
      }
      r.others = positive(cloud);
      r.struck = sumWithZero(hit, cloud);
      break;
    }
    case MissilePoisonBolt: {
      if (m.nonMaterial) {
        r.noEffect = true;
        r.note = "passes through non-material beings";
        return r;
      }
      int hit = sw(64 / defense);
      if (hit == 0) {   // "if (D5W == 0) break": the poison is never reached
        r.noEffect = true;
        r.note = "defence above 64: the bolt does nothing";
        return r;
      }
      Dist poison = poisonOnMonster(Dist::point(at.range), m, dice);
      if (m.poisonResistance == 15) r.note = "immune to poison; only the bolt itself counts";
      r.struck = sumWithZero(Dist::point(hit), poison);
      break;
    }
    case MissilePoisonCloud: {
      if (m.poisonResistance == 15) {
        r.noEffect = true;
        r.note = "immune to poison";
        return r;
      }
      Dist total = Dist::point(0);
      bool first = true;
      eachCloudTick(at.range, [&](int v) {
        Dist tick = groupShare(poisonOnMonster(poisonTick(v, dice), m, dice), dice);
        tick.add(0, std::max(0.0, 1.0 - tick.mass()));
        if (first) r.perTick = positive(tick);
        first = false;
        total = convolve(total, tick);
        r.ticks++;
      });
      r.struck = positive(total);
      r.others = r.struck;
      r.note = "total if the monster stays in the cloud for all of its " +
               std::to_string(r.ticks) + " ticks";
      break;
    }
    case MissileDispell: {
      if (!m.nonMaterial) {
        r.noEffect = true;
        r.note = "harms only non-material beings";
        return r;
      }
      int d5 = sw(at.range / 2 + 1);
      Dist cloud = dice.dist(d5, d5 + 1);
      r.struck = positive(groupShare(cloud, dice));
      r.others = r.struck;
      break;
    }
    default:
      r.noEffect = true;
      return r;
  }
  r.stats = summarize(r.struck);
  if (t.hp > 0) r.killChance = r.struck.atLeast(t.hp);
  return r;
}

int sampleMissileOnMonster(Draw &d, int missile, const MissileState &at, const Target &t,
                           bool struck)
{
  const MonsterType &m = t.type;
  if (!at.arrives || m.invincible) return 0;
  const int defense = m.defense == 0 ? 1 : m.defense;
  switch (missile) {
    case MissileFireball:
    case MissileLightning: {
      if (m.nonMaterial) return 0;
      int total = 0;
      if (struck) total += sw(((64 * (sampleMagicDamage(d, missile, at) & 0xffff)) & 0xffff) / defense);
      int size = cloudSize(missile, at.range);
      if (size > 0) {
        int e = sampleBlast(d, missile, size);
        if (e != 0 && m.fireResistance != 15) {
          e = e - d.rnd(2 * m.fireResistance + 1);
          if (e > 0) total += sampleGroupShare(d, e);
        }
      }
      return total;
    }
    case MissilePoisonBolt: {
      if (m.nonMaterial) return 0;
      int hit = sw(64 / defense);
      if (hit == 0) return 0;
      return hit + samplePoisonOnMonster(d, at.range, m);
    }
    case MissilePoisonCloud: {
      int total = 0;
      eachCloudTick(at.range, [&](int v) {
        int d5 = std::max(1, std::min(v / 32, 4) + d.bit());
        total += sampleGroupShare(d, samplePoisonOnMonster(d, d5, m));
      });
      return total;
    }
    case MissileDispell: {
      if (!m.nonMaterial) return 0;
      int d5 = sw(at.range / 2 + 1);
      d5 = sw(d5 + d.rnd(d5) + 1);
      return sampleGroupShare(d, d5);
    }
  }
  return 0;
}

HeroHit missileOnHero(int missile, const MissileState &at, const Hero &h, const Party &party,
                      const GameData &g, Dice &dice)
{
  HeroHit r;
  r.arrives = at.arrives;
  if (!at.arrives) {
    r.note = "fades before it gets there";
    return r;
  }
  const int torsoAndHead = WoundHead | WoundTorso;   // mask 12
  switch (missile) {
    case MissileFireball:
    case MissileLightning: {
      int kind = missile == MissileFireball ? KindFire : KindMissile;
      HarmResult hit = damageHero(magicDamage(missile, at, dice), torsoAndHead, kind, h, party,
                                  g, dice);
      HarmResult burn;
      int size = cloudSize(missile, at.range);
      if (size > 0)
        burn = damageHero(shareOfPartyDamage(positive(blast(missile, size, dice)), dice), 63,
                          KindFire, h, party, g, dice);
      r.others = burn.damage;
      r.struck = sumWithZero(hit.damage, burn.damage);
      r.woundChance = 1.0 - (1.0 - hit.woundChance) * (1.0 - burn.woundChance);
      break;
    }
    case MissilePoison: {
      // the spit: its strength and its poison come from the same roll
      Dist all;
      double poisonMass = 0;
      for (int low = 0; low < 16; low++) {
        Dist base = dice.dist(32, low);
        HarmResult hr = damageHero(finishMagic(base, at, dice), torsoAndHead, KindBlunt, h,
                                   party, g, dice);
        all.add(hr.damage, 1.0 / 16);
        r.woundChance += hr.woundChance / 16;
        double p = hr.damage.mass() / 16 * 0.5;
        poisonMass += p;
        r.poisonStrength.add(low + 10, p);
      }
      r.struck = all;
      r.poisonChance = poisonMass;
      break;
    }
    case MissilePoisonCloud: {
      Dist total = Dist::point(0);
      bool first = true;
      eachCloudTick(at.range, [&](int v) {
        Dist tick = shareOfPartyDamage(poisonTick(v, dice), dice);
        if (first) r.perTick = tick;
        first = false;
        total = convolve(total, tick);
        r.ticks++;
      });
      r.struck = positive(total);
      r.others = r.struck;
      r.note = "total if the party stays in the cloud for all of its " +
               std::to_string(r.ticks) + " ticks; nothing reduces it";
      break;
    }
    default:
      r.note = "does no harm to the party";
      break;
  }
  r.stats = summarize(r.struck);
  return r;
}

int sampleMissileOnHero(Draw &d, int missile, const MissileState &at, const Hero &h,
                        const Party &party, const GameData &g, bool struck)
{
  if (!at.arrives) return 0;
  const int torsoAndHead = WoundHead | WoundTorso;
  switch (missile) {
    case MissileFireball:
    case MissileLightning: {
      int kind = missile == MissileFireball ? KindFire : KindMissile;
      int total = 0;
      if (struck)
        total += sampleDamageHero(d, sampleMagicDamage(d, missile, at), torsoAndHead, kind, h,
                                  party, g);
      int size = cloudSize(missile, at.range);
      if (size > 0) {
        int e = sampleBlast(d, missile, size);
        if (e != 0) {
          int d5 = e / 8 + 1;
          int d7 = e - d5;
          int share = std::max(1, d7 + d.rnd(2 * d5));
          total += sampleDamageHero(d, share, 63, KindFire, h, party, g);
        }
      }
      return total;
    }
    case MissilePoison:
      return sampleDamageHero(d, sampleMagicDamage(d, missile, at), torsoAndHead, KindBlunt, h,
                              party, g);
    case MissilePoisonCloud: {
      int total = 0;
      eachCloudTick(at.range, [&](int v) {
        int d5 = std::max(1, std::min(v / 32, 4) + d.bit());
        int a = d5 / 8 + 1;
        total += std::max(1, d5 - a + d.rnd(2 * a));
      });
      return total;
    }
  }
  return 0;
}

std::vector<MonsterMissile> monsterMissiles(const MonsterType &m)
{
  if (m.missileClass <= 1) return {};
  switch (m.id) {
    case 14:   // Vexirk
    case 23:   // Lord Chaos
      return {{MissileFireball, 0.5}, {MissileDispell, 0.125}, {MissileLightning, 0.125},
              {MissilePoisonCloud, 0.125}, {MissileZo, 0.125}};
    case 1: return {{MissilePoison, 1.0}};                                   // Slime Devil
    case 3: return {{MissileLightning, 7.0 / 8}, {MissileZo, 1.0 / 8}};      // Flying Eye
    case 19: return {{MissilePoisonCloud, 0.5}, {MissileFireball, 0.5}};     // Zytaz
    case 22:   // Demon
    case 24:   // Dragon
      return {{MissileFireball, 1.0}};
  }
  return {};
}

MonsterLaunch monsterLaunch(const MonsterType &m, Dice &dice)
{
  MonsterLaunch l;
  Dist range = Dist::point(sw(m.attack / 4 + 1));
  range = dice.plusRollOf(range, [](int v) { return v; });
  range = dice.plusRollOf(range, [](int v) { return v; });
  l.range = mapValues(range, [](int v) { return v <= 20 ? 20 : (v > 255 ? 255 : v); });
  l.damage = m.dexterity;
  l.decay = 8;
  return l;
}

ShieldCast castShield(ShieldSpell spell, int power, int current)
{
  ShieldCast c;
  const int d6 = 4 * (power + 1);   // the strength of every "light" class spell
  if (spell == ShieldYaIr) {
    c.added = d6;
    c.ticks = d6 * d6;
  } else {
    const int strength = d6 * d6 + 100;
    c.added = strength / 32;
    c.ticks = strength;
  }
  if (current > 50) c.added = spell == ShieldYaIr ? c.added >> 2 : c.added / 4;
  return c;
}

}  // namespace calc
