// The game's physical-attack pipeline for player -> monster, as integer
// arithmetic with C semantics (truncating division), written line by line
// after the engine sources:
//   MaxLoad()                    Code11f52.cpp:4443
//   StaminaAdjustedAttribute()   Code11f52.cpp:4416
//   Quickness()                  Character.cpp:2200
//   IsCharacterLucky()           Character.cpp:2156 (incl. the +-2 luck drift)
//   DetermineThrowingDistance()  Character.cpp:2258
//   DeterminePhysicalAttackDamage() Attack.cpp:518 (to-hit gate, power, graze
//                                chain, randomise/shrink chain, Vorpal, mastery)
//   _Attack()                    Attack.cpp:1797  (stamina: energy + coin flip)
//   STRandom()/STRandom(n)       CSBCode.cpp:1216 ((r & 0xffff) % n)
// The Monte Carlo (DamageMonteCarlo) calls oneAttack(); the exact
// calculation (DamageExact) enumerates the very same rolls through the
// same helper functions.
#pragma once
#include <cstdint>
#include "DamageData.h"

struct Options {
  bool luckDrift = true;         // IsCharacterLucky moves Luck by -+2 per call
  bool moduloBias = true;        // STRandom(n) = (r & 0xffff) % n  (else uniform)
  bool monsterAttacking = false; // monsters_vulnerable_on_attack: defence halved
  bool staminaDrain = false;
  int swingsPerFight = 50;       // stamina and luck restart every fight; 0 = never
};

// ------------------------------------------------------------ primitives
namespace eng {

// C-style a / b (truncating); b != 0
inline int cdiv(int a, int b) { return a / b; }

int staminaAdjusted(const Character &ch, int stamina, int base);
int maxLoad(const Character &ch, int stamina);
// the three STRandom(8) rolls of Quickness()
int quickness(const Character &ch, int ml, int a, int b, int c);
int throwingDistance(const Character &ch, const Weapon &w, int ml,
                     int stamina, int roll16);
// D4 = defence of the monster for one roll r32 of STRandom(32)
int armorTerm(const Monster &m, const Weapon &w, bool attacking, int r32);

}  // namespace eng

