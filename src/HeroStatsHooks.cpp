// HeroStatsHooks.cpp : owns the hero statistics and connects them with loading and saving.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "HeroStatsDump.h"
#include "HeroStatsGame.h"
#include "HeroStatsHooks.h"
#include "HeroStatsPending.h"
#include "HeroStatsRunes.h"
#include "HeroStatsWindow.h"
#include "AutoMapWindow.h"
#include "AutoMapHooks.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

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
// Between HeroStats_BeginHarm* and HeroStats_EndHarm: the source of the harm, set by the
// outermost Begin
static int harmDepth = 0;
static HeroHarmSource harmSource = HeroHarmOther;
static HeroStatsBlow harmBlow = HeroStatsBlow_Nobody(HeroBlowByOther);
// The harm added to each hero's pending damage since DisplayCharacterDamage last applied it
static HeroStatsPending pending;
// The mana of the runes of each slot's incantation
static HeroStatsRunes runes;
// Between HeroStats_BeginRuneCast and HeroStats_EndRuneCast: the slot whose incantation resolves
static int runeCastSlot = -1;
// Between HeroStats_BeginDoorBlow* and HeroStats_EndDoorBlow: the champion whose blow hits a door
// (HERO_STATS_NOBODY for anyone else); doors whose bash timer runs, with the champion who hit them
struct DoorBlow { HeroStatsChampion by; HeroDoorMethod method; };
static bool doorBlowSet = false;
static DoorBlow doorBlow = {HERO_STATS_NOBODY, HeroDoorSmashed};
static std::map<int, DoorBlow> scheduledDoorBashes;   // DoorKey
// The level last marked visited in the store, so that a tick on the same level costs nothing;
// -1 after the store was replaced
static int levelMarked = -1;

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

std::string HeroStats_DungeonInPlay(void)
{
  return CurrentDungeon();
}

std::vector<HeroStatsChampion> HeroStats_PartyNow(void)
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
  harmDepth = 0;
  pending.ClearAll();
  runes.ClearAll();
  runeCastSlot = -1;
  levelMarked = -1;
  doorBlowSet = false;
  scheduledDoorBashes.clear();
}

static void GameStartsOrContinues(void)
{
  // The store was replaced: the next tick marks the level the party stands on
  levelMarked = -1;
  HeroStats_MarkPartyItemsHeld();
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
    HeroStatsWindow_Forget();
    GameStartsOrContinues();
    return;
  }
  store.MergeHardship(gameBeforeLoad);
}

void HeroStats_OnMapFileRead(const HeroStats &fromFile, const HeroStatsOrigins &originsFromFile)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), HeroStats_PartyNow(), fromFile))
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
  HeroStatsWindow_Forget();
  MeetParty();
  GameStartsOrContinues();
}

