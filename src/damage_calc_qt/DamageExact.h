// Exact (no sampling) evaluation of oneAttack(): every random step is a
// uniform integer roll over a small range, so the whole distribution of
// one swing (damage, hit?, stamina cost) is obtained by propagating
// probability tables through the same steps in the same order.  Stamina
// and luck are carried as a joint Markov state from swing to swing.
#pragma once
#include <utility>
#include "DamageEngine.h"
#include "DamageResults.h"

struct ExactParams {
  Character ch;
  Monster mon;
  Options opt;
  int64_t trials = 20000;
};

ComboResult exactCombo(const ExactParams &p, const Weapon &w, const Attack &a);

// DPS of one swing at the character's stamina (what the strength graph needs)
double exactDps(const Character &ch, const Weapon &w, const Attack &a,
                const Monster &m, const Options &opt);

// All combos, spread over `threads` threads (0 = all cores); combos are
// (weapon index, attack index).
std::vector<ComboResult> exactAll(
    const ExactParams &p, const std::vector<std::pair<int, int>> &combos,
    int threads = 0);
