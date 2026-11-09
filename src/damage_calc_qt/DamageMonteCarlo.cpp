#include "DamageMonteCarlo.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace {

constexpr int kHistSize = 2048;
constexpr int64_t kBatchSwings = 1 << 16;

struct Batch {
  uint64_t swings = 0, hits = 0, fights = 0;
  int64_t dmg = 0, cost = 0, ticks2 = 0, xp = 0, endSum = 0;
  uint64_t dmg2 = 0;
  int minHit = 1 << 30, maxHit = 0;
};

struct ComboPlan {
  Weapon w;
  Attack a;
  int64_t batchSwings;      // swings per task
  int64_t nBatches;         // tasks of this combo
  bool structured;
  bool sequential;          // one task runs all batches in order
  std::vector<Batch> batches;
  std::vector<std::vector<uint64_t>> hist;   // per worker
};

struct Task {
  int combo;
  int64_t batch;
};

// Runs `count` swings; st/fightPos carry over when one task runs several
// batches of a single long sequence (lastBatch closes its end stamina).
template <class R>
bool runBatch(const McParams &p, const ComboPlan &plan, SwingCtx &ctx, R &rng,
              int64_t count, Batch &b, std::vector<uint64_t> &hist,
              McProgress *prog, SwingState &st, int64_t &fightPos,
              bool lastBatch)
{
  const Attack &a = plan.a;
  const Monster &m = p.mon;
  const int F = p.opt.swingsPerFight;
  const int start = std::min(p.ch.stamina, p.ch.maxStamina);
  const int mult = m.level;
  uint64_t local = 0;
  bool cancelled = false;
  for (int64_t i = 0; i < count; i++) {
    if (plan.structured && fightPos == F) {   // next fight
      b.endSum += st.stamina;
      b.fights++;
      st = SwingState{start, p.ch.luck};
      fightPos = 0;
    }
    fightPos++;
    SwingResult r = oneAttack(rng, ctx, st);
    b.swings++;
    b.cost += r.cost;
    int gained;
    if (r.hit) {
      b.hits++;
      b.dmg += r.damage;
      b.dmg2 += uint64_t(r.damage) * uint64_t(r.damage);
      b.ticks2 += 2 * a.time;
      if (r.damage < b.minHit) b.minHit = r.damage;
      if (r.damage > b.maxHit) b.maxHit = r.damage;
      gained = a.exp + (r.damage * m.exp) / 16 + 3;
      hist[size_t(std::min(r.damage, kHistSize - 1))]++;
    } else {
      b.ticks2 += a.time;
      gained = a.exp / 2;
      hist[0]++;
    }
    if (mult != 0) gained *= mult;
    b.xp += int64_t(gained) * 2;
    if (prog && ++local == 4096) {
      prog->done.fetch_add(local, std::memory_order_relaxed);
      local = 0;
      if (prog->cancel.load(std::memory_order_relaxed)) {
        cancelled = true;
        break;
      }
    }
  }
  if (prog) prog->done.fetch_add(local, std::memory_order_relaxed);
  if (plan.structured || lastBatch) {
    b.endSum += st.stamina;
    b.fights++;
  }
  return !cancelled;
}

uint64_t mix(uint64_t a, uint64_t b)
{
  uint64_t x = a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
  return XoshiroRng::splitmix(x);
}

Mean batchMean(const std::vector<double> &x, double overall)
{
  Mean m;
  m.v = overall;
  size_t n = x.size();
  if (n < 2) return m;
  double s = 0;
  for (double v : x) s += (v - overall) * (v - overall);
  m.se = std::sqrt(s / double(n - 1) / double(n));
  return m;
}

}  // namespace