void HeroStats_OnNoMapFile(void)
{
  if (HeroStatsGame_IsSame(gameInPlay, dungeonInPlay, CurrentDungeon(), HeroStats_PartyNow(), gameBeforeLoad))
  {
    store.CountReload();
  }
  else
  {
    store.Forget();
  }
  HeroStatsWindow_Forget();
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

static void MarkPartyLevelVisited(void)
{
  if (levelMarked == d.partyLevel) return;
  store.MarkLevelVisited(d.partyLevel);
  levelMarked = d.partyLevel;
}

// A champion who joined is met at once, not at their first blow or wound: walking, picking up and
// eating count for the party too, and the window shows nothing until it knows a champion. A
// candidate in a mirror is not met until taken.
static void MeetNewChampions(void)
{
  static int metCount = -1;
  if (d.PotentialCharacterOrdinal != 0) return;
  int count = d.NumCharacter < 4 ? d.NumCharacter : 4;
  bool allKnown = count == metCount;
  for (int i = 0; i < count && allKnown; i++)
  {
    bool known = false;
    for (size_t c = 0; c < store.Champions().size() && !known; c++)
      known = store.Champions()[c].id == d.hero[i].fingerPrint;
    allKnown = known;
  }
  if (allKnown) return;
  MeetParty();
  metCount = count;
}

void HeroStats_OnPartyOnLevel(void)
{
  MarkPartyLevelVisited();
  MeetNewChampions();
  store.CountTick(d.partyLevel, d.PartySleeping != 0);
  store.MarkTileWalked(d.partyLevel, d.partyX, d.partyY);
}

// The level everything is counted at; being there visits it
static int PartyLevel(void)
{
  MarkPartyLevelVisited();
  return d.partyLevel;
}

static bool IsItem(int item, RN *rn)
{
  if (item < 0 || item >= RNFireball) return false;
  rn->ConstructFromInteger(item);
  if (*rn == RNnul || *rn == RNeof) return false;
  DBTYPE type = rn->dbType();
  return type >= dbWEAPON && type <= dbMISC;
}

void HeroStats_OnItemPickedUp(int item)
{
  RN rn;
  if (!IsItem(item, &rn) || !origins.MarkHeld(item)) return;
  store.CountItem(PartyLevel(), HeroStats_WeaponOf(rn), HeroItemPickedUp);
}

void HeroStats_OnItemHeld(int item)
{
  RN rn;
  if (IsItem(item, &rn)) origins.MarkHeld(item);
}

void HeroStats_MarkPartyItemsHeld(void)
{
  for (int slot = 0; slot < d.NumCharacter && slot < 4; slot++)
    for (int place = 0; place < 30; place++) HeroStats_OnItemHeld(d.hero[slot].Possession(place).ConvertToInteger());
  HeroStats_OnItemHeld(d.objectInHand.ConvertToInteger());
}

void HeroStats_OnItemUsedInWall(int item)
{
  RN rn;
  if (!IsItem(item, &rn)) return;
  store.CountItem(PartyLevel(), HeroStats_WeaponOf(rn), HeroItemUsedInWall);
}

void HeroStats_OnItemFreed(int item)
{
  origins.ForgetItem(item);
}

void HeroStats_OnItemConsumed(int slot, int item, bool eaten)
{
  RN rn;
  if (!IsHero(slot) || !IsItem(item, &rn)) return;
  MeetHero(slot);
  store.CountConsumed(d.hero[slot].fingerPrint, PartyLevel(), HeroStats_WeaponOf(rn),
                      eaten ? HeroConsumeEaten : HeroConsumeDrunk);
}

void HeroStats_OnScrollRead(int scroll)
{
  RN rn;
  if (!IsItem(scroll, &rn) || rn.dbType() != dbSCROLL || !origins.MarkRead(scroll)) return;
  store.CountScrollRead(PartyLevel());
}

void HeroStats_OnNourished(int slot, int foodValue, int foodGained, int waterValue, int waterGained)
{
  if (!IsHero(slot)) return;
  MeetHero(slot);
  int level = PartyLevel();
  store.CreditFood(d.hero[slot].fingerPrint, level, foodValue, foodGained);
  store.CreditWater(d.hero[slot].fingerPrint, level, waterValue, waterGained);
}

void HeroStats_OnFountainDrink(int slot, int waterValue, int waterGained)
{
  if (!IsHero(slot)) return;
  MeetHero(slot);
  int level = PartyLevel();
  store.CountFountainDrink(d.hero[slot].fingerPrint, level);
  store.CreditWater(d.hero[slot].fingerPrint, level, waterValue, waterGained);
}

void HeroStats_OnChestOpened(int chest)
{
  RN rn;
  if (!IsItem(chest, &rn) || rn.dbType() != dbCHEST || !origins.MarkOpened(chest)) return;
  store.CountChestOpened(PartyLevel());
}

static HeroStatsBlow MissileBlow(int missileIndex);

static int DoorKey(int level, int x, int y)
{
  return (level << 16) | ((x & 0xff) << 8) | (y & 0xff);
}

static void BeginDoorBlow(const HeroStatsBlow &blow, HeroDoorMethod method)
{
  doorBlowSet = true;
  doorBlow.by = blow.by == HeroBlowByChampion ? blow.champion : HERO_STATS_NOBODY;
  doorBlow.method = method;
}

void HeroStats_BeginDoorBlowByChampion(int slot)
{
  if (!IsHero(slot)) return;
  MeetHero(slot);
  doorBlowSet = true;
  doorBlow.by = d.hero[slot].fingerPrint;
  doorBlow.method = HeroDoorSmashed;
}

void HeroStats_BeginDoorBlowByMissile(int missileIndex)
{
  HeroStatsBlow blow = MissileBlow(missileIndex);
  BeginDoorBlow(blow, blow.kind == HeroDamageMagic ? HeroDoorBySpell : HeroDoorShot);
}

void HeroStats_BeginDoorBlowByCloud(int cloudIndex)
{
  HeroStatsBlow blow;
  if (!origins.CloudOrigin(cloudIndex, &blow)) blow = HeroStatsBlow_Nobody(HeroBlowByCloud);
  BeginDoorBlow(blow, HeroDoorBySpell);
}

void HeroStats_EndDoorBlow(void)
{
  doorBlowSet = false;
}

void HeroStats_OnDoorBashed(int level, int x, int y)
{
  (void)x;
  (void)y;
  if (!doorBlowSet || doorBlow.by == HERO_STATS_NOBODY) return;
  MarkPartyLevelVisited();
  store.CountDoorBashed(doorBlow.by, level, doorBlow.method);
}

void HeroStats_OnDoorButtonClicked(int x, int y)
{
  bool isOpen = (d.LevelCellFlags[x][y] & 7) == 0;
  store.CountDoorButton(PartyLevel(), !isOpen);
}

void HeroStats_OnDoorOpenedByZo(int missileIndex, int level)
{
  HeroStatsBlow blow = MissileBlow(missileIndex);
  if (blow.by != HeroBlowByChampion) return;
  MarkPartyLevelVisited();
  store.CountDoorOpenedByZo(blow.champion, level);
}

void HeroStats_OnDoorBashScheduled(int level, int x, int y)
{
  // Every blow before the timer fires sets another timer: the first one breaks the door
  int key = DoorKey(level, x, y);
  if (scheduledDoorBashes.count(key) != 0) return;
  DoorBlow nobody = {HERO_STATS_NOBODY, HeroDoorSmashed};
  scheduledDoorBashes[key] = doorBlowSet ? doorBlow : nobody;
}

void HeroStats_OnDoorBashTimer(int level, int x, int y, bool breaksNow)
{
  std::map<int, DoorBlow>::iterator it = scheduledDoorBashes.find(DoorKey(level, x, y));
  if (it == scheduledDoorBashes.end()) return;
  DoorBlow blow = it->second;
  scheduledDoorBashes.erase(it);
  if (breaksNow && blow.by != HERO_STATS_NOBODY) store.CountDoorBashed(blow.by, level, blow.method);
}

// Chaos Strikes Back starts on a screen of its own, which is no dungeon level
static const int LEVEL_OF_THE_PRISON_SCREEN = 255;

void HeroStats_OnPartyStepped(void)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  store.CountStep(PartyLevel());
}

