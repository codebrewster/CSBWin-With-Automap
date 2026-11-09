// Multi-threaded Monte Carlo over oneAttack() (the engine formulas).
// The work is cut into fixed-size batches (whole fights), each with its
// own RNG stream derived from the seed, so the result does not depend on
// the number of threads.  The spread between batches gives the standard
// errors the comparison with the exact calculation needs.
#pragma once
#include <atomic>
#include <utility>
#include <vector>
#include "DamageEngine.h"
#include "DamageResults.h"

struct McParams {
  Character ch;
  Monster mon;
  Options opt;
  int64_t trials = 1000000;
  uint64_t seed = 12345;
  int threads = 0;          // 0 = all cores
};

struct McProgress {
  std::atomic<uint64_t> done{0};
  std::atomic<bool> cancel{false};
};

// combos are (weapon index, attack index) into weapons()/attacks()
std::vector<ComboResult> runMonteCarlo(
    const McParams &p, const std::vector<std::pair<int, int>> &combos,
    McProgress *progress = nullptr);

// Exact and Monte Carlo agree?  z = (mc - exact) / se.
struct Check {
  const char *name;
  double exact, mc, se, z;
  bool ok;
};
struct CompareReport {
  std::vector<Check> checks;
  double chi2z = 0;        // z-score of the chi-square test of the damage histogram
  int cells = 0;
  bool rangeOk = true;
  bool ok = true;
};
CompareReport compareResults(const ComboResult &exact, const ComboResult &mc,
                             double zLimit = 4.5);