std::vector<ComboResult> runMonteCarlo(
    const McParams &p, const std::vector<std::pair<int, int>> &combos,
    McProgress *prog)
{
  const auto &W = weapons();
  const auto &A = attacks();
  const Options &o = p.opt;
  int nThreads = p.threads > 0 ? p.threads
                               : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;

  std::vector<ComboPlan> plans(combos.size());
  std::vector<Task> tasks;
  for (size_t ci = 0; ci < combos.size(); ci++) {
    ComboPlan &pl = plans[ci];
    pl.w = W[size_t(combos[ci].first)];
    pl.a = A[size_t(combos[ci].second)];
    const bool fixedGate = (p.mon.nonMaterial && !(pl.w.vorpal || pl.a.disrupt)) ||
                           (pl.a.disrupt && !p.mon.nonMaterial);
    const bool drift = o.luckDrift && !fixedGate;
    const bool stateful = o.staminaDrain || drift;
    const int F = o.swingsPerFight;
    pl.structured = F > 0 && stateful;
    pl.sequential = stateful && !pl.structured;
    if (pl.structured) {
      int64_t k = (kBatchSwings + F - 1) / F;
      pl.batchSwings = k * F;
      pl.nBatches = (p.trials + pl.batchSwings - 1) / pl.batchSwings;
    } else if (stateful) {          // one long sequence: a single task
      pl.batchSwings = kBatchSwings;
      pl.nBatches = (p.trials + kBatchSwings - 1) / kBatchSwings;
    } else {
      pl.batchSwings = kBatchSwings;
      pl.nBatches = (p.trials + kBatchSwings - 1) / kBatchSwings;
    }
    pl.batches.assign(size_t(pl.nBatches), Batch());
    pl.hist.assign(size_t(nThreads), std::vector<uint64_t>(kHistSize, 0));
    if (pl.sequential) tasks.push_back({int(ci), 0});
    else
      for (int64_t b = 0; b < pl.nBatches; b++) tasks.push_back({int(ci), b});
  }
  // longest sequences first (the single-task stateful ones)
  std::stable_sort(tasks.begin(), tasks.end(), [&](const Task &x, const Task &y) {
    return plans[size_t(x.combo)].sequential > plans[size_t(y.combo)].sequential;
  });

  std::atomic<size_t> next{0};
  auto worker = [&](int wid) {
    for (;;) {
      size_t ti = next.fetch_add(1);
      if (ti >= tasks.size()) break;
      if (prog && prog->cancel.load()) break;
      const Task &t = tasks[ti];
      ComboPlan &pl = plans[size_t(t.combo)];
      SwingCtx ctx(p.ch, pl.w, pl.a, p.mon, p.opt);
      uint64_t seed = mix(mix(p.seed, uint64_t(t.combo)), uint64_t(t.batch));
      XoshiroRng rng(seed);
      auto &h = pl.hist[size_t(wid)];
      SwingState st{std::min(p.ch.stamina, p.ch.maxStamina), p.ch.luck};
      int64_t fightPos = 0;
      int64_t from = pl.sequential ? 0 : t.batch;
      int64_t to = pl.sequential ? pl.nBatches : t.batch + 1;
      for (int64_t bi = from; bi < to; bi++) {
        int64_t count = std::min<int64_t>(pl.batchSwings, p.trials - bi * pl.batchSwings);
        if (count <= 0) break;
        if (!runBatch(p, pl, ctx, rng, count, pl.batches[size_t(bi)], h, prog,
                      st, fightPos, bi == pl.nBatches - 1))
          break;
      }
    }
  };
  std::vector<std::thread> pool;
  for (int i = 1; i < nThreads; i++) pool.emplace_back(worker, i);
  worker(0);
  for (auto &t : pool) t.join();

  // ---- collect
  std::vector<ComboResult> out(combos.size());
  for (size_t ci = 0; ci < combos.size(); ci++) {
    ComboPlan &pl = plans[ci];
    ComboResult &r = out[ci];
    Batch tot;
    std::vector<double> xh, xd, xc, xt, xx, xe;
    for (const Batch &b : pl.batches) {
      if (!b.swings) continue;
      tot.swings += b.swings; tot.hits += b.hits; tot.fights += b.fights;
      tot.dmg += b.dmg; tot.cost += b.cost; tot.ticks2 += b.ticks2;
      tot.xp += b.xp; tot.endSum += b.endSum; tot.dmg2 += b.dmg2;
      tot.minHit = std::min(tot.minHit, b.minHit);
      tot.maxHit = std::max(tot.maxHit, b.maxHit);
      double n = double(b.swings);
      xh.push_back(b.hits / n);
      xd.push_back(b.dmg / n);
      xc.push_back(b.cost / n);
      xt.push_back(b.ticks2 / 2.0 / n);
      xx.push_back(b.xp / n);
      if (b.fights) xe.push_back(double(b.endSum) / double(b.fights));
    }
    if (!tot.swings) continue;
    r.valid = true;
    r.structured = pl.structured;
    r.singleRun = pl.sequential;
    r.trials = int64_t(tot.swings);
    const double n = double(tot.swings);
    const double ticks = tot.ticks2 / 2.0;
    Row &row = r.row;
    row.combo = std::string(pl.w.name) + "+" + pl.a.name;
    row.weapon = pl.w.name;
    row.attack = pl.a.name;
    row.hitPct = 100.0 * tot.hits / n;
    row.dmgPerTry = tot.dmg / n;
    row.dmgPerHit = double(tot.dmg) / double(std::max<uint64_t>(tot.hits, 1));
    row.minHit = tot.hits ? tot.minHit : 0;
    row.maxHit = tot.maxHit;
    row.dps = 6.0 * tot.dmg / std::max(ticks, 1.0);
    row.dmgPerSta = double(tot.dmg) / double(std::max<int64_t>(tot.cost, 1));
    row.staPerTry = tot.cost / n;
    row.xpPerTry = tot.xp / n;
    row.xps = 6.0 * tot.xp / std::max(ticks, 1.0);
    row.endStamina = tot.fights ? double(tot.endSum) / double(tot.fights) : 0;
    r.means.hit = batchMean(xh, tot.hits / n);
    r.means.dmg = batchMean(xd, tot.dmg / n);
    r.means.cost = batchMean(xc, tot.cost / n);
    r.means.ticks = batchMean(xt, ticks / n);
    r.means.xp = batchMean(xx, tot.xp / n);
    r.means.endSta = batchMean(xe, row.endStamina);
    r.dmgCount.assign(kHistSize, 0);
    for (auto &h : pl.hist)
      for (int i = 0; i < kHistSize; i++) r.dmgCount[size_t(i)] += h[size_t(i)];
  }
  return out;
}

