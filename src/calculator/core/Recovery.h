// How hit points, stamina and mana come back, and what food and water
// that takes: TenSecondUpdate() (Code17818.cpp:159), played update by update.
// Nothing in it is random: the "roll" is three bits of the game clock.
#pragma once
#include <vector>
#include "Model.h"

namespace calc {

enum RestMode { RestWalking, RestStanding, RestSleeping, RestModeCount };
const char *restModeName(int mode);

struct RecoveryPoint {
  int tick = 0;                 // since the start
  int hp = 0, stamina = 0, mana = 0, food = 0, water = 0;
};

struct RecoveryGoal {
  bool needed = false;          // it was not full at the start
  bool reached = false;
  int ticks = 0;                // until it is full
  int food = 0, water = 0;      // eaten and drunk up to then
};

struct RecoveryResult {
  RecoveryGoal hp, stamina, mana, all;
  std::vector<RecoveryPoint> curve;   // one point per update
  int updateEvery = 64;         // ticks between two updates
  // what one update gives, when it gives
  int hpGain = 0, manaGain = 0, staminaGain = 0;
  int hpUpdates = 0, manaUpdates = 0;   // in how many of 8 updates it does
  int manaStaminaCost = 0;      // stamina one mana update burns
  int staminaForMana = 0;       // ... in total until mana is full
  int hpLostToExhaustion = 0;
  bool starving = false;        // food or water fell below zero on the way
  bool died = false;
  int ticksSimulated = 0;
};

// gameTime = the clock at the start (it decides which updates give)
RecoveryResult recover(const Hero &hero, int mode, unsigned gameTime, int maxTicks = 60000);

// One update on a hero's numbers; time = the clock, still = ticks since the party moved
struct RecoveryState {
  int hp, stamina, mana, food, water;
};
void tenSecondUpdate(RecoveryState &s, const Hero &hero, bool sleeping, unsigned time, int still,
                     int *staminaForMana = nullptr, int *hpLost = nullptr);

}  // namespace calc
