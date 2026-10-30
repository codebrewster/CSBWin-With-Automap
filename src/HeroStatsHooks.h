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

// Harm: DamageCharacter reports the damage it adds to a hero's pending damage, after armour and
// resistances. Begin/End around the call that hurts the heroes names the source; one pair around
// DamageAllCharacters covers every hero. Pairs nest, and the outermost names the source: the
// stamina that hunger drains spills over as starvation, not exhaustion.
void HeroStats_BeginHarm(HeroHarmSource source);
// A missile hits a hero: a champion's is friendly fire (a hero hitting themself too), a
// monster's is a monster missile of its type, one with no known origin a trap's
void HeroStats_BeginHarmByMissile(int missileIndex);
// A fireball or lightning explodes on the party: between HeroStats_BeginCloud and
// HeroStats_EndCloud it is the exploding missile's, like HeroStats_BeginHarmByMissile; any other
// explosion (scripts, the end of the game) is HeroHarmOther
void HeroStats_BeginHarmByExplosion(void);
// A poison cloud's tick: HeroHarmPoisonCloud with the cloud's origin, the caster when a champion's
void HeroStats_BeginHarmByCloud(int cloudIndex);
void HeroStats_BeginHarmByMonster(int monsterType);
void HeroStats_EndHarm(void);
// End of DamageCharacter: the final damage, credited to the source and kept as pending damage
// until DisplayCharacterDamage applies it. Without a Begin the source is HeroHarmOther.
void HeroStats_OnHeroHarmed(int slot, int finalDamage);
// DisplayCharacterDamage is about to kill the slot: the death's cause is the source that added
// most to the slot's pending damage (ties to the later one), HeroHarmOther when none did
void HeroStats_OnHeroDying(int slot);
// A death that does not come from pending damage (a harmful potion or food)
void HeroStats_OnHeroKilledBy(int slot, HeroHarmSource cause);
// After the loop of DisplayCharacterDamage: the pending damage has been applied
void HeroStats_OnHarmApplied(void);
// The game is lost: every champion is dead
void HeroStats_OnPartyWiped(void);

// Mana and spells. Each rune's mana is taken as it is clicked (TAG01b990) and kept per slot until
// the incantation resolves or the engine throws it away; mana thrown away is "Runes cleared".
// The incantation belongs to its champion: changing the caster or closing the panel keeps it.
//
// A rune's mana was taken. Entered as a power rune, it starts a new incantation: the runes before
// it (four entered, then a power rune) are thrown away.
void HeroStats_OnRuneEntered(int slot, int mana);
void HeroStats_OnRuneRemoved(int slot);                    // its mana: "Runes cleared"
// KillCharacter, RemoveCharacter: the engine empties the incantation
void HeroStats_OnIncantationDiscarded(int slot);
// CharacterDelete: the champions after the slot move down one, their runes with them
void HeroStats_OnSlotRemoved(int slot);
// Around CastSpell in CastMagic: the spell is the slot's incantation, so the outcome hooks take
// its runes' mana. incantationKept: CastMagic leaves the incantation (no flask for a potion);
// otherwise it empties it, and mana no outcome took (a caster dead or gone) is "Runes cleared".
// Outside the pair (a script casting) the outcomes count with no mana.
void HeroStats_BeginRuneCast(int slot);
void HeroStats_EndRuneCast(bool incantationKept);
// The rune key of the slot's incantation, HERO_STATS_NO_SPELL for a slot that is no hero
uint32_t HeroStats_IncantationKey(int slot);
void HeroStats_OnSpellCast(int slot, uint32_t spell);      // all rune mana to the spell row
// A failure: all rune mana is wasted; noSuchSpell marks the key HERO_SPELL_NO_SUCH_SPELL
void HeroStats_OnSpellFailed(int slot, uint32_t spell, bool noSuchSpell);
// A potion spell with no empty flask in hand: a failure, but CastMagic keeps the incantation, so
// its runes' mana stays for the next try
void HeroStats_OnSpellNeedsFlask(int slot, uint32_t spell);
void HeroStats_OnPotionBrewed(int slot, int potionIndex);
void HeroStats_OnPotionFreed(int potionIndex);             // DeleteDBEntry freed the record
void HeroStats_OnItemSpellMana(int slot, int mana);       // CharacterThrowsSpell: staffs, wands
void HeroStats_OnShieldActionMana(int slot, int mana);
void HeroStats_OnHealAction(int slot, int mana, int hpGained);
void HeroStats_OnManaRegenerated(int slot, int amount);
void HeroStats_OnHpRegenerated(int slot, int amount);
// HP and mana actually added (mana only up to the maximum); forgets the potion's brewer
void HeroStats_OnPotionDrunk(int slot, int potionIndex, int hpGained, int manaGained);
void HeroStats_OnResurrected(HeroStatsChampion who);

#endif
