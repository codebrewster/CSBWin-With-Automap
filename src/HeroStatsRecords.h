#ifndef HEROSTATSRECORDS_H
#define HEROSTATSRECORDS_H

#include <map>
#include <stdint.h>
#include <tuple>
#include <vector>
#include "HeroStatsExperience.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// The best moments of the run: the biggest hit, the closest call, the longest streak of kills
// without a scratch ... kept per champion (or the party) and level. No engine.
// Records are no hardship: a reload takes the file's.

enum HeroRecordKind { HeroRecBiggestHit, HeroRecBiggestSpellHit, HeroRecBiggestSpellTotal, HeroRecMostKillsAtOnce,
                      HeroRecKillStreak, HeroRecBiggestHitTaken, HeroRecBiggestSpellHitTaken, HeroRecClosestCall,
                      HeroRecBiggestHeal, HeroRecMostManaOnSpell, HeroRecBiggestXpGain, HeroRecHeaviestLoad,
                      HeroRecLongestFall, HeroRecLongestTeleportChain, HeroRecLongestSleep,
                      HeroRecMostPoison, HeroRecLongestPoisoned, HeroRecMostPoisonings,
                      HeroRecMostWoundsAtOnce, HeroRecMostWoundsInOneBlow,
                      HeroRecTotalPoisonings, HeroRecTotalWounds, HERO_RECORD_COUNT };
bool HeroRecord_IsTotal(HeroRecordKind kind);        // a sum over the levels, not a best of them
bool HeroRecord_LowerIsBetter(HeroRecordKind kind);  // the closest call
bool HeroRecord_PartyOnly(HeroRecordKind kind);      // falls, teleport chains, sleep
// Kept for the party as its own (not the best champion's): the party-only ones and the kill streak
bool HeroRecord_PartyHasItsOwn(HeroRecordKind kind);

// value: the record's number. who: the champion, or HERO_STATS_PARTY for the party's own. how:
// the weapon and action, the spell, or the monster (whose blow or spell it was taken from), as
// experience keeps its sources. monsterType, variant, count: on whom (count: the members of the
// group). extra: the most a champion can carry (tenths of kg) for the heaviest load, the class
// for the biggest experience gain. firstLevel, firstTime: where and when a streak or chain began.
// kinds: a streak's two most killed (monster type, count), type -1 for none.
struct HeroRecordEntry
{
  int value;
  HeroStatsChampion who;
  int level;
  uint32_t time;
  HeroXpSource how;
  int monsterType, variant, count;
  int extra;
  int firstLevel;
  uint32_t firstTime;
  int kinds[2][2];
};
HeroRecordEntry HeroRecordEntry_Empty(void);

class HeroStatsRecords
{
public:
  void Forget(void);
  // Kept when it beats the record of its kind, champion and level; a tie keeps the one kept. A
  // closest call needs a champion alive (value at least 1).
  void Offer(HeroRecordKind kind, const HeroRecordEntry &entry);
  // The best of what who and level cover (PARTY: the party's own for HeroRecord_PartyHasItsOwn,
  // else the best champion; WHOLE_RUN: every level); equal values: the earlier game time
  bool Best(HeroRecordKind kind, HeroStatsChampion who, int level, HeroRecordEntry *out) const;

  // Kills without a scratch: a champion's kill counts for their own streak and the party's;
  // damage to a champion ends theirs and the party's
  void CountKill(HeroStatsChampion who, int level, uint32_t time, int monsterType);
  void CountHarm(HeroStatsChampion who);
  // Chains: pits fallen through, teleporters, one after the other; a step or turn ends them
  void CountPitFall(int level, uint32_t time);
  void CountTeleport(int level, uint32_t time);
  void CountMove(void);
  // A tick: asleep it adds to the sleep, awake it ends it
  void CountSleepTick(int level, uint32_t time, bool asleep);

  // Poison. A stretch is the time a champion is poisoned without a break: it begins with the
  // first dose and ends when the last one runs out, is cured, or the champion dies.
  // combined: the poison working on the champion now (the sum over all doses); newDose: a new
  // poisoning, not the next tick of a dose already working. Offers the most poison at once.
  void CountPoison(HeroStatsChampion who, int level, uint32_t time, int combined, bool newDose);
  void CountPoisonDamage(HeroStatsChampion who, int damage);
  // Ends the stretch (if one runs): offers its length, and the doses that began in it
  void EndPoison(HeroStatsChampion who, int level, uint32_t time);
  // Wounds: the wounded parts of a champion now (offered as the most at once), and the parts one
  // blow wounded that were whole before
  // Adds to a champion's total on a level (HeroRecord_IsTotal)
  void CountTotal(HeroRecordKind kind, HeroStatsChampion who, int level, uint32_t time, int add);
  void CountWounds(HeroStatsChampion who, int level, uint32_t time, int woundedNow, int newWounds);

  // For the .map file (HeroStats::FIRST_LAYOUT_WITH_EXPERIENCE), with the streaks, chains and
  // sleep still running
  void Serialize(std::vector<uint8_t> *out) const;
  // withPoison: the stretches of poison still running follow (HeroStats::FIRST_LAYOUT_WITH_POISON)
  bool Deserialize(HeroBytesReader &in, bool withPoison = true);
  size_t SerializedSize(void) const;

private:
  // A streak, chain or sleep still running: its count, where and when it began and last went on,
  // and (a streak) the monsters it killed
  struct Running
  {
    int count;
    int firstLevel, level;
    uint32_t firstTime, time;
    std::map<int, int> kinds;
  };
  void Extend(Running *running, int level, uint32_t time);
  void OfferRunning(HeroRecordKind kind, HeroStatsChampion who, const Running &running);
  // (kind, champion, level)
  typedef std::tuple<int, HeroStatsChampion, int> Key;
  std::map<Key, HeroRecordEntry> m_best;
  std::map<HeroStatsChampion, Running> m_streaks;  // HERO_STATS_PARTY: the party's
  Running m_fall = {}, m_teleports = {}, m_sleep = {};
  // A stretch of poison still running: where and when it began and was last seen, the poison
  // damage taken in it and the doses that began in it
  struct Stretch { int firstLevel, level; uint32_t firstTime, time; int damage, doses; };
  std::map<HeroStatsChampion, Stretch> m_poisoned;
  static HeroRecordEntry StretchEntry(HeroStatsChampion who, const Stretch &stretch, bool byLength);
};

#pragma pack(pop)

#endif