void HeroStats_RefreshMapCounts(bool force)
{
  static Uint32 lastRefresh = 0;
  static bool refreshed = false;
  Uint32 now = SDL_GetTicks();
  if (!force && refreshed && now - lastRefresh < 500) return;
  lastRefresh = now;
  refreshed = true;
  store.SetExploredTiles(AutoMap_ExploredTilesPerLevel());
  std::map<int, AutoMapWallCounts> walls = AutoMap_WallCountsPerLevel();
  std::map<int, int> texts, seen, viewed;
  for (std::map<int, AutoMapWallCounts>::const_iterator at = walls.begin(); at != walls.end(); ++at)
  {
    texts[at->first] = at->second.textsRead;
    seen[at->first] = at->second.portraitsSeen;
    viewed[at->first] = at->second.portraitsViewed;
  }
  store.SetWallTextsRead(texts);
  store.SetChampionsSeen(seen, viewed);
}

void HeroStats_OnPartyTurned(void)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  store.CountTurn(PartyLevel());
}

void HeroStats_OnPartyTeleported(int level)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  store.MarkLevelVisited(level);
  store.CountTeleport(level);
}

void HeroStats_OnPartyWentDownPit(int level)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  store.MarkLevelVisited(level);
  // The rope's Climb Down sets it for the first pit only; the pits below are falls
  if (d.SupressPitDamage != 0) store.CountPitClimb(level);
  else store.CountPitFall(level);
}

