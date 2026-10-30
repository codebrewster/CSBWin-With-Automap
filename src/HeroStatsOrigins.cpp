// HeroStatsOrigins.cpp : who launched a missile, made a cloud, brewed a potion.
#include "HeroStatsOrigins.h"
#include "HeroStatsBytes.h"

void HeroStatsOrigins::Forget(void)
{
  m_expecting = false;
  m_missiles.clear();
  m_clouds.clear();
  m_brewers.clear();
}

void HeroStatsOrigins::ExpectMissile(const HeroStatsBlow &blow)
{
  m_expected = blow;
  m_expecting = true;
}

void HeroStatsOrigins::TakeExpectedMissile(int missileIndex)
{
  if (m_expecting)
    m_missiles[missileIndex] = m_expected;  // records are reused: replace what an old missile left
  else
    m_missiles.erase(missileIndex);
  m_expecting = false;
}

bool HeroStatsOrigins::MissileOrigin(int missileIndex, HeroStatsBlow *blow) const
{
  std::map<int, HeroStatsBlow>::const_iterator it = m_missiles.find(missileIndex);
  if (it == m_missiles.end())
    return false;
  *blow = it->second;
  return true;
}

void HeroStatsOrigins::ForgetMissile(int missileIndex)
{
  m_missiles.erase(missileIndex);
}

void HeroStatsOrigins::SetCloudOrigin(int cloudIndex, const HeroStatsBlow &blow)
{
  m_clouds[cloudIndex] = blow;
}

bool HeroStatsOrigins::CloudOrigin(int cloudIndex, HeroStatsBlow *blow) const
{
  std::map<int, HeroStatsBlow>::const_iterator it = m_clouds.find(cloudIndex);
  if (it == m_clouds.end())
    return false;
  *blow = it->second;
  return true;
}

void HeroStatsOrigins::ForgetCloud(int cloudIndex)
{
  m_clouds.erase(cloudIndex);
}

void HeroStatsOrigins::SetBrewer(int potionIndex, HeroStatsChampion brewer)
{
  m_brewers[potionIndex] = brewer;
}

HeroStatsChampion HeroStatsOrigins::Brewer(int potionIndex) const
{
  std::map<int, HeroStatsChampion>::const_iterator it = m_brewers.find(potionIndex);
  return it == m_brewers.end() ? HERO_STATS_NOBODY : it->second;
}

void HeroStatsOrigins::ForgetPotion(int potionIndex)
{
  m_brewers.erase(potionIndex);
}

const std::map<int, HeroStatsBlow> &HeroStatsOrigins::Missiles(void) const { return m_missiles; }
const std::map<int, HeroStatsBlow> &HeroStatsOrigins::Clouds(void) const { return m_clouds; }
const std::map<int, HeroStatsChampion> &HeroStatsOrigins::Brewers(void) const { return m_brewers; }

void HeroStatsOrigins::Restore(const std::map<int, HeroStatsBlow> &missiles,
                               const std::map<int, HeroStatsBlow> &clouds,
                               const std::map<int, HeroStatsChampion> &brewers)
{
  m_expecting = false;
  m_missiles = missiles;
  m_clouds = clouds;
  m_brewers = brewers;
}

// Three counted tables: index (4 bytes) then the blow for missiles and clouds, the champion
// (2 bytes) for brewers
void HeroStatsOrigins::Serialize(std::vector<uint8_t> *out) const
{
  const std::map<int, HeroStatsBlow> *tables[2] = {&m_missiles, &m_clouds};
  for (int t = 0; t < 2; t++)
  {
    HeroBytes_Put(out, (uint32_t)tables[t]->size(), 4);
    for (std::map<int, HeroStatsBlow>::const_iterator it = tables[t]->begin(); it != tables[t]->end(); ++it)
    {
      HeroBytes_PutInt(out, it->first);
      HeroBytes_PutBlow(out, it->second);
    }
  }
  HeroBytes_Put(out, (uint32_t)m_brewers.size(), 4);
  for (std::map<int, HeroStatsChampion>::const_iterator it = m_brewers.begin(); it != m_brewers.end(); ++it)
  {
    HeroBytes_PutInt(out, it->first);
    HeroBytes_Put(out, it->second, 2);
  }
}

bool HeroStatsOrigins::Deserialize(const uint8_t *data, size_t size)
{
  HeroBytesReader in(data, size);
  std::map<int, HeroStatsBlow> tables[2];
  for (int t = 0; t < 2; t++)
  {
    uint32_t count = in.GetCount(4 + HERO_BYTES_BLOW_SIZE);
    for (uint32_t i = 0; i < count && in.ok; i++)
    {
      int index = in.GetInt();
      HeroStatsBlow blow;
      if (!in.GetBlow(&blow)) return false;
      tables[t][index] = blow;
    }
  }
  std::map<int, HeroStatsChampion> brewers;
  uint32_t count = in.GetCount(4 + 2);
  for (uint32_t i = 0; i < count && in.ok; i++)
  {
    int index = in.GetInt();
    brewers[index] = (HeroStatsChampion)in.Get(2);
  }
  if (!in.ok || in.left != 0) return false;
  Restore(tables[0], tables[1], brewers);
  return true;
}
