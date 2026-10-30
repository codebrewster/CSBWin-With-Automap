// HeroStatsHooks.cpp : owns the hero statistics and connects them with loading and saving.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "HeroStatsDump.h"
#include "HeroStatsGame.h"
#include "AutoMapWindow.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include "HeroStatsHooks.h"

static HeroStats store;
static HeroStatsOrigins origins;
// The game before the load in progress, to judge a load that has no .map file
static HeroStats gameBeforeLoad;
// A game is in play once a new game started or a saved game was loaded. The dungeon is told by
// the dungeon file name, the same one the automap uses for the save; a new game is a different
// game even in the same dungeon.
static bool gameInPlay = false;
static std::string dungeonInPlay;

// The blow of the damage call in progress: the deaths it causes are its kills
static bool lastBlowSet = false;
static HeroStatsBlow lastBlow = HeroStatsBlow_Nobody(HeroBlowByOther);
// Missiles that have damaged a monster: only the first damage is a hit
static std::set<int> missilesThatHit;
// Between HeroStats_BeginCloud and HeroStats_EndCloud: the exploding missile and its blow; the
// first cloud made takes the blow
static bool explosionActive = false;
static bool explosionCloudPending = false;
static int explosionMissile = -1;
static HeroStatsBlow explosionBlow = HeroStatsBlow_Nobody(HeroBlowByOther);

HeroStats &HeroStats_Store(void)
{
  return store;
}

HeroStatsOrigins &HeroStats_Origins(void)
{
  return origins;
}

static std::string CurrentDungeon(void)
{
  return dungeonName == NULL ? std::string() : std::string(dungeonName);
}

static std::vector<HeroStatsChampion> PartyNow(void)
{
  std::vector<HeroStatsChampion> party;
  for (int i = 0; i < d.NumCharacter && i < 4; i++) party.push_back(d.hero[i].fingerPrint);
  return party;
}

static void MeetHero(int slot)
{
  char name[sizeof d.hero[slot].name + 1];
  memcpy(name, d.hero[slot].name, sizeof d.hero[slot].name);
  name[sizeof d.hero[slot].name] = 0;
  store.MeetChampion(d.hero[slot].fingerPrint, name);
}

static void MeetParty(void)
{
  for (int i = 0; i < d.NumCharacter && i < 4; i++) MeetHero(i);
}

static void ForgetEngineState(void)
{
  lastBlowSet = false;
  missilesThatHit.clear();
  explosionActive = false;
  explosionCloudPending = false;
  explosionMissile = -1;
}

static void GameStartsOrContinues(void)
{
  gameInPlay = true;
  dungeonInPlay = CurrentDungeon();
  gameBeforeLoad.Forget();
}

void HeroStats_OnDungeonLoaded(void)
{
  gameBeforeLoad = store;
  store.Forget();
  origins.Forget();
  ForgetEngineState();
  if (d.gameState != GAMESTATE_ResumeSavedGame)
  {
    GameStartsOrContinues();
    return;
  }
  store.MergeHardship(gameBeforeLoad);
}

void HeroStats_OnMapFileRead(const HeroStats &fromFile, const HeroStatsOrigins &originsFromFile)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), PartyNow(), fromFile))
  {
    store.TakeFromFile(fromFile);
    store.CountReload();
  }
  else
  {
    store.Forget();
    store.TakeFromFile(fromFile);
  }
  origins = originsFromFile;
  MeetParty();
  GameStartsOrContinues();
}

void HeroStats_OnNoMapFile(void)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), PartyNow(), gameBeforeLoad))
  {
    store.CountReload();
  }
  else
  {
    store.Forget();
  }
  MeetParty();
  GameStartsOrContinues();
}

void HeroStats_BeforeGameSaved(void)
{
  MeetParty();
}

void HeroStats_OnGameSaved(void)
{
  if (!AutoMapWindow_DebugIsOn()) return;
  printf("Hero statistics:\n%s", HeroStatsDump_Text(store).c_str());
}

// ---------------------------------------------------------------------------------------------
// Engine hooks

static bool IsHero(int slot)
{
  return slot >= 0 && slot < d.NumCharacter && slot < 4;
}

// The level everything is counted at; being there visits it
static int PartyLevel(void)
{
  store.MarkLevelVisited(d.partyLevel);
  return d.partyLevel;
}

static HeroStatsBlow HeroBlow(int slot, HeroDamageKind kind, int weapon, bool thrown, int option,
                              uint32_t spell)
{
  MeetHero(slot);
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = d.hero[slot].fingerPrint;
  blow.kind = kind;
  blow.weapon = weapon;
  blow.thrown = thrown;
  blow.option = option;
  blow.spell = spell;
  return blow;
}

static void SetLastBlow(const HeroStatsBlow &blow)
{
  lastBlow = blow;
  lastBlowSet = true;
}

// Launchers and scripts launch missiles without saying whose they are: those are traps
static HeroStatsBlow MissileBlow(int missileIndex)
{
  HeroStatsBlow blow;
  if (!origins.MissileOrigin(missileIndex, &blow)) blow = HeroStatsBlow_Nobody(HeroBlowByTrap);
  return blow;
}