void HeroStats_OnPartyBumped(void)
{
  if (d.partyLevel == LEVEL_OF_THE_PRISON_SCREEN) return;
  store.CountBump(PartyLevel());
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
  if (!IsHero(slot)) return HERO_STATS_UNARMED;
  RN inHand = d.hero[slot].Possession(1);
  return inHand == RNnul ? HERO_STATS_UNARMED : HeroStats_WeaponOf(inHand);
}

// The basic type, not the name index: that one changes with a torch's flame, a compass's
// facing or a charge left, and would split one item into several rows
int HeroStats_WeaponOf(RN item)
{
  return (int)GetBasicObjectType(item);
}

std::string HeroStats_WeaponNameOf(int weapon)
{
  return HeroStats_WeaponName(weapon, d.ObjectNames, (int)(sizeof d.ObjectNames / sizeof d.ObjectNames[0]));
}

std::string HeroStats_AttackOptionNameOf(int option)
{
  if (option == HERO_STATS_THROW_OPTION) return "Throw";
  if (option < 0) return "";
  const char *at = (const char *)d.AttackNames;
  const char *end = at + sizeof d.AttackNames;
  for (; option > 0 && at < end; option--)
    while (at < end && *(at++) != 0) {}
  if (at >= end) return "";
  return std::string(at, strnlen(at, end - at));
}

// Statistics.cpp
void GetMonsterDisplayName(int monsterType, int graphic, char *name);

static std::string MonsterNameOf(int monsterType, int variant)
{
  char name[256] = "";
  GetMonsterDisplayName(monsterType, variant, name);
  return name;
}

HeroStatsNames HeroStats_NamesOfTheGame(void)
{
  HeroStatsNames names = {MonsterNameOf, HeroStats_WeaponNameOf, HeroStats_AttackOptionNameOf};
  return names;
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

// Melee blows count their tries in HeroStats_OnMeleeAttack, missiles when they are launched
static bool ActionCanHit(int attackType)
{
  switch (attackType)
  {
  case atk_BASH: case atk_HACK: case atk_BERZERK: case atk_KICK: case atk_SWING: case atk_CHOP:
  case atk_DISRUPT: case atk_JAB: case atk_PARRY: case atk_STAB2: case atk_STAB1: case atk_STUN:
  case atk_THRUST: case atk_MELEE: case atk_SLASH: case atk_CLEAVE: case atk_PUNCH:
  case atk_LIGHTNING: case atk_DISPELL: case atk_FIREBALL: case atk_SPIT: case atk_INVOKE:
  case atk_SHOOT: case atk_THROW:
    return true;
  default:
    return false;
  }
}

void HeroStats_OnActionDone(int slot, int attackType)
{
  if (!IsHero(slot) || attackType < 0 || ActionCanHit(attackType)) return;
  HeroStatsBlow blow = HeroBlow(slot, HeroDamageMelee, HeroStats_WeaponInHand(slot), false, attackType,
                                HERO_STATS_NO_SPELL);
  store.CountActionUse(PartyLevel(), blow);
}

void HeroStats_ExpectHeroMissile(int slot, HeroDamageKind kind, int weapon, bool thrown, int option,
                                 uint32_t spell)
{
  if (!IsHero(slot)) return;
  origins.ExpectMissile(HeroBlow(slot, kind, weapon, thrown, option, spell));
}

void HeroStats_ExpectMonsterMissile(int monsterType, int missile)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = monsterType;
  blow.spell = HERO_SPELL_MONSTER_MISSILE | (uint32_t)(missile & 0xff);
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

// ---------------------------------------------------------------------------------------------
// Harm and deaths

static void BeginHarmWith(HeroHarmSource source, const HeroStatsBlow &blow)
{
  if (harmDepth++ != 0) return;
  harmSource = source;
  harmBlow = blow;
}

// The source of a missile's or an explosion's harm, from its blow
static void BeginHarmByMissileBlow(const HeroStatsBlow &blow)
{
  HeroHarmSource source = HeroHarmOther;
  if (blow.by == HeroBlowByChampion) source = HeroHarmFriendlyFire;
  else if (blow.by == HeroBlowByMonster) source = HeroHarmMonsterMissile;
  else if (blow.by == HeroBlowByTrap) source = HeroHarmTrap;
  BeginHarmWith(source, blow);
}

void HeroStats_BeginHarm(HeroHarmSource source)
{
  BeginHarmWith(source, HeroStatsBlow_Nobody(HeroBlowByOther));
}

void HeroStats_BeginHarmByMissile(int missileIndex)
{
  BeginHarmByMissileBlow(MissileBlow(missileIndex));
}

void HeroStats_BeginHarmByExplosion(void)
{
  if (explosionActive)
    BeginHarmByMissileBlow(explosionBlow);
  else
    HeroStats_BeginHarm(HeroHarmOther);
}

void HeroStats_BeginHarmByCloud(int cloudIndex)
{
  HeroStatsBlow blow;
  if (!origins.CloudOrigin(cloudIndex, &blow)) blow = HeroStatsBlow_Nobody(HeroBlowByCloud);
  BeginHarmWith(HeroHarmPoisonCloud, blow);
}

void HeroStats_BeginHarmByMonster(int monsterType)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByMonster);
  blow.monsterType = monsterType;
  BeginHarmWith(HeroHarmMonster, blow);
}