// ------------------------------------------------------------ RNGs
// raw() returns 24 random bits like the game's STRandom(void).  (The game's
// own 32-bit LCG is deliberately not used: its low bits have periods of a
// few hundred values, which correlates the draws of a fixed-length swing.)
struct XoshiroRng {  // xoshiro256**, seeded through splitmix64
  uint64_t s[4];
  static uint64_t splitmix(uint64_t &x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
  explicit XoshiroRng(uint64_t seed) {
    for (auto &v : s) v = splitmix(seed);
  }
  static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
  inline uint64_t next() {
    uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t; s[3] = rotl(s[3], 45);
    return r;
  }
  inline uint32_t raw() { return uint32_t(next() >> 40); }
  inline uint32_t u32() { return uint32_t(next() >> 32); }
};

// ------------------------------------------------------------ one swing
struct SwingResult {
  int damage = 0;   // 0 when nothing was hit
  bool hit = false;
  int cost = 0;     // stamina cost of the attempt
};

struct SwingCtx {          // everything fixed over a run of swings
  const Character *ch;
  const Weapon *wpn;
  const Attack *atk;
  const Monster *mon;
  Options opt;
  int halfStamina;         // MaxStamina / 2
  int mlFull;              // MaxLoad at stamina >= half
  bool vorpalOrDisrupt;
  SwingCtx(const Character &c, const Weapon &w, const Attack &a,
           const Monster &m, const Options &o)
      : ch(&c), wpn(&w), atk(&a), mon(&m), opt(o) {
    halfStamina = c.maxStamina / 2;
    mlFull = eng::maxLoad(c, c.maxStamina);
    vorpalOrDisrupt = w.vorpal || a.disrupt;
  }
};

struct SwingState {        // what a swing changes
  int stamina;
  int luck;
};

template <class R>
struct Draw {              // STRandom family on top of a raw 24-bit source
  R &r;
  bool bias;
  inline int rnd(int n) {  // STRandom(n)
    if (bias) return int((r.raw() & 0xffff) % uint32_t(n));
    return int((uint64_t(r.u32()) * uint32_t(n)) >> 32);
  }
  inline int bit() { return int(r.raw() & 1); }          // STRandomBool
  inline int r03() { return int(r.raw() & 3); }          // STRandom0_3
  inline int low16() { return int(r.raw() & 0xffff); }
};

template <class R>
inline SwingResult oneAttack(R &rng, const SwingCtx &c, SwingState &st)
{
  Draw<R> d{rng, c.opt.moduloBias};
  const Character &ch = *c.ch;
  const Weapon &w = *c.wpn;
  const Attack &a = *c.atk;
  const Monster &m = *c.mon;
  SwingResult res;
  auto finish = [&](int sadj) {
    int cost = a.energy + d.bit() + sadj;
    if (c.opt.staminaDrain) st.stamina = st.stamina > cost ? st.stamina - cost : 0;
    return cost;
  };
  // Non-material beings: only the Vorpal Blade and Disrupt reach the
  // damage code; anything else spends 2 + STRandomBool (Attack.cpp:948).
  if (m.nonMaterial && !c.vorpalOrDisrupt) {
    res.cost = finish(2 + d.bit());
    return res;
  }
  if (a.disrupt && !m.nonMaterial) {  // Disrupt fizzles (Attack.cpp:1117)
    res.cost = finish(0);
    return res;
  }
  const int ml = st.stamina >= c.halfStamina ? c.mlFull
                                             : eng::maxLoad(ch, st.stamina);
  // ---- to-hit gate (Attack.cpp:599-635)
  int qa = d.rnd(8);
  int qb = d.rnd(8), qc = d.rnd(8);
  int q = eng::quickness(ch, ml, qa, qb, qc);
  int need = d.rnd(32) + m.dex + m.level - 16;
  if (q <= need) {
    bool lucky = (d.r03() == 0);
    if (!lucky) {
      // IsCharacterLucky(75 - P7)  (Character.cpp:2156)
      int luckNeeded = 75 - a.p7;
      bool ok = false;
      if (d.bit() && d.rnd(100) > luckNeeded) {
        ok = true;
      } else {
        int cur = st.luck;
        int roll = cur == 0 ? 0 : d.rnd(cur);
        ok = roll > luckNeeded;
        if (c.opt.luckDrift) {
          int v = cur + (ok ? -2 : 2);
          st.luck = v <= ch.luckMin ? ch.luckMin : (v > ch.luckMax ? ch.luckMax : v);
        }
      }
      if (!ok) {
        res.cost = finish(2 + d.bit());
        return res;
      }
    }
  }
  // ---- power (Attack.cpp:653-713)
  int t = eng::throwingDistance(ch, w, ml, st.stamina, d.low16() & 15);
  int d7, d6;
  if (t != 0) {
    d7 = t + d.rnd(t / 2 + 1);
    d7 = d7 * a.p8 / 32;
    int d4 = eng::armorTerm(m, w, c.opt.monsterAttacking, d.rnd(32));
    d7 = d7 + d.rnd(32) - d4;
    d6 = d7;
  } else {
    d6 = 0;
    d7 = 0;
  }
  if (t == 0 || d6 <= 1) {  // graze chain (Attack.cpp:720-809)
    d7 = d.r03();
    if (d7 == 0) {
      res.cost = finish(2 + d.bit());
      return res;
    }
    d6 += d.rnd(16);
    if (d6 > 0 || d.bit()) {
      d7 += d.r03();
      if (d.r03() == 0) {
        int v = d.rnd(16) + d6;
        d7 += v > 0 ? v : 0;
      }
    }
  }
  // ---- randomise / shrink (Attack.cpp:810-844)
  d7 /= 2;
  int d0 = d7 == 0 ? 0 : d.rnd(d7);
  d7 = d7 + d.r03() + d0;
  if (d7 != 0) d7 = d7 + d.rnd(d7);
  d7 /= 4;
  d7 = d7 + d.r03() + 1;
  if (w.vorpal && !m.nonMaterial) {  // Attack.cpp:856
    d7 /= 2;
    if (d7 == 0) {
      res.cost = finish(2 + d.bit());
      return res;
    }
  }
  if (d.rnd(64) < ch.mastery[a.skill]) d7 += 10;  // Attack.cpp:884
  if (d7 < 0) d7 = 0;
  res.cost = finish(4 + d.r03());  // Attack.cpp:925, every hit
  res.damage = d7;
  res.hit = true;
  return res;
}
