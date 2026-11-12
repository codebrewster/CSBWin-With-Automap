#include "HeroAttack.h"

#include <map>
#include <unordered_map>
#include <utility>

namespace calc {

// ------------------------------------------------------------ primitives
namespace eng {

int staminaAdjusted(const Hero &h, int stamina, int base)
{
  int half = h.maxStamina / 2;
  if (stamina < half) {
    int b = base / 2;
    return b + (half ? (b * stamina) / half : 0);
  }
  return base;
}

int maxLoad(const Hero &h, int stamina)
{
  int ml = 8 * h.strength() + 100;
  ml = staminaAdjusted(h, stamina, ml);
  if (h.wounds) {                          // any wound: -1/8, legs: -1/4
    int sh = (h.wounds & WoundLegs) ? 2 : 3;
    ml = ml - (ml >> sh);
  }
  if (h.elvenBoots) ml = ml + (ml >> 4);
  ml += 9;
  return ml - ml % 10;
}

int quickness(const Hero &h, int ml, int a, int b, int c)
{
  int q = h.dexterity() + a;
  q = q - ((q / 2) * h.load) / (ml ? ml : 1);
  q /= 2;
  int lo = b + 1, hi = 100 - c;
  if (q <= lo) return lo;
  if (q > hi) return hi;
  return q;
}

int throwingDistance(const Hero &h, const WeaponType &w, int hand, int ml, int stamina,
                     int roll16)
{
  int d7 = h.strength() + roll16;
  int d6 = w.weight;
  int d5 = ml / 16;
  if (d6 <= d5) {
    d7 = d7 + d6 - 12;
  } else {
    int w2 = (d5 - 12) / 2 + d5;
    if (d6 <= w2) d7 = d7 + (d6 - d5) / 2;
    else d7 = d7 - 2 * (d6 - w2);
  }
  if (w.id >= 0) {                         // a weapon: its strength and the skills behind it
    d7 += w.strength;
    d6 = 0;
    int u1 = w.cls;
    if (u1 == 0 || u1 == 2) d6 = h.mastery[SkillSwing];
    if (u1 != 0 && u1 < 16) d6 += h.mastery[SkillThrow];
    if (u1 >= 16 && u1 < 112) d6 += h.mastery[SkillShoot];
    d7 += 2 * d6;
  }
  d7 = staminaAdjusted(h, stamina, d7);
  if (h.wounds & (hand == 0 ? WoundReadyHand : WoundActionHand)) d7 /= 2;
  int r = d7 / 2;
  return r <= 0 ? 0 : (r > 100 ? 100 : r);
}

int armorTerm(const Target &t, const WeaponType &w, bool halved, int r32)
{
  int d4 = r32 + t.type.defense + t.level;
  if (halved) d4 /= 2;
  if (w.id == WeaponDiamondEdge) d4 = d4 - d4 / 4;
  else if (w.id == WeaponExecutioner) d4 = d4 - d4 / 8;
  return d4;
}

}  // namespace eng

// ------------------------------------------------------------ one swing with dice
SwingSample sampleSwing(Draw &d, const Hero &ch, const WeaponType &w, const ActionType &a,
                        const Target &t, const Rules &rules, bool staminaDrain, SwingState &st)
{
  const MonsterType &m = t.type;
  const bool disrupt = a.id == ActionDisrupt;
  const bool vorpal = w.id == WeaponVorpalBlade;
  const bool halved = rules.vulnerableOnAttack && t.attacking;
  SwingSample res;
  auto finish = [&](int sadj) {
    int cost = a.energy + d.bit() + sadj;
    if (staminaDrain) st.stamina = st.stamina > cost ? st.stamina - cost : 0;
    return cost;
  };
  // Non-material beings: only the Vorpal Blade and Disrupt reach the damage
  // code; anything else spends 2 + STRandomBool (Attack.cpp:948).
  if (m.nonMaterial && !(vorpal || disrupt)) {
    res.cost = finish(2 + d.bit());
    return res;
  }
  if (disrupt && !m.nonMaterial) {   // Disrupt fizzles (Attack.cpp:1117)
    res.cost = finish(0);
    return res;
  }
  const int ml = eng::maxLoad(ch, st.stamina);
  // ---- to-hit gate (Attack.cpp:599-635)
  int qa = d.rnd(8);
  int qb = d.rnd(8), qc = d.rnd(8);
  int q = eng::quickness(ch, ml, qa, qb, qc);
  int need = d.rnd(32) + m.dexterity + t.level - 16;
  if (q <= need) {
    bool lucky = (d.r03() == 0);
    if (!lucky) {
      // IsCharacterLucky(75 - P7)  (Character.cpp:2156)
      int luckNeeded = 75 - a.hitChance;
      bool ok = false;
      if (d.bit() && d.rnd(100) > luckNeeded) {
        ok = true;
      } else {
        int cur = st.luck;
        int roll = cur == 0 ? 0 : d.rnd(cur);
        ok = roll > luckNeeded;
        if (rules.luckDrift) {
          int v = cur + (ok ? -2 : 2);
          st.luck = v <= ch.attrMin[Luck] ? ch.attrMin[Luck]
                                          : (v > ch.attrMax[Luck] ? ch.attrMax[Luck] : v);
        }
      }
      if (!ok) {
        res.cost = finish(2 + d.bit());
        return res;
      }
    }
  }
  // ---- power (Attack.cpp:653-713)
  int thr = eng::throwingDistance(ch, w, 1, ml, st.stamina, d.low(15));
  int d7, d6;
  if (thr != 0) {
    d7 = thr + d.rnd(thr / 2 + 1);
    d7 = d7 * a.damage / 32;
    int d4 = eng::armorTerm(t, w, halved, d.rnd(32));
    d7 = d7 + d.rnd(32) - d4;
    d6 = d7;
  } else {
    d6 = 0;
    d7 = 0;
  }
  if (thr == 0 || d6 <= 1) {   // graze chain (Attack.cpp:720-809)
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
  if (vorpal && !m.nonMaterial) {   // Attack.cpp:856
    d7 /= 2;
    if (d7 == 0) {
      res.cost = finish(2 + d.bit());
      return res;
    }
  }
  if (d.rnd(64) < ch.mastery[a.skill]) d7 += 10;   // Attack.cpp:884
  if (d7 < 0) d7 = 0;
  res.cost = finish(4 + d.r03());   // Attack.cpp:925, every hit
  res.damage = d7;
  res.hit = true;
  return res;
}

namespace {

// ------------------------------------------------------------ exact: one swing
struct Metrics {
  double pHit = 0, eDmg = 0, eCost = 0, eTicks = 0, eXp = 0;
};

struct Core {                 // one swing at one stamina class
  bool fixed = false;         // no to-hit gate (non-material / fizzling Disrupt)
  double g1 = 0;              // P(quickness <= needed)
  std::vector<double> hit;    // P(damage = d | gate passed)
  std::vector<std::pair<int, double>> costPass;
  Metrics pass;               // expectations given the gate was passed
  int minHit = 0, maxHit = 0;
};

struct Setup {
  Hero ch;
  WeaponType w;
  ActionType a;
  Target t;
  Rules rules;
  SwingOptions opt;
};

class Model {
 public:
  explicit Model(const Setup &s) : s_(s), dice_(s.rules.moduloBias)
  {
    const ActionType &a = s.a;
    const MonsterType &m = s.t.type;
    mult_ = s.t.level;
    missXp_ = a.exp / 2;
    if (mult_ != 0) missXp_ *= mult_;
    missXp_ *= 2;
    // gate-miss: energy + R2 + 2 + R2
    for (int r = 0; r < 2; r++)
      for (int k = 2; k <= 3; k++) missCost_.push_back({a.energy + r + k, 0.25});
    missM_.eCost = a.energy + 0.5 + 2.5;
    missM_.eTicks = swingPeriod(a.ticks);  // the monster was attacked: full delay
    missM_.eXp = missXp_;
    // luck: IsCharacterLucky(75 - P7)
    need_ = 75 - a.hitChance;
    const auto &w100 = dice_.roll(100);
    double sum = 0;
    for (int v = 0; v < 100; v++)
      if (v > need_) sum += w100[size_t(v)];
    p1_ = 0.5 * sum;
    const bool disrupt = a.id == ActionDisrupt;
    const bool vorpal = s.w.id == WeaponVorpalBlade;
    fixedFlag_ = (m.nonMaterial && !(vorpal || disrupt)) || (disrupt && !m.nonMaterial);
  }

  const Core &core(int stamina)
  {
    int key = stamina < s_.ch.maxStamina / 2 ? stamina : -1;
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    return cache_.emplace(key, buildCore(stamina)).first->second;
  }

  double p2(int luck)       // P(STRandom(luck) > need)
  {
    if (luck <= 0) return 0 > need_ ? 1.0 : 0.0;
    const auto &t = dice_.roll(luck);
    double sum = 0;
    for (int v = 0; v < luck; v++)
      if (v > need_) sum += t[size_t(v)];
    return sum;
  }
  double p1() const { return p1_; }
  const std::vector<std::pair<int, double>> &missCost() const { return missCost_; }
  const Metrics &missMetrics() const { return missM_; }
  bool fixed() const { return fixedFlag_; }
  const Setup &setup() const { return s_; }

 private:
  Core buildCore(int stamina)
  {
    Core c;
    const Hero &ch = s_.ch;
    const ActionType &a = s_.a;
    const MonsterType &m = s_.t.type;
    const int energy = a.energy;
    const bool disrupt = a.id == ActionDisrupt;
    if (fixedFlag_) {
      c.fixed = true;
      c.g1 = 0;
      if (disrupt && !m.nonMaterial) {
        for (int r = 0; r < 2; r++) c.costPass.push_back({energy + r, 0.5});
        c.pass.eCost = energy + 0.5;
      } else {
        for (int r = 0; r < 2; r++)
          for (int k = 2; k <= 3; k++) c.costPass.push_back({energy + r + k, 0.25});
        c.pass.eCost = energy + 0.5 + 2.5;
      }
      c.pass.eTicks = swingPeriod(a.ticks, disrupt && !m.nonMaterial);  // Disrupt on a material monster attacks nothing
      c.pass.eXp = missXp_;
      return c;
    }
    const int ml = eng::maxLoad(ch, stamina);
    // ---- gate: P(q <= need), need = dex + R32 + level - 16
    std::map<int, double> qp;
    for (int x = 0; x < 8; x++)
      for (int y = 0; y < 8; y++)
        for (int z = 0; z < 8; z++)
          qp[eng::quickness(ch, ml, x, y, z)] += dice_.roll(8)[size_t(x)] *
                                                 dice_.roll(8)[size_t(y)] *
                                                 dice_.roll(8)[size_t(z)];
    int base = m.dexterity + s_.t.level - 16;
    const auto &w32 = dice_.roll(32);
    double ple = 0;
    for (auto &kv : qp)
      for (int r = 0; r < 32; r++)
        if (kv.first <= base + r) ple += kv.second * w32[size_t(r)];
    c.g1 = ple;
    // ---- power: d6 before the graze test
    double t0 = 0;
    Dist amix;
    for (int r = 0; r < 16; r++) {
      int t = eng::throwingDistance(ch, s_.w, 1, ml, stamina, r);
      if (t == 0) {
        t0 += 1.0 / 16;
        continue;
      }
      int n = t / 2 + 1;
      const auto &wt = dice_.roll(n);
      for (int rr = 0; rr < n; rr++)
        amix.add((t + rr) * a.damage / 32, wt[size_t(rr)] / 16.0);
    }
    const bool halved = s_.rules.vulnerableOnAttack && s_.t.attacking;
    Dist x;
    for (int r4 = 0; r4 < 32; r4++) {
      int d4 = eng::armorTerm(s_.t, s_.w, halved, r4);
      for (int r = 0; r < 32; r++) x.add(r - d4, w32[size_t(r4)] * w32[size_t(r)]);
    }
    Dist d6 = convolve(amix, x);
    // ---- split: d6 <= 1 (or t == 0) goes through the graze chain
    Dist gd6, clean;
    if (t0 > 0) gd6.add(0, t0);
    for (size_t i = 0; i < d6.p.size(); i++) {
      int v = d6.lo + int(i);
      if (d6.p[i] == 0) continue;
      if (v <= 1) gd6.add(v, d6.p[i]);
      else clean.add(v, d6.p[i]);
    }
    Dist all = clean;  // d7 entering the shrink chain
    if (!gd6.empty()) {
      Dist pre = dice_.plusRoll(gd6, 16);   // d6' = d6 + R16
      // the d7 == 0 quarter simply drops out (a miss)
      const auto &w16 = dice_.roll(16);
      for (size_t i = 0; i < pre.p.size(); i++) {
        double m1 = pre.p[i];
        if (m1 == 0) continue;
        int d6p = pre.lo + int(i);
        double ent = d6p > 0 ? 1.0 : 0.5;
        for (int g = 1; g <= 3; g++) {
          double pa = m1 * 0.25;
          all.add(g, pa * (1.0 - ent));
          double pe = pa * ent;
          for (int b = 0; b < 4; b++) {
            double pb = pe * 0.25;
            all.add(g + b, pb * 0.75);
            for (int r = 0; r < 16; r++)
              all.add(g + b + std::max(0, r + d6p), pb * 0.25 * w16[size_t(r)]);
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
    Dist emix;                                  // e = h + R4 + R(h)
    for (size_t h = 0; h < hmass.size(); h++) {
      double w = hmass[h];
      if (w == 0) continue;
      if (h == 0) {
        for (int j = 0; j < 4; j++) emix.add(j, w / 4.0);
      } else {
        const auto &wt = dice_.roll(int(h));
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
      const auto &wt = dice_.roll(e);
      for (int r = 0; r < e; r++) {
        size_t k = size_t((e + r) / 4);
        if (kmass.size() <= k) kmass.resize(k + 1, 0.0);
        kmass[k] += w * wt[size_t(r)];
      }
    }
    Dist fin;                                   // + R4 + 1
    for (size_t k = 0; k < kmass.size(); k++)
      if (kmass[k] != 0)
        for (int j = 0; j < 4; j++) fin.add(int(k) + j + 1, kmass[k] / 4.0);
    // ---- Vorpal halving, mastery +10
    const bool vorp = s_.w.id == WeaponVorpalBlade && !m.nonMaterial;
    double pm = 0;
    {
      const auto &w64 = dice_.roll(64);
      int lvl = std::min(std::max(ch.mastery[a.skill], 0), 64);
      for (int r = 0; r < lvl; r++) pm += w64[size_t(r)];
    }
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
    double pHit = 0, eD = 0, eXp = 0;
    c.minHit = c.maxHit = 0;
    bool first = true;
    for (size_t d = 0; d < c.hit.size(); d++) {
      double w = c.hit[d];
      if (w <= 0) continue;
      pHit += w;
      eD += double(d) * w;
      int g = a.exp + (int(d) * m.expClass) / 16 + 3;
      if (mult_ != 0) g *= mult_;
      eXp += w * g * 2;
      if (first) c.minHit = int(d);
      c.maxHit = int(d);
      first = false;
    }
    c.pass.pHit = pHit;
    c.pass.eDmg = eD;
    c.pass.eTicks = swingPeriod(a.ticks);
    c.pass.eXp = eXp + (1.0 - pHit) * missXp_;
    // cost: hit -> energy + R2 + 4..7, miss -> energy + R2 + 2..3
    std::map<int, double> cm;
    for (int r = 0; r < 2; r++) {
      for (int k = 4; k <= 7; k++) cm[energy + r + k] += pHit * 0.125;
      for (int k = 2; k <= 3; k++) cm[energy + r + k] += (1.0 - pHit) * 0.25;
    }
    double eCost = 0;
    for (auto &kv : cm) {
      c.costPass.push_back({kv.first, kv.second});
      eCost += kv.first * kv.second;
    }
    c.pass.eCost = eCost;
    return c;
  }

  Setup s_;
  Dice dice_;
  std::unordered_map<int, Core> cache_;
  int mult_ = 1, need_ = 0;
  double missXp_ = 0, p1_ = 0;
  bool fixedFlag_ = false;
  std::vector<std::pair<int, double>> missCost_;
  Metrics missM_;
};

// ------------------------------------------------------- a fight, swing by swing
// Stamina and luck are a joint Markov state carried from swing to swing.
struct Totals {
  double pHit = 0, eDmg = 0, eCost = 0, eTicks = 0, eXp = 0;
  std::vector<double> dmg;      // expected number of hits per damage value
  int lo = -1, hi = -1;
  double endSta = 0;
};

class Fight {
 public:
  explicit Fight(Model &m) : m_(m)
  {
    const Setup &s = m.setup();
    drain_ = s.opt.staminaDrain;
    drift_ = s.rules.luckDrift && !m.fixed();
    start_ = std::max(0, std::min(s.ch.stamina, s.ch.maxStamina));
    l0_ = std::max(0, s.ch.luck());
    lmin_ = s.ch.attrMin[Luck];
    lmax_ = s.ch.attrMax[Luck];
    sN_ = drain_ ? start_ + 1 : 1;
    lN_ = drift_ ? std::max(std::max(lmax_, lmin_), l0_) + 1 : 1;
    p1_ = m.p1();
    if (drift_) {
      p2_.resize(size_t(lN_));
      for (int l = 0; l < lN_; l++) p2_[size_t(l)] = m.p2(l);
    } else {
      p2f_ = m.p2(l0_);
    }
  }

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
      std::fill(nxt.begin(), nxt.end(), 0.0);
      int nsLo = sN_, nsHi = -1, nlLo = lN_, nlHi = -1;
      step(cur, nxt, sLo, sHi, lLo, lHi, nsLo, nsHi, nlLo, nlHi);
      n++;
      double diff = 0;   // converged?
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

  void accumulate(const std::vector<double> &cur, int sLo, int sHi, int lLo, int lHi, double f,
                  Totals &tot)
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

  void put(std::vector<double> &nxt, int s, int l, double v, int &sLo, int &sHi, int &lLo,
           int &lHi)
  {
    if (v <= 0) return;
    nxt[idx(s, l)] += v;
    sLo = std::min(sLo, s); sHi = std::max(sHi, s);
    lLo = std::min(lLo, l); lHi = std::max(lHi, l);
  }

  void step(const std::vector<double> &cur, std::vector<double> &nxt, int sLo, int sHi, int lLo,
            int lHi, int &nsLo, int &nsHi, int &nlLo, int &nlHi)
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
        double passStay = m * w.stay, passDown = m * w.down;
        for (auto &cp : c->costPass) {
          int s2 = std::max(s - cp.first, 0);
          put(nxt, s2, ls, passStay * cp.second, nsLo, nsHi, nlLo, nlHi);
          put(nxt, s2, ld, passDown * cp.second, nsLo, nsHi, nlLo, nlHi);
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

double meanTriesToReach(const Dist &damage, int hp)
{
  if (hp <= 0) return 0;
  double pHit = 0;
  for (size_t i = 0; i < damage.p.size(); i++)
    if (damage.lo + int(i) > 0) pHit += damage.p[i];
  if (pHit <= 0) return 0;   // never
  std::vector<double> e(size_t(hp) + 1, 0.0);
  for (int h = 1; h <= hp; h++) {
    double s = 1.0;
    for (size_t i = 0; i < damage.p.size(); i++) {
      int d = damage.lo + int(i);
      if (d <= 0 || damage.p[i] == 0) continue;
      if (h - d > 0) s += damage.p[i] * e[size_t(h - d)];
    }
    e[size_t(h)] = s / pHit;
  }
  return e[size_t(hp)];
}

std::vector<double> killCurve(const Dist &damage, int hp, int maxTries)
{
  std::vector<double> out(size_t(maxTries) + 1, 0.0);
  if (hp <= 0) {
    std::fill(out.begin(), out.end(), 1.0);
    return out;
  }
  double pHit = 0;
  for (size_t i = 0; i < damage.p.size(); i++)
    if (damage.lo + int(i) > 0) pHit += damage.p[i];
  std::vector<double> left(size_t(hp) + 1, 0.0), nxt;   // P(hit points left = h), h >= 1
  left[size_t(hp)] = 1.0;
  double dead = 0;
  for (int n = 1; n <= maxTries; n++) {
    nxt.assign(size_t(hp) + 1, 0.0);
    for (int h = 1; h <= hp; h++) {
      double m = left[size_t(h)];
      if (m == 0) continue;
      nxt[size_t(h)] += m * (1.0 - pHit);
      for (size_t i = 0; i < damage.p.size(); i++) {
        int d = damage.lo + int(i);
        if (d <= 0 || damage.p[i] == 0) continue;
        if (h - d > 0) nxt[size_t(h - d)] += m * damage.p[i];
        else dead += m * damage.p[i];
      }
    }
    left.swap(nxt);
    out[size_t(n)] = dead;
  }
  return out;
}

SwingResult heroAttack(const Hero &hero, const WeaponType &weapon, const ActionType &action,
                       const Target &target, const Rules &rules, const SwingOptions &options)
{
  Setup s{hero, weapon, action, target, rules, options};
  if (s.opt.swings < 1) s.opt.swings = 1;
  Model model(s);
  Fight fight(model);
  Totals tot = fight.run(s.opt.swings);
  const double n = double(s.opt.swings);
  SwingResult r;
  r.valid = true;
  r.weapon = weapon.id;
  r.action = action.id;
  r.hitChance = tot.pHit / n;
  r.perSwing = tot.eDmg / n;
  r.perHit = tot.pHit > 0 ? tot.eDmg / tot.pHit : 0;
  r.minHit = tot.lo < 0 ? 0 : tot.lo;
  r.maxHit = tot.hi < 0 ? 0 : tot.hi;
  r.ticks = tot.eTicks / n;
  r.dps = tot.eTicks > 0 ? ticksPerSecond() * tot.eDmg / tot.eTicks : 0;
  r.staminaPerSwing = tot.eCost / n;
  r.damagePerStamina = tot.eCost > 0 ? tot.eDmg / tot.eCost : 0;
  r.expPerSwing = tot.eXp / n;
  r.expPerSecond = tot.eTicks > 0 ? ticksPerSecond() * tot.eXp / tot.eTicks : 0;
  r.endStamina = tot.endSta;
  for (size_t d = 1; d < tot.dmg.size(); d++) r.damage.add(int(d), tot.dmg[d] / n);
  if (target.hp > 0) {
    r.swingsToKill = meanTriesToReach(r.damage, target.hp);
    r.secondsToKill = r.swingsToKill * r.ticks / ticksPerSecond();
  }
  return r;
}

}  // namespace calc
