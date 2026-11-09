#include "DamageExact.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <map>
#include <thread>
#include <unordered_map>

namespace {

// ---------------------------------------------------------------- PMFs
struct Pmf {
  int lo = 0;
  std::vector<double> p;
  bool empty() const { return p.empty(); }
  int hi() const { return lo + int(p.size()) - 1; }
  void add(int v, double w) {
    if (p.empty()) {
      lo = v;
      p.assign(1, 0.0);
    }
    if (v < lo) {
      p.insert(p.begin(), size_t(lo - v), 0.0);
      lo = v;
    } else if (v > hi()) {
      p.resize(size_t(v - lo + 1), 0.0);
    }
    p[size_t(v - lo)] += w;
  }
  double at(int v) const {
    return (v < lo || v > hi()) ? 0.0 : p[size_t(v - lo)];
  }
};

// distribution of STRandom(n) = (r & 0xffff) % n, r uniform in 0..65535
struct RollTables {
  bool bias;
  std::vector<std::vector<double>> cache;
  explicit RollTables(bool b) : bias(b) {}
  const std::vector<double> &get(int n) {
    if (int(cache.size()) <= n) cache.resize(size_t(n) + 1);
    auto &v = cache[size_t(n)];
    if (v.empty()) {
      v.resize(size_t(n));
      for (int i = 0; i < n; i++)
        v[size_t(i)] = bias ? double((65535 - i) / n + 1) / 65536.0 : 1.0 / n;
    }
    return v;
  }
};

Pmf conv(const Pmf &a, const Pmf &b)
{
  Pmf out;
  if (a.empty() || b.empty()) return out;
  out.lo = a.lo + b.lo;
  out.p.assign(a.p.size() + b.p.size() - 1, 0.0);
  for (size_t i = 0; i < a.p.size(); i++) {
    double pa = a.p[i];
    if (pa == 0.0) continue;
    for (size_t j = 0; j < b.p.size(); j++) out.p[i + j] += pa * b.p[j];
  }
  return out;
}

// -------------------------------------------------------------- one swing
struct Metrics {
  double pHit = 0, eDmg = 0, eDmg2 = 0, eCost = 0, eTicks = 0, eXp = 0;
};

struct Core {                 // one swing at one stamina class
  bool fixed = false;         // no to-hit gate (non-material / fizzling Disrupt)
  double g1 = 0;              // P(quickness <= needed)
  std::vector<double> hit;    // P(damage = d | gate passed)
  std::vector<std::pair<int, double>> costPass;
  Metrics pass;               // expectations given the gate was passed
  int minHit = 0, maxHit = 0;
};

class Model {
 public:
  Model(const ExactParams &p, const Weapon &w, const Attack &a)
      : p_(p), w_(w), a_(a), rolls_(p.opt.moduloBias)
  {
    mult_ = p.mon.level;
    missXp_ = a.exp / 2;
    if (mult_ != 0) missXp_ *= mult_;
    missXp_ *= 2;
    // gate-miss: energy + R2 + 2 + R2
    for (int r = 0; r < 2; r++)
      for (int s = 2; s <= 3; s++) missCost_.push_back({a.energy + r + s, 0.25});
    missM_.eCost = a.energy + 0.5 + 2.5;
    missM_.eTicks = a.time / 2.0;
    missM_.eXp = missXp_;
    // luck: IsCharacterLucky(75 - P7)
    need_ = 75 - a.p7;
    const auto &w100 = rolls_.get(100);
    double s = 0;
    for (int v = 0; v < 100; v++)
      if (v > need_) s += w100[size_t(v)];
    p1_ = 0.5 * s;
    fixedFlag_ = (p.mon.nonMaterial && !(w.vorpal || a.disrupt)) ||
                 (a.disrupt && !p.mon.nonMaterial);
  }

  const Core &core(int stamina) {
    int key = stamina < p_.ch.maxStamina / 2 ? stamina : -1;
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    return cache_.emplace(key, buildCore(stamina)).first->second;
  }

  double p2(int luck)       // P(STRandom(luck) > need)
  {
    if (luck <= 0) return 0 > need_ ? 1.0 : 0.0;
    const auto &t = rolls_.get(luck);
    double s = 0;
    for (int v = 0; v < luck; v++)
      if (v > need_) s += t[size_t(v)];
    return s;
  }
  double p1() const { return p1_; }
  const std::vector<std::pair<int, double>> &missCost() const { return missCost_; }
  const Metrics &missMetrics() const { return missM_; }
  bool fixed() const { return fixedFlag_; }
  const ExactParams &params() const { return p_; }
  const Attack &atk() const { return a_; }