void HeroStats_EndHarm(void)
{
  if (harmDepth > 0) harmDepth--;
}

void HeroStats_OnHeroHarmed(int slot, int finalDamage)
{
  if (!IsHero(slot) || finalDamage <= 0) return;
  HeroHarmSource source = harmDepth > 0 ? harmSource : HeroHarmOther;
  HeroStatsBlow blow = harmDepth > 0 ? harmBlow : HeroStatsBlow_Nobody(HeroBlowByOther);
  MeetHero(slot);
  store.CreditHarm(d.hero[slot].fingerPrint, PartyLevel(), source, blow, finalDamage);
  pending.Add(slot, source, blow, finalDamage);
}

static void CountDeath(int slot, HeroHarmSource cause, const HeroStatsBlow &blow)
{
  MeetHero(slot);
  HeroStatsDeath death;
  death.who = d.hero[slot].fingerPrint;
  death.level = PartyLevel();
  death.cause = cause;
  death.by = blow;
  store.CountDeath(death);
}

void HeroStats_OnHeroDying(int slot)
{
  if (!IsHero(slot)) return;
  HeroHarmSource cause;
  HeroStatsBlow blow;
  if (!pending.Cause(slot, &cause, &blow))
  {
    // Damage a script wrote into the pending damage itself
    cause = HeroHarmOther;
    blow = HeroStatsBlow_Nobody(HeroBlowByOther);
  }
  CountDeath(slot, cause, blow);
  pending.Clear(slot);
}

void HeroStats_OnHeroKilledBy(int slot, HeroHarmSource cause)
{
  if (!IsHero(slot)) return;
  CountDeath(slot, cause, HeroStatsBlow_Nobody(HeroBlowByOther));
}

void HeroStats_OnHarmApplied(void)
{
  pending.ClearAll();
}

void HeroStats_OnPartyWiped(void)
{
  store.CountPartyWipe();
}

// ---------------------------------------------------------------------------------------------
// Mana, spells, healing, resurrection

static HeroStatsChampion MetChampion(int slot)
{
  MeetHero(slot);
  return d.hero[slot].fingerPrint;
}

static void CreditRunesCleared(int slot, int mana)
{
  if (!IsHero(slot) || mana <= 0) return;
  store.CreditManaSpent(MetChampion(slot), PartyLevel(), HERO_SPELL_RUNES_CLEARED, mana);
}

// The mana of the incantation that resolves; none for a spell a script casts
static int TakeRuneCastMana(void)
{
  if (runeCastSlot < 0) return 0;
  return runes.TakeAll(runeCastSlot);
}

void HeroStats_OnRuneEntered(int slot, int mana)
{
  if (!IsHero(slot)) return;
  // byte33 is the position of the rune being entered; 0 is the power rune of a new incantation
  if (d.hero[slot].byte33 == 0) CreditRunesCleared(slot, runes.TakeAll(slot));
  runes.AddRune(slot, mana);
}

void HeroStats_OnRuneRemoved(int slot)
{
  if (!IsHero(slot)) return;
  int mana = 0;
  runes.RemoveLastRune(slot, &mana);
  CreditRunesCleared(slot, mana);
}

void HeroStats_OnIncantationDiscarded(int slot)
{
  if (!IsHero(slot)) return;
  CreditRunesCleared(slot, runes.TakeAll(slot));
}

