// A probability table over integers.  Every random step of the engine is a
// roll over a small range, so a whole formula's distribution is obtained by
// carrying such a table through the same steps in the same order.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace calc {

struct Dist {
  int lo = 0;
  std::vector<double> p;

  bool empty() const { return p.empty(); }
  int hi() const { return lo + int(p.size()) - 1; }

  void add(int v, double w)
  {
    if (w == 0.0) return;
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
  void add(const Dist &o, double f = 1.0)
  {
    for (size_t i = 0; i < o.p.size(); i++) add(o.lo + int(i), o.p[i] * f);
  }
  double at(int v) const { return (v < lo || v > hi()) ? 0.0 : p[size_t(v - lo)]; }

  static Dist point(int v, double w = 1.0)
  {
    Dist d;
    d.add(v, w);
    return d;
  }

  double mass() const
  {
    double s = 0;
    for (double v : p) s += v;
    return s;
  }
  // Mean of the values, not divided by the mass
  double sum() const
  {
    double s = 0;
    for (size_t i = 0; i < p.size(); i++) s += p[i] * double(lo + int(i));
    return s;
  }
  double mean() const
  {
    double m = mass();
    return m > 0 ? sum() / m : 0.0;
  }
  double sumSquares() const
  {
    double s = 0;
    for (size_t i = 0; i < p.size(); i++) s += p[i] * double(lo + int(i)) * double(lo + int(i));
    return s;
  }
  // Smallest and largest value with a chance
  int minValue() const
  {
    for (size_t i = 0; i < p.size(); i++)
      if (p[i] > 0) return lo + int(i);
    return 0;
  }
  int maxValue() const
  {
    for (size_t i = p.size(); i-- > 0;)
      if (p[i] > 0) return lo + int(i);
    return 0;
  }
  // P(value >= v)
  double atLeast(int v) const
  {
    double s = 0;
    for (size_t i = 0; i < p.size(); i++)
      if (lo + int(i) >= v) s += p[i];
    return s;
  }
  // Smallest v with P(value <= v) >= q of the mass
  int quantile(double q) const
  {
    double need = q * mass(), s = 0;
    for (size_t i = 0; i < p.size(); i++) {
      s += p[i];
      if (p[i] > 0 && s >= need - 1e-12) return lo + int(i);
    }
    return maxValue();
  }
  void scale(double f)
  {
    for (double &v : p) v *= f;
  }
  Dist scaled(double f) const
  {
    Dist d = *this;
    d.scale(f);
    return d;
  }
  void trim()
  {
    while (!p.empty() && p.back() == 0.0) p.pop_back();
    size_t lead = 0;
    while (lead < p.size() && p[lead] == 0.0) lead++;
    if (lead) {
      p.erase(p.begin(), p.begin() + long(lead));
      lo += int(lead);
    }
  }
};

// Each value v with weight w is handed to f(v, w, out)
template <class F>
Dist bind(const Dist &d, F f)
{
  Dist out;
  for (size_t i = 0; i < d.p.size(); i++)
    if (d.p[i] != 0.0) f(d.lo + int(i), d.p[i], out);
  return out;
}

// Each value v becomes f(v)
template <class F>
Dist mapValues(const Dist &d, F f)
{
  return bind(d, [&](int v, double w, Dist &out) { out.add(f(v), w); });
}

// The sum of two independent tables
inline Dist convolve(const Dist &a, const Dist &b)
{
  Dist out;
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

// The engine's dice.  STRandom(n) is (r & 0xffff) % n with r uniform, which
// favours the low values a little when n does not divide 65536.
class Dice {
 public:
  explicit Dice(bool moduloBias = true) : bias_(moduloBias) {}
  bool bias() const { return bias_; }
  // P(STRandom(n) == i) for i in 0..n-1; n >= 1
  const std::vector<double> &roll(int n)
  {
    if (n < 1) n = 1;
    if (int(cache_.size()) <= n) cache_.resize(size_t(n) + 1);
    auto &v = cache_[size_t(n)];
    if (v.empty()) {
      v.resize(size_t(n));
      for (int i = 0; i < n; i++)
        v[size_t(i)] = bias_ ? double((65535 - i) / n + 1) / 65536.0 : 1.0 / n;
    }
    return v;
  }
  Dist dist(int n, int offset = 0)
  {
    Dist d;
    const auto &t = roll(n);
    d.lo = offset;
    d.p.assign(t.begin(), t.end());
    return d;
  }
  // d + STRandom(n)
  Dist plusRoll(const Dist &d, int n) { return convolve(d, dist(n)); }
  // v + STRandom(f(v)) for each v, f(v) >= 1
  template <class F>
  Dist plusRollOf(const Dist &d, F f)
  {
    return bind(d, [&](int v, double w, Dist &out) {
      int n = f(v);
      const auto &t = roll(n);
      for (int r = 0; r < n; r++) out.add(v + r, w * t[size_t(r)]);
    });
  }

 private:
  bool bias_;
  std::vector<std::vector<double>> cache_;
};

// Summary numbers of a damage table whose missing mass is "no damage"
struct Summary {
  double chance = 0;   // P(damage > 0)
  double mean = 0;     // average over the hits
  double perTry = 0;   // average over all attempts
  int min = 0, max = 0;
  int p05 = 0, median = 0, p95 = 0;
};
inline Summary summarize(const Dist &hits)
{
  Summary s;
  Dist d;
  for (size_t i = 0; i < hits.p.size(); i++)
    if (hits.lo + int(i) > 0) d.add(hits.lo + int(i), hits.p[i]);
  s.chance = d.mass();
  if (s.chance <= 0) return s;
  s.mean = d.mean();
  s.perTry = d.sum();
  s.min = d.minValue();
  s.max = d.maxValue();
  s.p05 = d.quantile(0.05);
  s.median = d.quantile(0.5);
  s.p95 = d.quantile(0.95);
  return s;
}

}  // namespace calc
