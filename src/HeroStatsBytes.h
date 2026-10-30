#ifndef HEROSTATSBYTES_H
#define HEROSTATSBYTES_H

// Byte-level helpers shared by HeroStats and HeroStatsOrigins for the statistics block of the
// .map file. Numbers are stored lowest byte first; signed values are stored as their 32-bit
// two's complement.
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

inline void HeroBytes_Put(std::vector<uint8_t> *out, uint32_t value, int byteCount)
{
  for (int i = 0; i < byteCount; i++) out->push_back((uint8_t)((value >> (8 * i)) & 0xff));
}

inline void HeroBytes_PutInt(std::vector<uint8_t> *out, int value)
{
  HeroBytes_Put(out, (uint32_t)value, 4);
}

inline void HeroBytes_PutBlow(std::vector<uint8_t> *out, const HeroStatsBlow &blow)
{
  HeroBytes_Put(out, (uint32_t)blow.by, 1);
  HeroBytes_Put(out, blow.champion, 2);
  HeroBytes_Put(out, (uint32_t)blow.kind, 1);
  HeroBytes_PutInt(out, blow.weapon);
  HeroBytes_Put(out, blow.thrown ? 1 : 0, 1);
  HeroBytes_PutInt(out, blow.option);
  HeroBytes_Put(out, blow.spell, 4);
  HeroBytes_PutInt(out, blow.monsterType);
}

// Size of one blow as HeroBytes_PutBlow writes it
const size_t HERO_BYTES_BLOW_SIZE = 1 + 2 + 1 + 4 + 1 + 4 + 4 + 4;

// Reads from a buffer without trusting it: after the first short or invalid field ok is false
// and every later read gives 0.
struct HeroBytesReader
{
  const uint8_t *p;
  size_t left;
  bool ok;

  HeroBytesReader(const uint8_t *data, size_t size) : p(data), left(size), ok(true) {}
  uint32_t Get(int byteCount)
  {
    if (!ok || left < (size_t)byteCount) { ok = false; return 0; }
    uint32_t value = 0;
    for (int i = 0; i < byteCount; i++) value |= (uint32_t)p[i] << (8 * i);
    p += byteCount;
    left -= (size_t)byteCount;
    return value;
  }
  int GetInt(void) { return (int)Get(4); }
  // A count of records of at least minRecordBytes each; one that cannot fit in the bytes left
  // is rejected before anything is allocated
  uint32_t GetCount(size_t minRecordBytes)
  {
    uint32_t count = Get(4);
    if (ok && (uint64_t)count * minRecordBytes > left) ok = false;
    return ok ? count : 0;
  }
  bool GetBlow(HeroStatsBlow *blow)
  {
    uint32_t by = Get(1);
    blow->champion = (HeroStatsChampion)Get(2);
    uint32_t kind = Get(1);
    blow->weapon = GetInt();
    blow->thrown = Get(1) != 0;
    blow->option = GetInt();
    blow->spell = Get(4);
    blow->monsterType = GetInt();
    if (by > (uint32_t)HeroBlowByOther || kind >= (uint32_t)HERO_DAMAGE_KIND_COUNT) ok = false;
    if (!ok) return false;
    blow->by = (HeroBlowBy)by;
    blow->kind = (HeroDamageKind)kind;
    return true;
  }
};

#pragma pack(pop)

#endif
