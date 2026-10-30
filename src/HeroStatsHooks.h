#ifndef HEROSTATSHOOKS_H
#define HEROSTATSHOOKS_H

// Owns the hero statistics of the game in play and connects them with loading, saving and the
// engine. Include it before CSB.h, which leaves #pragma pack(1) switched on: HeroStats.h pulls in
// the standard library, which must not be compiled packed. The HeroStats* headers wrap their own
// types in #pragma pack(push, 8), so those keep one layout even if this order is broken.

#include "HeroStats.h"
#include "HeroStatsOrigins.h"

HeroStats &HeroStats_Store(void);
HeroStatsOrigins &HeroStats_Origins(void);

// AutoMap_OnDungeonLoaded: forgets the stores of the last dungeon. A new game starts empty; for
// a saved game the hardship stays in memory until HeroStats_OnMapFileRead (or, without a usable
// .map file, HeroStats_OnNoMapFile) decides whether it is the same game.
void HeroStats_OnDungeonLoaded(void);
// ReadMapFile read a file. Same game (a game was already in play in this session, same dungeon,
// and every champion of the party known to the file by fingerprint) -> TakeFromFile +
// CountReload; otherwise the file's numbers, hardship included, replace memory. The first load
// after the program starts is never a reload.
void HeroStats_OnMapFileRead(const HeroStats &fromFile, const HeroStatsOrigins &origins);
// A saved game was loaded with no usable .map file: the stores stay empty. The hardship in
// memory survives (and the load counts as a reload) when the party is known to the game that
// was in play, in the same dungeon.
void HeroStats_OnNoMapFile(void);
// The map file is about to be written: the champions of the party are known by name
void HeroStats_BeforeGameSaved(void);
// With map_debug set, the dump is printed on every save
void HeroStats_OnGameSaved(void);

// The engine hooks below only observe: no randomness, no engine state, no change of flow.
// Each converts a slot to its champion (CHARDESC::fingerPrint), meets the champion by name and
// counts at the party's level, which it marks visited.
//
// Kills: every call that credits damage, and every Begin, sets a "last blow" register; the
// deaths that happen inside that damage call are the register's. HeroStats_EndMonsterBlow
// clears it after each damage call site, so a stale blow never names a later killer.

// PhysicalAttackFilter: a melee attack at a monster group (0: none) with the damage the attack
// panel shows. One try; a hit when the damage is above 0. The weapon is the item in the weapon
// hand (possession 1), the option the attack type.
void HeroStats_OnMeleeAttack(int slot, int attackType, int monsterGroup, int damageShown);
// The weapon of the slot's attacks: the name index of the item in the weapon hand (possession 1),
// HERO_STATS_FISTS when it is empty
int HeroStats_WeaponInHand(int slot);
// The next missile launched is the champion's (one try for the weapon and option when it is
// launched) or a monster's
void HeroStats_ExpectHeroMissile(int slot, HeroDamageKind kind, int weapon, bool thrown, int option,
                                 uint32_t spell);
void HeroStats_ExpectMonsterMissile(int monsterType);
// After a call that may refuse to launch (CharacterThrowsSpell): an expectation it left is gone
void HeroStats_DropExpectedMissile(void);
// LaunchMissile has looked for a record: the missile takes the expectation (-1: no record, the
// expectation is dropped)
void HeroStats_OnMissileLaunched(int missileIndex);
void HeroStats_OnMissileFreed(int missileIndex);
// PROJECTILE DAMAGE on a monster; the first damage of a missile is its hit. A missile with no
// known origin (launchers, scripts) is a trap's.
void HeroStats_OnMissileDamage(int missileIndex, int damageShown);
// Around CreateCloud for a missile's explosion: the explosion's SPELL DAMAGE is the missile's,
// and the cloud CreateCloud makes first inherits its blow
void HeroStats_BeginCloud(int missileIndex);
void HeroStats_OnCloudMade(int cloudIndex);                 // every cloud made by CreateCloud
void HeroStats_EndCloud(void);
// SPELL DAMAGE: one number for the group, credited once per member. Outside Begin/EndCloud the
// cloud has no known origin.
void HeroStats_OnSpellDamage(int damageShown, int memberCount);
// A cloud's tick hurts a group: damage per member, by the cloud's origin
void HeroStats_OnCloudDamage(int cloudIndex, int damage, int memberCount);
void HeroStats_OnCloudFreed(int cloudIndex);
// Door, pit: until End, deaths count for it
void HeroStats_BeginMonsterBlow(HeroBlowBy by);
void HeroStats_EndMonsterBlow(void);
// ProcessMonsterDeleteFilter: every death, with its DMW_* reason. DMW_Damage and DMW_Movement1
// (a group the pit killed) take the last blow, if one is set; everything else is
// HeroBlowByOther.
void HeroStats_OnMonsterDied(int monsterType, int variant, int why);

#endif
