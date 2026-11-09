#include "DamageEngine.h"

namespace eng {

int staminaAdjusted(const Character &ch, int stamina, int base)
{
  int half = ch.maxStamina / 2;           // D6W
  if (stamina < half) {
    int b = base / 2;
    return b + (b * stamina) / half;
  }
  return base;
}

int maxLoad(const Character &ch, int stamina)
{
  int ml = 8 * ch.strength + 100;
  ml = staminaAdjusted(ch, stamina, ml);
  if (ch.wounds) {                         // any wound: -1/8, legs: -1/4
    int sh = (ch.wounds & WoundLegs) ? 2 : 3;
    ml = ml - (ml >> sh);
  }
  if (ch.elvenBoots) ml = ml + (ml >> 4);
  ml += 9;
  return ml - ml % 10;
}

int quickness(const Character &ch, int ml, int a, int b, int c)
{
  int q = ch.dexterity + a;
  q = q - ((q / 2) * ch.load) / ml;
  q /= 2;
  int lo = b + 1, hi = 100 - c;
  if (q <= lo) return lo;
  if (q > hi) return hi;
  return q;
}

int throwingDistance(const Character &ch, const Weapon &w, int ml,
                     int stamina, int roll16)
{
  int d7 = ch.strength + roll16;
  int d6 = w.weight;
  int d5 = ml / 16;
  if (d6 <= d5) {
    d7 = d7 + d6 - 12;
  } else {
    int w2 = (d5 - 12) / 2 + d5;
    if (d6 <= w2) d7 = d7 + (d6 - d5) / 2;
    else d7 = d7 - 2 * (d6 - w2);
  }
  d7 += w.ubyte2;
  d6 = 0;
  int u1 = w.ubyte1;
  if (u1 == 0 || u1 == 2) d6 = ch.mastery[4];
  if (u1 != 0 && u1 < 16) d6 += ch.mastery[10];
  if (u1 >= 16 && u1 < 112) d6 += ch.mastery[11];
  d7 += 2 * d6;
  d7 = staminaAdjusted(ch, stamina, d7);
  if (ch.wounds & WoundActionHand) d7 /= 2;   // hand 1 -> mask 2
  int r = d7 / 2;
  return r <= 0 ? 0 : (r > 100 ? 100 : r);
}

int armorTerm(const Monster &m, const Weapon &w, bool attacking, int r32)
{
  int d4 = r32 + m.armor + m.level;
  if (attacking) d4 /= 2;
  if (w.armorDiv) d4 = d4 - d4 / w.armorDiv;
  return d4;
}

}  // namespace eng