void HeroStats_OnSlotRemoved(int slot)
{
  runes.RemoveSlot(slot);
}

void HeroStats_BeginRuneCast(int slot)
{
  runeCastSlot = IsHero(slot) ? slot : -1;
}

void HeroStats_EndRuneCast(bool incantationKept)
{
  if (runeCastSlot >= 0 && !incantationKept) CreditRunesCleared(runeCastSlot, runes.TakeAll(runeCastSlot));
  runeCastSlot = -1;
}

void HeroStats_OnSpellAborted(int slot, int action)
{
  if (runeCastSlot < 0 || !IsHero(slot)) return;
  uint32_t spell = HeroStats_RuneSpellKey(d.hero[slot].incantation);
  // The silent abort: the dungeon's script performs the spell instead of the engine
  if (action == 1) HeroStats_OnSpellCast(slot, spell);
  else HeroStats_OnSpellFailed(slot, spell, false);
}

void HeroStats_OnSpellCast(int slot, uint32_t spell)
{
  if (!IsHero(slot)) return;
  store.CountCast(MetChampion(slot), PartyLevel(), spell, TakeRuneCastMana());
}

void HeroStats_OnSpellFailed(int slot, uint32_t spell, bool noSuchSpell)
{
  if (!IsHero(slot)) return;
  uint32_t key = noSuchSpell ? (spell | HERO_SPELL_NO_SUCH_SPELL) : spell;
  store.CountFailedCast(MetChampion(slot), PartyLevel(), key, TakeRuneCastMana());
}

void HeroStats_OnSpellNeedsFlask(int slot, uint32_t spell)
{
  if (!IsHero(slot)) return;
  store.CountFailedCast(MetChampion(slot), PartyLevel(), spell, 0);
}

void HeroStats_OnPotionBrewed(int slot, int potionIndex)
{
  if (!IsHero(slot) || potionIndex < 0) return;
  origins.SetBrewer(potionIndex, MetChampion(slot));
}

void HeroStats_OnPotionFreed(int potionIndex)
{
  origins.ForgetPotion(potionIndex);
}

void HeroStats_OnItemSpellMana(int slot, int mana)
{
  if (!IsHero(slot) || mana <= 0) return;
  store.CreditManaSpent(MetChampion(slot), PartyLevel(), HERO_SPELL_ITEM_SPELLS, mana);
}

void HeroStats_OnShieldActionMana(int slot, int mana)
{
  if (!IsHero(slot) || mana <= 0) return;
  store.CreditManaSpent(MetChampion(slot), PartyLevel(), HERO_SPELL_SHIELD_ACTION, mana);
}

void HeroStats_OnHealAction(int slot, int mana, int hpGained)
{
  if (!IsHero(slot)) return;
  HeroStatsChampion who = MetChampion(slot);
  int level = PartyLevel();
  store.CreditManaSpent(who, level, HERO_SPELL_HEAL_ACTION, mana);
  store.CreditHealing(who, level, HeroHealAction, HERO_STATS_NOBODY, hpGained);
}

void HeroStats_OnManaRegenerated(int slot, int amount)
{
  if (!IsHero(slot) || amount <= 0) return;
  store.CreditManaGained(MetChampion(slot), PartyLevel(), HeroManaRegeneration, amount);
}

void HeroStats_OnHpRegenerated(int slot, int amount)
{
  if (!IsHero(slot) || amount <= 0) return;
  store.CreditHealing(MetChampion(slot), PartyLevel(), HeroHealRegeneration, HERO_STATS_NOBODY, amount);
}

void HeroStats_OnPotionDrunk(int slot, int potionIndex, int hpGained, int manaGained)
{
  if (IsHero(slot))
  {
    HeroStatsChampion who = MetChampion(slot);
    int level = PartyLevel();
    store.CreditHealing(who, level, HeroHealPotion, origins.Brewer(potionIndex), hpGained);
    store.CreditManaGained(who, level, HeroManaPotion, manaGained);
  }
  // The flask is empty now: a potion brewed into it later is someone's new potion
  origins.ForgetPotion(potionIndex);
}

void HeroStats_OnResurrected(HeroStatsChampion who)
{
  store.CountResurrection(who, PartyLevel());
}
