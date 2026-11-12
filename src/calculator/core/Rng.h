// Random source of the Monte Carlo transcriptions that check the exact
// tables (tests, calc_check).  raw() gives 24 random bits like the game's
// STRandom(void); the game's own generator is not used because its low
// bits repeat after a few hundred values.
#pragma once
#include <cstdint>

namespace calc {

struct Rng {   // xoshiro256**, seeded through splitmix64
  uint64_t s[4];
  static uint64_t splitmix(uint64_t &x)
  {
    uint64_t z = (x += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
  explicit Rng(uint64_t seed)
  {
    for (auto &v : s) v = splitmix(seed);
  }
  static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
  uint64_t next()
  {
    uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t; s[3] = rotl(s[3], 45);
    return r;
  }
  uint32_t raw() { return uint32_t(next() >> 40); }
  uint32_t u32() { return uint32_t(next() >> 32); }
};

// The STRandom family on top of a raw source
struct Draw {
  Rng &r;
  bool bias = true;
  int rnd(int n)   // STRandom(n)
  {
    if (n < 1) n = 1;
    if (bias) return int((r.raw() & 0xffff) % uint32_t(n));
    return int((uint64_t(r.u32()) * uint32_t(n)) >> 32);
  }
  int bit() { return int(r.raw() & 1); }        // STRandomBool
  int r03() { return int(r.raw() & 3); }        // STRandom0_3
  int low(int mask) { return int(r.raw() & uint32_t(mask)); }
};

}  // namespace calc
