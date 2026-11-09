// Command-line check: exact vs Monte Carlo on random scenarios, plus
// speed.  Usage: damage_cli [scenarios] [trials] [threads]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include "DamageExact.h"
#include "DamageMonteCarlo.h"

int main(int argc, char **argv)
{
  int count = argc > 1 ? atoi(argv[1]) : 40;
  int64_t trials = argc > 2 ? atoll(argv[2]) : 2000000;
  int threads = argc > 3 ? atoi(argv[3]) : 0;
  std::mt19937_64 rng(20260101);
  auto pick = [&](std::initializer_list<int> v) {
    return *(v.begin() + rng() % v.size());
  };
  int bad = 0;
  double tEx = 0, tMc = 0;
  for (int i = 0; i < count; i++) {
    McParams p;
    Character &ch = p.ch;
    ch.strength = pick({8, 20, 35, 55, 70, 90});
    ch.dexterity = pick({10, 30, 43, 70});
    ch.luck = pick({0, 1, 25, 50, 80});
    ch.load = pick({0, 100, 200, 400, 900});
    ch.elvenBoots = rng() % 10 < 3;
    ch.maxStamina = pick({200, 750, 1500});
    ch.wounds = pick({0, 0, 0, 2, 16, 3});
    ch.luckMax = pick({60, 100});
    for (int s = 4; s < 18; s++) ch.mastery[s] = int(rng() % 16);
    ch.stamina = pick({ch.maxStamina, ch.maxStamina, ch.maxStamina / 3,
                       ch.maxStamina / 10});
    int wi = int(rng() % weapons().size()), ai = int(rng() % attacks().size());
    p.mon = monsters()[rng() % monsters().size()];
    p.opt.staminaDrain = rng() % 10 < 4;
    p.opt.luckDrift = rng() % 10 < 6;
    p.opt.moduloBias = rng() % 10 < 7;
    p.opt.monsterAttacking = rng() % 10 < 2;
    p.opt.swingsPerFight = p.opt.staminaDrain ? pick({0, 25, 60}) : pick({0, 40});
    p.trials = trials;
    if (p.opt.swingsPerFight > 0)
      p.trials += p.opt.swingsPerFight - p.trials % p.opt.swingsPerFight;
    p.seed = 1000 + i;
    p.threads = threads;

    ExactParams ep;
    ep.ch = p.ch; ep.mon = p.mon; ep.opt = p.opt; ep.trials = p.trials;
    auto t0 = std::chrono::steady_clock::now();
    ComboResult ex = exactCombo(ep, weapons()[size_t(wi)], attacks()[size_t(ai)]);
    auto t1 = std::chrono::steady_clock::now();
    auto mcr = runMonteCarlo(p, {{wi, ai}});
    auto t2 = std::chrono::steady_clock::now();
    tEx += std::chrono::duration<double>(t1 - t0).count();
    tMc += std::chrono::duration<double>(t2 - t1).count();
    CompareReport rep = compareResults(ex, mcr[0]);
    printf("%s %2d %-14s %-8s %-15s drain=%d fight=%2d drift=%d bias=%d att=%d",
           rep.ok ? "ok  " : "FAIL", i, weapons()[size_t(wi)].name,
           attacks()[size_t(ai)].name, p.mon.name.c_str(), p.opt.staminaDrain,
           p.opt.swingsPerFight, p.opt.luckDrift, p.opt.moduloBias,
           p.opt.monsterAttacking);
    printf("  hit %.4f/%.4f dmg %.4f/%.4f chi2z %.1f\n", ex.means.hit.v,
           mcr[0].means.hit.v, ex.means.dmg.v, mcr[0].means.dmg.v, rep.chi2z);
    if (!rep.ok) {
      bad++;
      for (auto &c : rep.checks)
        if (!c.ok)
          printf("       %-14s exact %.6f  mc %.6f  se %.2g  z %.1f\n", c.name,
                 c.exact, c.mc, c.se, c.z);
      if (!rep.rangeOk)
        printf("       range mc %d..%d exact %d..%d\n", mcr[0].row.minHit,
               mcr[0].row.maxHit, ex.row.minHit, ex.row.maxHit);
    }
  }
  printf("%d/%d failed; exact %.2fs, Monte Carlo %.2fs (%lld trials each)\n",
         bad, count, tEx, tMc, (long long)trials);
  return bad ? 1 : 0;
}