 private:
  Core buildCore(int stamina)
  {
    Core c;
    const Character &ch = p_.ch;
    const Monster &m = p_.mon;
    const int energy = a_.energy;
    if (fixedFlag_) {
      c.fixed = true;
      c.g1 = 0;
      if (a_.disrupt && !m.nonMaterial) {
        for (int r = 0; r < 2; r++) c.costPass.push_back({energy + r, 0.5});
        c.pass.eCost = energy + 0.5;
      } else {
        for (int r = 0; r < 2; r++)
          for (int s = 2; s <= 3; s++) c.costPass.push_back({energy + r + s, 0.25});
        c.pass.eCost = energy + 0.5 + 2.5;
      }
      c.pass.eTicks = a_.time / 2.0;
      c.pass.eXp = missXp_;
      return c;
    }
    const int ml = eng::maxLoad(ch, stamina);
    // ---- gate: P(q <= need), need = dex + R32 + level - 16
    std::map<int, double> qp;
    for (int a = 0; a < 8; a++)
      for (int b = 0; b < 8; b++)
        for (int cc = 0; cc < 8; cc++)
          qp[eng::quickness(ch, ml, a, b, cc)] += 1.0 / 512;
    int base = m.dex + m.level - 16;
    double ple = 0;
    for (auto &kv : qp) {
      int n = 0;
      for (int r = 0; r < 32; r++)
        if (kv.first <= base + r) n++;
      ple += kv.second * n / 32.0;
    }
    c.g1 = ple;
    // ---- power: d6 before the graze test
    double t0 = 0;
    Pmf amix;
    for (int r = 0; r < 16; r++) {
      int t = eng::throwingDistance(ch, w_, ml, stamina, r);
      if (t == 0) {
        t0 += 1.0 / 16;
        continue;
      }
      int n = t / 2 + 1;
      const auto &wt = rolls_.get(n);
      for (int rr = 0; rr < n; rr++)
        amix.add((t + rr) * a_.p8 / 32, wt[size_t(rr)] / 16.0);
    }
    Pmf x;
    for (int r4 = 0; r4 < 32; r4++) {
      int d4 = eng::armorTerm(m, w_, p_.opt.monsterAttacking, r4);
      for (int r = 0; r < 32; r++) x.add(r - d4, 1.0 / 1024);
    }
    Pmf d6 = conv(amix, x);
    // ---- split: d6 <= 1 (or t == 0) goes through the graze chain
    Pmf gd6, clean;
    if (t0 > 0) gd6.add(0, t0);
    for (size_t i = 0; i < d6.p.size(); i++) {
      int v = d6.lo + int(i);
      if (d6.p[i] == 0) continue;
      if (v <= 1) gd6.add(v, d6.p[i]);
      else clean.add(v, d6.p[i]);
    }
    Pmf all = clean;  // d7 entering the shrink chain
    if (!gd6.empty()) {
      double total = 0;
      for (double v : gd6.p) total += v;
      Pmf pre;       // d6' = d6 + R16
      for (size_t i = 0; i < gd6.p.size(); i++) {
        double w = gd6.p[i];
        if (w == 0) continue;
        for (int r = 0; r < 16; r++) pre.add(gd6.lo + int(i) + r, w / 16.0);
      }
      (void)total;  // the d7 == 0 quarter simply drops out (a miss)
      for (size_t i = 0; i < pre.p.size(); i++) {
        double m1 = pre.p[i];
        if (m1 == 0) continue;
        int d6p = pre.lo + int(i);
        double ent = d6p > 0 ? 1.0 : 0.5;
        for (int a = 1; a <= 3; a++) {
          double pa = m1 * 0.25;
          all.add(a, pa * (1.0 - ent));
          double pe = pa * ent;
          for (int b = 0; b < 4; b++) {
            double pb = pe * 0.25;
            all.add(a + b, pb * 0.75);
            double pq = pb * 0.25 / 16.0;
            for (int r = 0; r < 16; r++) all.add(a + b + std::max(0, r + d6p), pq);
          }
        }
      }
    }
    // ---- shrink chain (Attack.cpp:810-844)
    std::vector<double> hmass;
    for (size_t i = 0; i < all.p.size(); i++) {
      if (all.p[i] == 0) continue;
      int v = all.lo + int(i);
      if (v < 0) v = 0;  // cannot happen (d7 >= 1 here)
      size_t h = size_t(v / 2);
      if (hmass.size() <= h) hmass.resize(h + 1, 0.0);
      hmass[h] += all.p[i];
    }
    Pmf emix;                                  // e = h + R4 + R(h)
    for (size_t h = 0; h < hmass.size(); h++) {
      double w = hmass[h];
      if (w == 0) continue;
      if (h == 0) {
        for (int j = 0; j < 4; j++) emix.add(j, w / 4.0);
      } else {
        const auto &wt = rolls_.get(int(h));
        for (int j = 0; j < 4; j++)
          for (int cc = 0; cc < int(h); cc++)
            emix.add(int(h) + j + cc, w / 4.0 * wt[size_t(cc)]);
      }
    }
    std::vector<double> kmass;                 // (e + R(e)) / 4
    for (size_t i = 0; i < emix.p.size(); i++) {
      double w = emix.p[i];
      if (w == 0) continue;
      int e = emix.lo + int(i);
      if (e == 0) {
        if (kmass.empty()) kmass.resize(1, 0.0);
        kmass[0] += w;
        continue;
      }
      const auto &wt = rolls_.get(e);
      for (int r = 0; r < e; r++) {
        size_t k = size_t((e + r) / 4);
        if (kmass.size() <= k) kmass.resize(k + 1, 0.0);
        kmass[k] += w * wt[size_t(r)];
      }
    }
    Pmf fin;                                   // + R4 + 1
    for (size_t k = 0; k < kmass.size(); k++)
      if (kmass[k] != 0)
        for (int j = 0; j < 4; j++) fin.add(int(k) + j + 1, kmass[k] / 4.0);
    // ---- Vorpal halving, mastery +10
    const bool vorp = w_.vorpal && !m.nonMaterial;
    double pm = std::min(std::max(ch.mastery[a_.skill], 0), 64) / 64.0;
    for (size_t i = 0; i < fin.p.size(); i++) {
      double w = fin.p[i];
      if (w == 0) continue;
      int d7 = fin.lo + int(i);
      if (vorp) {
        d7 /= 2;
        if (d7 == 0) continue;
      }
      size_t top = size_t(d7 + 10);
      if (c.hit.size() <= top) c.hit.resize(top + 1, 0.0);
      if (pm > 0) c.hit[size_t(d7 + 10)] += w * pm;
      if (pm < 1.0) c.hit[size_t(d7)] += w * (1.0 - pm);
    }
    // ---- expectations given pass
    double pHit = 0, eD = 0, eD2 = 0, eXp = 0;
    c.minHit = c.maxHit = 0;
    bool first = true;
    for (size_t d = 0; d < c.hit.size(); d++) {
      double w = c.hit[d];
      if (w <= 0) continue;
      pHit += w;
      eD += d * w;
      eD2 += double(d) * double(d) * w;
      int g = a_.exp + (int(d) * m.exp) / 16 + 3;
      if (mult_ != 0) g *= mult_;
      eXp += w * g * 2;
      if (first) c.minHit = int(d);
      c.maxHit = int(d);
      first = false;
    }
    c.pass.pHit = pHit;
    c.pass.eDmg = eD;
    c.pass.eDmg2 = eD2;
    c.pass.eTicks = a_.time * pHit + a_.time / 2.0 * (1.0 - pHit);
    c.pass.eXp = eXp + (1.0 - pHit) * missXp_;
    // cost: hit -> energy + R2 + 4..7, miss -> energy + R2 + 2..3
    std::map<int, double> cm;
    for (int r = 0; r < 2; r++) {
      for (int s = 4; s <= 7; s++) cm[energy + r + s] += pHit * 0.125;
      for (int s = 2; s <= 3; s++) cm[energy + r + s] += (1.0 - pHit) * 0.25;
    }
    double eCost = 0;
    for (auto &kv : cm) {
      c.costPass.push_back({kv.first, kv.second});
      eCost += kv.first * kv.second;
    }
    c.pass.eCost = eCost;
    return c;
  }

