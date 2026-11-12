#include "Defense.h"

#include "HeroAttack.h"

namespace calc {

const char *damageKindName(int kind)
{
  static const char *const names[] = {"plain", "fire", "blunt (half armour)", "blunt", "sharp",
                                      "magic", "psychic", "missile"};
  return kind >= 0 && kind < 8 ? names[kind] : "?";
}

int scaleByAttribute(int attribute, int damage)
{
  int f = 170 - attribute;
  if (f < 16) return damage >> 3;
  return int16_t((uint16_t(damage) * uint16_t(f)) >> 7);
}

namespace {

int scaledDefense(const ArmourType &a, bool sharp)   // getDefenseScaled
{
  return sharp ? (a.defense * ((a.pierce & 7) + 4)) >> 3 : a.defense;
}

const ArmourType *armourAt(const Hero &h, const GameData &g, int slot)
{
  int id = h.armour[slot];
  return id >= 0 && id < int(g.armours.size()) ? &g.armours[size_t(id)] : nullptr;
}

WeaponType heldThing(const Hero &h, int hand)   // not a weapon: only its weight counts
{
  WeaponType w;
  w.id = -1;
  w.weight = h.handWeight[hand];
  return w;
}

int clampProtection(int v)
{
  v >>= 1;
  return v <= 0 ? 0 : (v > 100 ? 100 : v);
}

int popcount6(int mask)
{
  int n = 0;
  for (int i = 0; i < 6; i++)
    if (mask & (1 << i)) n++;
  return n;
}

}  // namespace

Dist slotProtection(int slot, bool sharp, const Hero &h, const Party &party, const GameData &g,
                    Dice &dice)
{
  Dist sum = Dist::point(h.actionDefense + h.shield + party.partyShield);
  const int ml = eng::maxLoad(h, h.stamina);
  for (int hand = 0; hand < 2; hand++) {
    const ArmourType *a = armourAt(h, g, hand);
    if (!a || !a->isShield()) continue;
    Dist part;
    for (int r = 0; r < 16; r++) {
      int v = eng::throwingDistance(h, heldThing(h, hand), hand, ml, h.stamina, r) +
              scaledDefense(*a, sharp);
      v *= g.slotProtection[slot];
      part.add(v >> (hand == slot ? 4 : 5), 1.0 / 16);
    }
    sum = convolve(sum, part);
  }
  Dist vit = dice.dist(h.attr[Vitality] / 8 + 1);
  if (sharp) vit = mapValues(vit, [](int v) { return v / 2; });
  sum = convolve(sum, vit);
  if (slot > 1)
    if (const ArmourType *a = armourAt(h, g, slot))
      sum = mapValues(sum, [&](int v) { return v + scaledDefense(*a, sharp); });
  if (h.wounds & (1 << slot)) {
    Dist penalty;
    for (int r = 0; r < 4; r++) penalty.add(-(r + 8), 0.25);
    sum = convolve(sum, penalty);
  }
  if (party.sleeping) sum = mapValues(sum, [](int v) { return v >> 1; });
  return mapValues(sum, clampProtection);
}

static int sampleSlotProtection(Draw &d, int slot, bool sharp, const Hero &h, const Party &party,
                                const GameData &g)
{
  int d6 = 0;
  const int ml = eng::maxLoad(h, h.stamina);
  for (int hand = 0; hand < 2; hand++) {
    const ArmourType *a = armourAt(h, g, hand);
    if (!a || !a->isShield()) continue;
    int v = eng::throwingDistance(h, heldThing(h, hand), hand, ml, h.stamina, d.low(15)) +
            scaledDefense(*a, sharp);
    v *= g.slotProtection[slot];
    d6 += v >> (hand == slot ? 4 : 5);
  }
  int d7 = d.rnd(h.attr[Vitality] / 8 + 1);
  if (sharp) d7 /= 2;
  d7 += h.actionDefense + h.shield + party.partyShield + d6;
  if (slot > 1)
    if (const ArmourType *a = armourAt(h, g, slot)) d7 += scaledDefense(*a, sharp);
  if (h.wounds & (1 << slot)) d7 -= d.r03() + 8;
  if (party.sleeping) d7 >>= 1;
  return clampProtection(d7);
}

HarmResult damageHero(const Dist &raw, int mask, int kind, const Hero &h, const Party &party,
                      const GameData &g, Dice &dice)
{
  HarmResult out;
  Dist fin;
  if (kind == KindPlain) {
    for (size_t i = 0; i < raw.p.size(); i++)
      if (raw.lo + int(i) > 0) out.damage.add(raw.lo + int(i), raw.p[i]);
    return out;
  }
  // the armour the blow meets: the mean over the body parts it lands on
  Dist prot = Dist::point(0);
  const int parts = popcount6(mask);
  if (parts) {
    for (int slot = 0; slot < 6; slot++)
      if (mask & (1 << slot))
        prot = convolve(prot, slotProtection(slot, kind == KindSharp, h, party, g, dice));
    prot = mapValues(prot, [&](int v) { return v / parts; });
  }
  if (kind == KindPsychic) {
    int f = 115 - h.attr[Wisdom];
    fin = mapValues(raw, [&](int v) {
      return v <= 0 || f <= 0 ? 0 : int(int16_t((uint16_t(v) * uint16_t(f)) >> 6));
    });
  } else if (kind == KindMagic) {
    fin = mapValues(raw, [&](int v) {
      return v <= 0 ? 0 : scaleByAttribute(h.attr[AntiMagic], v) - party.spellShield;
    });
  } else {
    Dist pre = raw;
    if (kind == KindFire)
      pre = mapValues(raw, [&](int v) {
        return v <= 0 ? 0 : scaleByAttribute(h.attr[AntiFire], v) - party.fireShield;
      });
    if (kind == KindBlunt2) prot = mapValues(prot, [](int v) { return v >> 1; });
    out.protection = prot;
    for (size_t i = 0; i < pre.p.size(); i++) {
      int v = pre.lo + int(i);
      if (pre.p[i] == 0 || v <= 0) continue;
      for (size_t j = 0; j < prot.p.size(); j++) {
        if (prot.p[j] == 0) continue;
        int f = 130 - (prot.lo + int(j));
        fin.add(int(int16_t((uint16_t(v) * uint16_t(f)) >> 6)), pre.p[i] * prot.p[j]);
      }
    }
  }
  // wounds: the damage against a roll scaled by Vitality, doubling each round
  Dist limit = mapValues(dice.dist(128, 10),
                         [&](int v) { return scaleByAttribute(h.attr[Vitality], v); });
  const int slots = popcount6(mask);
  for (size_t i = 0; i < fin.p.size(); i++) {
    int v = fin.lo + int(i);
    double w = fin.p[i];
    if (w == 0 || v <= 0) continue;
    out.damage.add(v, w);
    if (!slots) continue;
    for (size_t j = 0; j < limit.p.size(); j++) {
      if (limit.p[j] == 0) continue;
      int d6 = limit.lo + int(j);
      if (v <= d6) continue;
      int rounds = 0;
      do {
        rounds++;
        d6 = int16_t(d6 << 1);
        if (v <= d6) break;
      } while (d6 != 0);
      double pw = w * limit.p[j];
      out.woundChance += pw * (1.0 - std::pow(1.0 - slots / 8.0, rounds));
      double one = pw * (1.0 - std::pow(7.0 / 8.0, rounds));
      for (int s = 0; s < 6; s++)
        if (mask & (1 << s)) out.woundSlot[s] += one;
    }
  }
  return out;
}

int sampleDamageHero(Draw &d, int damage, int mask, int kind, const Hero &h, const Party &party,
                     const GameData &g, int *wounds)
{
  int d7 = int16_t(damage);
  if (d7 <= 0) return 0;
  if (kind != KindPlain) {
    int d4 = 0, d5 = 0;
    for (int slot = 0; slot <= 5; slot++) {
      if (!(mask & (1 << slot))) continue;
      d4++;
      d5 += sampleSlotProtection(d, slot, kind == KindSharp, h, party, g);
    }
    if (d4) d5 /= d4;
    if (kind == KindPsychic) {
      int f = 115 - h.attr[Wisdom];
      d7 = f <= 0 ? 0 : int16_t((uint16_t(d7) * uint16_t(f)) >> 6);
    } else if (kind == KindMagic) {
      d7 = scaleByAttribute(h.attr[AntiMagic], d7) - party.spellShield;
    } else {
      if (kind == KindFire) d7 = scaleByAttribute(h.attr[AntiFire], d7) - party.fireShield;
      else if (kind == KindBlunt2) d5 >>= 1;
      if (d7 <= 0) return 0;
      d7 = int16_t((uint16_t(d7) * uint16_t(130 - d5)) >> 6);
    }
    if (d7 <= 0) return 0;
    int d6 = scaleByAttribute(h.attr[Vitality], d.rnd(128) + 10);
    if (d7 > d6) {
      do {
        int r = d.rnd(8);
        if (wounds) *wounds |= (1 << r) & mask;
        d6 = int16_t(d6 << 1);
        if (d7 <= d6) break;
      } while (d6 != 0);
    }
  }
  return d7 <= 0 ? 0 : d7;
}

Dist shareOfPartyDamage(const Dist &damage, Dice &dice)
{
  return bind(damage, [&](int v, double w, Dist &out) {
    if (v == 0) return;
    int d5 = v / 8 + 1;
    int d7 = v - d5;
    d5 *= 2;
    const auto &t = dice.roll(d5);
    for (int r = 0; r < d5; r++) out.add(std::max(1, d7 + r), w * t[size_t(r)]);
  });
}

Poison poisonCourse(int strength)
{
  Poison p;
  p.strength = strength;
  int rounds = 0;
  for (int s = strength;; s--) {
    p.totalDamage += std::max(1, s / 64);
    rounds++;
    if (s - 1 <= 0) break;
  }
  p.ticks = (rounds - 1) * 36;
  return p;
}

Hero withoutArmour(const Hero &hero)
{
  Hero h = hero;
  for (int &a : h.armour) a = -1;
  h.shield = 0;
  h.actionDefense = 0;
  return h;
}

}  // namespace calc