// A missile's first damage to a monster is its hit; a fireball that hurts a whole group, or hurts
// one monster and then explodes, is still one hit
static void CountMissileHit(int level, int missileIndex, const HeroStatsBlow &blow)
{
  if (missilesThatHit.count(missileIndex) != 0) return;
  missilesThatHit.insert(missileIndex);
  store.CountAttackHit(level, blow);
}

int HeroStats_WeaponInHand(int slot)
{
  if (!IsHero(slot)) return HERO_STATS_FISTS;
  RN inHand = d.hero[slot].Possession(1);
  return inHand == RNnul ? HERO_STATS_FISTS : (int)inHand.NameIndex();
}

void HeroStats_OnMeleeAttack(int slot, int attackType, int monsterGroup, int damageShown)
{
  if (!IsHero(slot) || monsterGroup == 0) return;
  HeroStatsBlow blow = HeroBlow(slot, HeroDamageMelee, HeroStats_WeaponInHand(slot), false, attackType,
                                HERO_STATS_NO_SPELL);
  int level = PartyLevel();
  store.CountAttackTry(level, blow);
  if (damageShown > 0)
  {
    store.CountAttackHit(level, blow);
    store.CreditMonsterDamage(level, blow, damageShown);
  }
  SetLastBlow(blow);
}

void HeroStats_ExpectHeroMissile(int slot, HeroDamageKind kind, int weapon, bool thrown, int option,
                                 uint32_t spell)
{
  if (!IsHero(slot)) return;
  origins.ExpectMissile(HeroBlow(slot, kind, weapon, thrown, option, spell));
}

void HeroStats_ExpectMonsterMissile(int monsterType)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = monsterType;
  origins.ExpectMissile(blow);
}

void HeroStats_DropExpectedMissile(void)
{
  // Origins drops an expectation only by handing it on: give it to no record, then forget that
  origins.TakeExpectedMissile(-1);
  origins.ForgetMissile(-1);
}

void HeroStats_OnMissileLaunched(int missileIndex)
{
  if (missileIndex < 0)
  {
    HeroStats_DropExpectedMissile();
    return;
  }
  origins.TakeExpectedMissile(missileIndex);
  missilesThatHit.erase(missileIndex);
  HeroStatsBlow blow;
  if (origins.MissileOrigin(missileIndex, &blow)) store.CountAttackTry(PartyLevel(), blow);
}

void HeroStats_OnMissileFreed(int missileIndex)
{
  origins.ForgetMissile(missileIndex);
  missilesThatHit.erase(missileIndex);
}

void HeroStats_OnMissileDamage(int missileIndex, int damageShown)
{
  HeroStatsBlow blow = MissileBlow(missileIndex);
  int level = PartyLevel();
  if (damageShown > 0)
  {
    CountMissileHit(level, missileIndex, blow);
    store.CreditMonsterDamage(level, blow, damageShown);
  }
  SetLastBlow(blow);
}

void HeroStats_BeginCloud(int missileIndex)
{
  explosionActive = true;
  explosionCloudPending = true;
  explosionMissile = missileIndex;
  explosionBlow = MissileBlow(missileIndex);
}

void HeroStats_OnCloudMade(int cloudIndex)
{
  // Records are reused: a cloud nobody is known to have made must not keep an old cloud's origin
  if (explosionCloudPending)
    origins.SetCloudOrigin(cloudIndex, explosionBlow);
  else
    origins.ForgetCloud(cloudIndex);
  // Only the explosion's own cloud: the death clouds of the monsters it kills are nobody's
  explosionCloudPending = false;
}

void HeroStats_EndCloud(void)
{
  explosionActive = false;
  explosionCloudPending = false;
  explosionMissile = -1;
  lastBlowSet = false;
}

void HeroStats_OnSpellDamage(int damageShown, int memberCount)
{
  HeroStatsBlow blow = explosionActive ? explosionBlow : HeroStatsBlow_Nobody(HeroBlowByCloud);
  int level = PartyLevel();
  if (damageShown > 0 && memberCount > 0)
  {
    if (explosionActive) CountMissileHit(level, explosionMissile, blow);
    store.CreditMonsterDamage(level, blow, damageShown * memberCount);
  }
  SetLastBlow(blow);
}

void HeroStats_OnCloudDamage(int cloudIndex, int damage, int memberCount)
{
  HeroStatsBlow blow;
  if (!origins.CloudOrigin(cloudIndex, &blow)) blow = HeroStatsBlow_Nobody(HeroBlowByCloud);
  int level = PartyLevel();
  if (damage > 0 && memberCount > 0) store.CreditMonsterDamage(level, blow, damage * memberCount);
  SetLastBlow(blow);
}

void HeroStats_OnCloudFreed(int cloudIndex)
{
  origins.ForgetCloud(cloudIndex);
}

void HeroStats_BeginMonsterBlow(HeroBlowBy by)
{
  SetLastBlow(HeroStatsBlow_Nobody(by));
}

void HeroStats_EndMonsterBlow(void)
{
  lastBlowSet = false;
}

void HeroStats_OnMonsterDied(int monsterType, int variant, int why)
{
  // DMW_Movement1 is also how the last monster of a group a pit killed leaves the game
  bool takesLastBlow = why == DMW_Damage || why == DMW_Movement1;
  HeroStatsBlow blow = takesLastBlow && lastBlowSet ? lastBlow : HeroStatsBlow_Nobody(HeroBlowByOther);
  store.CountMonsterKill(PartyLevel(), monsterType, variant, blow);
}