  ExactParams p_;
  Weapon w_;
  Attack a_;
  RollTables rolls_;
  std::unordered_map<int, Core> cache_;
  int mult_ = 1, need_ = 0;
  double missXp_ = 0, p1_ = 0;
  bool fixedFlag_ = false;
  std::vector<std::pair<int, double>> missCost_;
  Metrics missM_;
};

// ------------------------------------------------------- fight propagation
struct Totals {
  double pHit = 0, eDmg = 0, eDmg2 = 0, eCost = 0, eTicks = 0, eXp = 0;
  std::vector<double> dmg;      // expected number of hits per damage value
  int lo = -1, hi = -1;
  double endSta = 0;            // sum over fights of E[end stamina]
  void addScaled(const Totals &o, double f)
  {
    pHit += f * o.pHit; eDmg += f * o.eDmg; eDmg2 += f * o.eDmg2;
    eCost += f * o.eCost; eTicks += f * o.eTicks; eXp += f * o.eXp;
    if (dmg.size() < o.dmg.size()) dmg.resize(o.dmg.size(), 0.0);
    for (size_t i = 0; i < o.dmg.size(); i++) dmg[i] += f * o.dmg[i];
    if (o.lo >= 0) {
      lo = lo < 0 ? o.lo : std::min(lo, o.lo);
      hi = hi < 0 ? o.hi : std::max(hi, o.hi);
    }
    endSta += f * o.endSta;
  }
};

class Fight {
 public:
  Fight(Model &m) : m_(m)
  {
    const ExactParams &p = m.params();
    drain_ = p.opt.staminaDrain;
    drift_ = p.opt.luckDrift && !m.fixed();
    start_ = std::min(p.ch.stamina, p.ch.maxStamina);
    l0_ = p.ch.luck;
    lmin_ = p.ch.luckMin;
    lmax_ = p.ch.luckMax;
    sN_ = drain_ ? start_ + 1 : 1;
    lN_ = drift_ ? std::max(lmax_, l0_) + 1 : 1;
    p1_ = m.p1();
    if (drift_) {
      p2_.resize(size_t(lN_));
      for (int l = 0; l < lN_; l++) p2_[size_t(l)] = m.p2(l);
    } else {
      p2f_ = m.p2(l0_);
    }
  }