CompareReport compareResults(const ComboResult &ex, const ComboResult &mc,
                             double zLimit)
{
  CompareReport rep;
  auto add = [&](const char *name, const Mean &e, const Mean &m) {
    Check c{name, e.v, m.v, m.se, 0, true};
    double d = m.v - e.v;
    if (m.se > 0) c.z = d / m.se;
    else c.z = (std::fabs(d) < 1e-9 * std::max(1.0, std::fabs(e.v)) ? 0 : 1e9);
    c.ok = std::fabs(c.z) <= zLimit;
    rep.ok = rep.ok && c.ok;
    rep.checks.push_back(c);
  };
  add("hit rate", ex.means.hit, mc.means.hit);
  add("damage / try", ex.means.dmg, mc.means.dmg);
  add("stamina / try", ex.means.cost, mc.means.cost);
  add("ticks / try", ex.means.ticks, mc.means.ticks);
  add("xp / try", ex.means.xp, mc.means.xp);
  if (!mc.singleRun || ex.means.endSta.v == mc.means.endSta.v)
    add("end stamina", ex.means.endSta, mc.means.endSta);
  // range of a single hit: the sampled range must lie inside the exact one
  if (mc.row.maxHit > ex.row.maxHit ||
      (mc.row.minHit && mc.row.minHit < ex.row.minHit))
    rep.rangeOk = false;
  rep.ok = rep.ok && rep.rangeOk;
  // chi-square of the whole damage distribution (0 = no hit); cells merged
  // until the expected count is >= 20
  const double n = double(mc.trials);
  size_t len = std::max(ex.dmgFraction.size(), mc.dmgCount.size());
  double accE = 0, accO = 0;
  std::vector<std::pair<double, double>> cells;
  for (size_t d = 0; d < len; d++) {
    accE += (d < ex.dmgFraction.size() ? ex.dmgFraction[d] : 0.0) * n;
    accO += d < mc.dmgCount.size() ? double(mc.dmgCount[d]) : 0.0;
    if (accE >= 20) {
      cells.push_back({accE, accO});
      accE = accO = 0;
    }
  }
  if (accE > 0 || accO > 0) {
    if (!cells.empty()) {
      cells.back().first += accE;
      cells.back().second += accO;
    } else {
      cells.push_back({accE, accO});
    }
  }
  if (cells.size() >= 2) {
    double chi = 0;
    for (auto &c : cells)
      if (c.first > 0) chi += (c.second - c.first) * (c.second - c.first) / c.first;
    int df = int(cells.size()) - 1;
    rep.cells = int(cells.size());
    rep.chi2z = (chi - df) / std::sqrt(2.0 * df);
    // swings of one fight are correlated when stamina/luck move, so this
    // test is only a rough guide there; it only fails on a clear excess
    if (rep.chi2z > 5.0 + (ex.structured ? 3.0 : 0.0)) rep.ok = false;
  }
  return rep;
}
