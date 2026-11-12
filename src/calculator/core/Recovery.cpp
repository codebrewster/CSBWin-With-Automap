#include "Recovery.h"

#include <algorithm>

namespace calc {

const char *restModeName(int mode)
{
  static const char *const names[] = {"Walking", "Standing still", "Sleeping"};
  return mode >= 0 && mode < RestModeCount ? names[mode] : "?";
}

namespace {

int limits(int lo, int x, int hi) { return x <= lo ? lo : (x > hi ? hi : x); }

// AdjustStamina (Code11f52.cpp:4640): what cannot be paid in stamina is paid in hit points
void adjustStamina(RecoveryState &s, const Hero &h, int decrement, int *hpLost)
{
  int ns = s.stamina - decrement;
  if (ns <= 0) {
    s.stamina = 0;
    int harm = (-ns) / 2;
    if (harm > 0) {
      harm = std::min(harm, s.hp);
      s.hp -= harm;
      if (hpLost) *hpLost += harm;
    }
  } else {
    s.stamina = std::min(ns, h.maxStamina);
  }
}

int random112(unsigned t) { return int((t & 0x40) | ((t & 0x80) >> 2) | ((t & 0x100) >> 4)); }

// DetermineMastery gives 1 for every skill while the party sleeps
int magicMastery(const Hero &h, bool sleeping)
{
  return sleeping ? 2 : h.mastery[SkillWizard] + h.mastery[SkillPriest];
}

int manaStep(const Hero &h, bool sleeping)
{
  int adj = h.maxMana / 40;
  if (sleeping) adj *= 2;
  return adj + 1;
}

int hpStep(const Hero &h, bool sleeping)
{
  int l = (h.maxHp >> 7) + 1;
  if (sleeping) l *= 2;
  if (h.ekkhardCross) l = l + l / 2 + 1;
  return l;
}

int staminaStep(const Hero &h, bool sleeping, int still)
{
  int l = limits(1, (h.maxStamina >> 8) - 1, 6);
  if (sleeping) l <<= 1;
  if (still > 80) {
    l++;
    if (still > 250) l++;
  }
  return l;
}

}  // namespace

void tenSecondUpdate(RecoveryState &s, const Hero &h, bool sleeping, unsigned time, int still,
                     int *staminaForMana, int *hpLost)
{
  if (s.hp == 0) return;
  const int r112 = random112(time);
  // ---- mana
  if (s.mana < h.maxMana) {
    int mastery = magicMastery(h, sleeping);
    if (r112 < mastery + h.attr[Wisdom]) {
      int adj = manaStep(h, sleeping);
      int cost = adj * std::max(7, 16 - mastery);
      if (staminaForMana) *staminaForMana += cost;
      adjustStamina(s, h, cost, hpLost);
      s.mana += std::min(adj, h.maxMana - s.mana);
    }
  } else if (s.mana > h.maxMana) {
    s.mana--;
  }
  // ---- stamina, food and water
  int d6 = 4;
  for (int d4 = h.maxStamina; d4 >>= 1, s.stamina < d4; d6 += 2) {
  }
  int d4 = 0;
  const int step = staminaStep(h, sleeping, still);
  do {
    bool last = d6 <= 4;
    if (s.food < -512) {
      if (last) {
        d4 += step;
        s.food -= 2;
      }
    } else {
      if (s.food >= 0) d4 -= step;
      s.food -= last ? 2 : d6 / 2;
    }
    if (s.water < -512) {
      if (last) {
        d4 += step;
        s.water--;
      }
    } else {
      if (s.water >= 0) d4 -= step;
      s.water -= last ? 1 : d6 / 2;
    }
    d6--;
    if (d6 == 0) break;
  } while (s.stamina < h.maxStamina + d4);
  adjustStamina(s, h, d4, hpLost);
  s.food = std::max(s.food, -1024);
  s.water = std::max(s.water, -1024);
  // ---- hit points
  if (s.hp > 0 && s.hp < h.maxHp && s.stamina >= h.maxStamina / 4 &&
      r112 < h.attr[Vitality] + 12)
    s.hp += std::min(hpStep(h, sleeping), h.maxHp - s.hp);
}

RecoveryResult recover(const Hero &h, int mode, unsigned gameTime, int maxTicks)
{
  RecoveryResult r;
  const bool sleeping = mode == RestSleeping;
  const unsigned mask = sleeping ? 15 : 63;
  r.updateEvery = int(mask) + 1;
  RecoveryState s{h.hp, h.stamina, h.mana, h.food, h.water};
  // what an update gives
  r.hpGain = hpStep(h, sleeping);
  r.manaGain = manaStep(h, sleeping);
  r.staminaGain = 2 * staminaStep(h, sleeping, mode == RestWalking ? 0 : 1000);
  r.manaStaminaCost = r.manaGain * std::max(7, 16 - magicMastery(h, sleeping));
  for (int k = 0; k < 8; k++) {
    int v = 16 * k;
    if (v < h.attr[Vitality] + 12) r.hpUpdates++;
    if (v < magicMastery(h, sleeping) + h.attr[Wisdom]) r.manaUpdates++;
  }
  r.hp.needed = s.hp < h.maxHp;
  r.stamina.needed = s.stamina < h.maxStamina;
  r.mana.needed = s.mana < h.maxMana;
  r.all.needed = r.hp.needed || r.stamina.needed || r.mana.needed;
  auto mark = [&](RecoveryGoal &g, bool full, int tick) {
    if (g.reached || !full) return;
    g.reached = true;
    g.ticks = tick;
    g.food = h.food - s.food;
    g.water = h.water - s.water;
  };
  auto check = [&](int tick) {
    mark(r.hp, s.hp >= h.maxHp, tick);
    mark(r.stamina, s.stamina >= h.maxStamina, tick);
    mark(r.mana, s.mana >= h.maxMana, tick);
    mark(r.all, s.hp >= h.maxHp && s.stamina >= h.maxStamina && s.mana >= h.maxMana, tick);
  };
  check(0);
  r.curve.push_back({0, s.hp, s.stamina, s.mana, s.food, s.water});
  int tick = 0;
  // run on a little past "all full" so the curve shows the plateau
  int stopAt = maxTicks;
  for (unsigned t = gameTime + 1; tick < stopAt; t++) {
    tick++;
    if ((t & mask) != 0) continue;
    int manaBurn = 0;
    tenSecondUpdate(s, h, sleeping, t, mode == RestWalking ? 0 : tick, &manaBurn,
                    &r.hpLostToExhaustion);
    if (!r.mana.reached) r.staminaForMana += manaBurn;
    if (s.food < 0 || s.water < 0) r.starving = true;
    check(tick);
    r.curve.push_back({tick, s.hp, s.stamina, s.mana, s.food, s.water});
    if (s.hp == 0) {
      r.died = true;
      break;
    }
    if (r.all.reached && stopAt == maxTicks) stopAt = std::min(maxTicks, tick + tick / 8 + 4 * r.updateEvery);
  }
  r.ticksSimulated = tick;
  return r;
}

}  // namespace calc