  // Propagate `steps` swings from the start state.
  Totals run(int64_t steps)
  {
    Totals tot;
    std::vector<double> cur(size_t(sN_) * size_t(lN_), 0.0), nxt = cur;
    int sLo = sIdx(start_), sHi = sLo;
    int lLo = drift_ ? l0_ : 0, lHi = lLo;
    cur[idx(sIdx(start_), lLo)] = 1.0;
    int64_t n = 0;
    while (n < steps) {
      accumulate(cur, sLo, sHi, lLo, lHi, 1.0, tot);
      std::fill(nxt.begin(), nxt.end(), 0.0);  // (sized to the active part below)
      int nsLo = sN_, nsHi = -1, nlLo = lN_, nlHi = -1;
      step(cur, nxt, sLo, sHi, lLo, lHi, nsLo, nsHi, nlLo, nlHi);
      n++;
      // converged?
      double diff = 0;
      int a0 = std::min(sLo, nsLo), a1 = std::max(sHi, nsHi);
      int b0 = std::min(lLo, nlLo), b1 = std::max(lHi, nlHi);
      for (int s = a0; s <= a1; s++)
        for (int l = b0; l <= b1; l++)
          diff = std::max(diff, std::fabs(nxt[idx(s, l)] - cur[idx(s, l)]));
      cur.swap(nxt);
      sLo = nsLo; sHi = nsHi; lLo = nlLo; lHi = nlHi;
      if (n < steps && (diff < 1e-15 || (n > 20000 && diff < 1e-11))) {
        accumulate(cur, sLo, sHi, lLo, lHi, double(steps - n), tot);
        n = steps;
      }
    }
    double e = 0;
    for (int s = sLo; s <= sHi; s++)
      for (int l = lLo; l <= lHi; l++) e += cur[idx(s, l)] * (drain_ ? s : start_);
    tot.endSta = e;
    return tot;
  }

 private:
  size_t idx(int s, int l) const { return size_t(s) * size_t(lN_) + size_t(l); }
  int sIdx(int stamina) const { return drain_ ? stamina : 0; }
  int stam(int sIndex) const { return drain_ ? sIndex : start_; }
  int clampL(int v) const { return v <= lmin_ ? lmin_ : (v > lmax_ ? lmax_ : v); }

