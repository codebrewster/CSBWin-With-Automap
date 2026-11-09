// Result types shared by the exact calculation and the Monte Carlo.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The columns of the result table (same as damage_calc_gui.py).
struct Row {
  std::string combo, weapon, attack;
  double hitPct = 0, dmgPerTry = 0, dmgPerHit = 0;
  int minHit = 0, maxHit = 0;
  double dps = 0, dmgPerSta = 0, staPerTry = 0, xpPerTry = 0, xps = 0;
  double endStamina = 0;
};

struct Mean {
  double v = 0;
  double se = 0;   // standard error (Monte Carlo only; 0 for exact)
};

// Per-swing means (endSta: per-fight mean), the quantities the two
// methods are compared on.
struct Means {
  Mean hit, dmg, cost, ticks, xp, endSta;
};

struct ComboResult {
  Row row;
  Means means;
  // fraction of swings per damage value (index 0 = no hit)
  std::vector<double> dmgFraction;      // exact
  std::vector<uint64_t> dmgCount;       // Monte Carlo
  int64_t trials = 0;
  bool structured = false;              // fights of fixed length
  bool singleRun = false;               // one long stateful sequence
  bool valid = false;
};
