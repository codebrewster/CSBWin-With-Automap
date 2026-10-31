#ifndef HEROSTATSORIGINS_H
#define HEROSTATSORIGINS_H

#include <map>
#include <set>
#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

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
  // Items the party has held, keyed by record (database type and index, HeroStats_ItemKey): an
  // item counts as picked up only the first time. True when the item was not held before.
  bool MarkHeld(int itemKey);
  bool WasHeld(int itemKey) const;
  void ForgetItem(int itemKey);                             // the record was freed
  const std::set<int> &HeldItems(void) const;
  // Chests the party has opened, by record: a chest counts as opened only the first time. True
  // when it was not opened before. A freed record is forgotten with ForgetItem.
  bool MarkOpened(int chestKey);
  // Scrolls the party has read, the same way: true the first time
  bool MarkRead(int scrollKey);
  // For the .map file
  const std::map<int, HeroStatsBlow> &Missiles(void) const;
  const std::map<int, HeroStatsBlow> &Clouds(void) const;
  const std::map<int, HeroStatsChampion> &Brewers(void) const;
  void Restore(const std::map<int, HeroStatsBlow> &missiles, const std::map<int, HeroStatsBlow> &clouds,
               const std::map<int, HeroStatsChampion> &brewers);

  // For the .map file, like HeroStats: the expected missile is not stored. layout as for
  // HeroStats::Deserialize: before HeroStats::FIRST_LAYOUT_WITH_ITEMS there are no held items,
  // before HeroStats::FIRST_LAYOUT_WITH_CONSUMPTION no opened chests, before
  // HeroStats::FIRST_LAYOUT_WITH_SCROLLS no read scrolls.
  void Serialize(std::vector<uint8_t> *out) const;
  bool Deserialize(const uint8_t *data, size_t size, int layout = HeroStats::LAYOUT);

private:
  bool m_expecting = false;
  HeroStatsBlow m_expected = HeroStatsBlow_Nobody(HeroBlowByOther);
  std::map<int, HeroStatsBlow> m_missiles;
  std::map<int, HeroStatsBlow> m_clouds;
  std::map<int, HeroStatsChampion> m_brewers;
  std::set<int> m_held;
  std::set<int> m_opened;
  std::set<int> m_read;
};

#pragma pack(pop)

#endif