  struct W { double stay, down, miss; };
  W weights(const Core &c, int l) const
  {
    if (c.fixed) return {1.0, 0.0, 0.0};
    double chk = c.g1 * 0.75;
    double p2 = drift_ ? p2_[size_t(l)] : p2f_;
    double stay = (1.0 - c.g1) + c.g1 * 0.25 + chk * p1_;
    double rest = chk * (1.0 - p1_);
    double up = rest * p2, miss = rest * (1.0 - p2);
    if (drift_) return {stay, up, miss};
    return {stay + up, 0.0, miss};
  }

  void accumulate(const std::vector<double> &cur, int sLo, int sHi, int lLo,
                  int lHi, double f, Totals &tot)
  {
    const Metrics &mm = m_.missMetrics();
    for (int s = sLo; s <= sHi; s++) {
      const Core *c = nullptr;
      double sumPass = 0, sumMiss = 0;
      for (int l = lLo; l <= lHi; l++) {
        double m = cur[idx(s, l)];
        if (m <= 0) continue;
        if (!c) c = &m_.core(stam(s));
        W w = weights(*c, l);
        sumPass += m * (1.0 - w.miss);
        sumMiss += m * w.miss;
      }
      if (!c) continue;
      sumPass *= f;
      sumMiss *= f;
      tot.pHit += sumPass * c->pass.pHit;
      tot.eDmg += sumPass * c->pass.eDmg;
      tot.eDmg2 += sumPass * c->pass.eDmg2;
      tot.eCost += sumPass * c->pass.eCost + sumMiss * mm.eCost;
      tot.eTicks += sumPass * c->pass.eTicks + sumMiss * mm.eTicks;
      tot.eXp += sumPass * c->pass.eXp + sumMiss * mm.eXp;
      if (tot.dmg.size() < c->hit.size()) tot.dmg.resize(c->hit.size(), 0.0);
      for (size_t d = 0; d < c->hit.size(); d++) tot.dmg[d] += sumPass * c->hit[d];
      if (sumPass > 0 && c->pass.pHit > 0) {
        tot.lo = tot.lo < 0 ? c->minHit : std::min(tot.lo, c->minHit);
        tot.hi = tot.hi < 0 ? c->maxHit : std::max(tot.hi, c->maxHit);
      }
    }
  }

  void put(std::vector<double> &nxt, int s, int l, double v, int &sLo,
           int &sHi, int &lLo, int &lHi)
  {
    if (v <= 0) return;
    nxt[idx(s, l)] += v;
    sLo = std::min(sLo, s); sHi = std::max(sHi, s);
    lLo = std::min(lLo, l); lHi = std::max(lHi, l);
  }

  void step(const std::vector<double> &cur, std::vector<double> &nxt, int sLo,
            int sHi, int lLo, int lHi, int &nsLo, int &nsHi, int &nlLo,
            int &nlHi)
  {
    for (int s = sLo; s <= sHi; s++) {
      const Core *c = nullptr;
      for (int l = lLo; l <= lHi; l++) {
        double m = cur[idx(s, l)];
        if (m <= 1e-300) continue;
        if (!c) c = &m_.core(stam(s));
        W w = weights(*c, l);
        int ld = drift_ ? clampL(l - 2) : 0, lu = drift_ ? clampL(l + 2) : 0;
        int ls = drift_ ? l : 0;
        if (!drain_) {
          put(nxt, s, ls, m * w.stay, nsLo, nsHi, nlLo, nlHi);
          put(nxt, s, ld, m * w.down, nsLo, nsHi, nlLo, nlHi);
          put(nxt, s, lu, m * w.miss, nsLo, nsHi, nlLo, nlHi);
          continue;
        }
        double pass_stay = m * w.stay, pass_down = m * w.down;
        for (auto &cp : c->costPass) {
          int s2 = std::max(s - cp.first, 0);
          put(nxt, s2, ls, pass_stay * cp.second, nsLo, nsHi, nlLo, nlHi);
          put(nxt, s2, ld, pass_down * cp.second, nsLo, nsHi, nlLo, nlHi);
        }
        for (auto &cp : m_.missCost()) {
          int s2 = std::max(s - cp.first, 0);
          put(nxt, s2, lu, m * w.miss * cp.second, nsLo, nsHi, nlLo, nlHi);
        }
      }
    }
  }

