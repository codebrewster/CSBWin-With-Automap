#ifndef HEROSTATSORIGINS_H
#define HEROSTATSORIGINS_H

#include <map>
#include "HeroStats.h"

// Who launched a missile, made a cloud, brewed a potion. Keyed by the record's index in its
// database, like ShotMissiles: the engine's records have no room for it.
class HeroStatsOrigins
{
public:
  void Forget(void);                                        // new dungeon
  // The next missile launched takes this blow (like ExpectShotMissile); taken or not, it is gone
  void ExpectMissile(const HeroStatsBlow &blow);
  void TakeExpectedMissile(int missileIndex);               // no expectation: the missile has no origin
  bool MissileOrigin(int missileIndex, HeroStatsBlow *blow) const;
  void ForgetMissile(int missileIndex);
  void SetCloudOrigin(int cloudIndex, const HeroStatsBlow &blow);
  bool CloudOrigin(int cloudIndex, HeroStatsBlow *blow) const;
  void ForgetCloud(int cloudIndex);
  void SetBrewer(int potionIndex, HeroStatsChampion brewer);
  HeroStatsChampion Brewer(int potionIndex) const;          // HERO_STATS_NOBODY when unknown
  void ForgetPotion(int potionIndex);
  // For the .map file
  const std::map<int, HeroStatsBlow> &Missiles(void) const;
  const std::map<int, HeroStatsBlow> &Clouds(void) const;
  const std::map<int, HeroStatsChampion> &Brewers(void) const;
  void Restore(const std::map<int, HeroStatsBlow> &missiles, const std::map<int, HeroStatsBlow> &clouds,
               const std::map<int, HeroStatsChampion> &brewers);

private:
  bool m_expecting = false;
  HeroStatsBlow m_expected = HeroStatsBlow_Nobody(HeroBlowByOther);
  std::map<int, HeroStatsBlow> m_missiles;
  std::map<int, HeroStatsBlow> m_clouds;
  std::map<int, HeroStatsChampion> m_brewers;
};

#endif