  Model &m_;
  bool drain_, drift_;
  int start_, l0_, lmin_, lmax_, sN_, lN_;
  double p1_, p2f_ = 0;
  std::vector<double> p2_;
};

}  // namespace

ComboResult exactCombo(const ExactParams &p, const Weapon &w, const Attack &a)
{
  Model model(p, w, a);
  Fight fight(model);
  const Options &o = p.opt;
  const bool drift = o.luckDrift && !model.fixed();
  const int F = o.swingsPerFight;
  const bool structured = F > 0 && (o.staminaDrain || drift);
  Totals tot;
  double fights = 1;
  if (structured) {
    int64_t full = p.trials / F, rem = p.trials % F;
    if (full > 0) tot.addScaled(fight.run(F), double(full));
    if (rem > 0) tot.addScaled(fight.run(rem), 1.0);
    fights = double(full + (rem ? 1 : 0));
  } else {
    tot = fight.run(p.trials);
  }
  ComboResult r;
  r.valid = true;
  r.structured = structured;
  r.singleRun = !structured && (o.staminaDrain || drift);
  r.trials = p.trials;
  const double n = double(p.trials);
  Row &row = r.row;
  row.combo = std::string(w.name) + "+" + a.name;
  row.weapon = w.name;
  row.attack = a.name;
  row.hitPct = n ? 100.0 * tot.pHit / n : 0;
  row.dmgPerTry = n ? tot.eDmg / n : 0;
  row.dmgPerHit = tot.pHit > 0 ? tot.eDmg / tot.pHit : 0;
  row.minHit = tot.lo < 0 ? 0 : tot.lo;
  row.maxHit = tot.hi < 0 ? 0 : tot.hi;
  row.dps = tot.eTicks ? 6 * tot.eDmg / tot.eTicks : 0;
  row.dmgPerSta = tot.eCost ? tot.eDmg / tot.eCost : 0;
  row.staPerTry = n ? tot.eCost / n : 0;
  row.xpPerTry = n ? tot.eXp / n : 0;
  row.xps = tot.eTicks ? 6 * tot.eXp / tot.eTicks : 0;
  row.endStamina = tot.endSta / fights;
  Means &m = r.means;
  m.hit.v = n ? tot.pHit / n : 0;
  m.dmg.v = row.dmgPerTry;
  m.cost.v = row.staPerTry;
  m.ticks.v = n ? tot.eTicks / n : 0;
  m.xp.v = row.xpPerTry;
  m.endSta.v = row.endStamina;
  r.dmgFraction.assign(tot.dmg.size(), 0.0);
  double hits = 0;
  for (size_t d = 0; d < tot.dmg.size(); d++) {
    r.dmgFraction[d] = n ? tot.dmg[d] / n : 0;
    hits += r.dmgFraction[d];
  }
  if (r.dmgFraction.empty()) r.dmgFraction.assign(1, 0.0);
  r.dmgFraction[0] = std::max(0.0, 1.0 - hits);
  return r;
}

double exactDps(const Character &ch, const Weapon &w, const Attack &a,
                const Monster &m, const Options &opt)
{
  ExactParams p;
  p.ch = ch;
  p.mon = m;
  p.opt = opt;
  p.opt.luckDrift = false;
  p.opt.staminaDrain = false;
  p.trials = 1;
  ComboResult r = exactCombo(p, w, a);
  return r.row.dps;
}

std::vector<ComboResult> exactAll(
    const ExactParams &p, const std::vector<std::pair<int, int>> &combos,
    int threads)
{
  std::vector<ComboResult> out(combos.size());
  int n = threads > 0 ? threads : int(std::thread::hardware_concurrency());
  n = std::max(1, std::min<int>(n, int(combos.size())));
  std::atomic<size_t> next{0};
  auto work = [&]() {
    for (size_t i; (i = next.fetch_add(1)) < combos.size();)
      out[i] = exactCombo(p, weapons()[size_t(combos[i].first)],
                          attacks()[size_t(combos[i].second)]);
  };
  std::vector<std::thread> pool;
  for (int i = 1; i < n; i++) pool.emplace_back(work);
  work();
  for (auto &t : pool) t.join();
  return out;
}
