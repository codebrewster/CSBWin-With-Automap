#ifndef HEROSTATSHOOKS_H
#define HEROSTATSHOOKS_H

// Owns the hero statistics of the game in play and connects them with loading, saving and the
// engine. Include it before CSB.h, which leaves #pragma pack(1) switched on: HeroStats.h pulls in
// the standard library, which must not be compiled packed. The HeroStats* headers wrap their own
// types in #pragma pack(push, 8), so those keep one layout even if this order is broken.

#include "HeroStats.h"
#include "HeroStatsOrigins.h"
#include "HeroStatsPages.h"

HeroStats &HeroStats_Store(void);
HeroStatsOrigins &HeroStats_Origins(void);
// Fingerprints of the champions in the party now, slot order (empty with no game loaded)
std::vector<HeroStatsChampion> HeroStats_PartyNow(void);
// The dungeon file of the game in play, "" before one is loaded
std::string HeroStats_DungeonInPlay(void);

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
// AutoMap_OnEngineTick, once the party stands on a loaded level (not the prison screen, not
// mid-way to another level): the level is visited, whether or not anything is counted there.
// Only a change of level since the last mark (or a new or loaded game) touches the store.
// It also counts the tick on that level, and whether the party slept through it.
void HeroStats_OnPartyOnLevel(void);
// The player moved the party: MoveParty went to another square or up or down stairs, TurnParty
// turned it (turning on stairs takes the stairs: a step). Bumping into a wall is neither.
void HeroStats_OnPartyStepped(void);
void HeroStats_OnPartyTurned(void);
// A teleporter on level took the party (each teleporter of a chain counts)
void HeroStats_OnPartyTeleported(int level);
// The party went down the pit on level: climbed down with a rope (the engine suppresses the
// pit's damage) or fell. Each pit of a chain counts.
void HeroStats_OnPartyWentDownPit(int level);
// MoveParty: the party bumped into a wall, a closed door or a solid false wall
void HeroStats_OnPartyBumped(void);
// Before the numbers are shown or exported: the explored tiles and the wall texts read, from the
// automap (at most twice a second unless forced)
void HeroStats_RefreshMapCounts(bool force);
// A scroll was opened and shown: counted the first time for each scroll
void HeroStats_OnScrollRead(int scroll);

// Items, known by their record (RN::ConvertToInteger), counted by their basic object type at the
// party's level. An item counts as picked up only the first time the party holds it.
// The party took an item from the dungeon into the hand: the floor, an alcove, a torch holder,
// a swap alcove, a gift from the gods, an open chest
void HeroStats_OnItemPickedUp(int item);
// The item is the party's without being found: taken from a champion's own inventory
void HeroStats_OnItemHeld(int item);
// Every item the champions carry, and the one in hand, is held: a game was loaded (files from
// before the held items were kept know none), a champion joined
void HeroStats_MarkPartyItemsHeld(void);
// A wall took the item in hand (a key in a lock, a coin or gem in a slot), before it is freed
void HeroStats_OnItemUsedInWall(int item);
// DeleteDBEntry freed an item's record: a later item in it is a new one
void HeroStats_OnItemFreed(int item);
// The slot's champion ate (chewed) or drank the item: food, a potion, a gulp from a waterskin
void HeroStats_OnItemConsumed(int slot, int item, bool eaten);
// The slot's champion ate or drank: food and water the item gives, and how much the champion's
// food and water rose (the rest went over the game's maximum)
void HeroStats_OnNourished(int slot, int foodValue, int foodGained, int waterValue, int waterGained);
// The slot's champion drank from a fountain
void HeroStats_OnFountainDrink(int slot, int waterValue, int waterGained);
// A chest was opened in the inventory: counted the first time for each chest
void HeroStats_OnChestOpened(int chest);

// Doors. Begin/End around HitDoor names whose blow hits the door: a champion's attack, a missile,
// an explosion's cloud. Only a champion's (by origin, for missiles and clouds) is counted: an
// attack smashes the door, a thrown or fired item shoots it open, a spell missile or an explosion
// opens it by spell.
void HeroStats_BeginDoorBlowByChampion(int slot);
void HeroStats_BeginDoorBlowByMissile(int missileIndex);
void HeroStats_BeginDoorBlowByCloud(int cloudIndex);
void HeroStats_EndDoorBlow(void);
// HitDoor broke the door at once, or set the timer that breaks it (an attack takes a moment)
void HeroStats_OnDoorBashed(int level, int x, int y);
void HeroStats_OnDoorBashScheduled(int level, int x, int y);
// The party clicked the button of the door at x, y on the loaded level: the toggle it sets closes
// an open door and opens any other
void HeroStats_OnDoorButtonClicked(int x, int y);
// A Zo spell's missile reached a closed door with a door switch and set it opening
void HeroStats_OnDoorOpenedByZo(int missileIndex, int level);
// ProcessTT_BASH_DOOR: the first timer of the door is done. breaksNow: the door was not broken
// yet, and the blow that set the timer counts.
void HeroStats_OnDoorBashTimer(int level, int x, int y, bool breaksNow);

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
// The weapon of the slot's attacks: the basic object type of the item in the weapon hand
// (possession 1), HERO_STATS_UNARMED when it is empty
int HeroStats_WeaponInHand(int slot);
// The weapon key of an item: its basic object type (GetBasicObjectType), the same whatever state
// the item is in (a lit or burnt-out torch, a compass facing anywhere, a charged or empty wand)
class RN;
int HeroStats_WeaponOf(RN item);
// The display name of a weapon key, from the game's item names (d.ObjectNames)
std::string HeroStats_WeaponNameOf(int weapon);
// The name of an attack option (d.AttackNames), "Throw" for HERO_STATS_THROW_OPTION, "" when unknown
std::string HeroStats_AttackOptionNameOf(int option);
// The names of the game in play for the statistics pages: monsters as the dungeon names them
// (GetMonsterDisplayName), weapons and attack options as above
HeroStatsNames HeroStats_NamesOfTheGame(void);
// _Attack carried out an action (after its filters). Actions that cannot hit (War Cry, Calm,
// Spellshield, Light, Climb Down ...) count one use for the weapon in hand and the action; melee
// blows and missiles count their tries elsewhere. Fuse counts before Fusion: on Lord Chaos the
// game ends inside it and never comes back.
void HeroStats_OnActionDone(int slot, int attackType);
// The next missile launched is the champion's (one try for the weapon and option when it is
// launched) or a monster's
void HeroStats_ExpectHeroMissile(int slot, HeroDamageKind kind, int weapon, bool thrown, int option,
                                 uint32_t spell);
// missile: the RN of the missile (RNFireball ... RNPoisonCloud), kept as its kind
void HeroStats_ExpectMonsterMissile(int monsterType, int missile);
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
// A spell filter aborted the cast (CastSpell actions 1-7). Action 1 (abort, no message) is how
// custom dungeons perform the spell in a script themselves: a cast of the slot's incantation.
// Actions 2-7 are failures of it. Outside Begin/EndRuneCast (a script casting) no runes were
// paid for, so nothing counts.
void HeroStats_OnSpellAborted(int slot, int action);
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
// HP and mana actually added: HP is capped at the maximum by the engine, mana may go above it
// (the engine allows that for potions). Forgets the potion's brewer.
void HeroStats_OnPotionDrunk(int slot, int potionIndex, int hpGained, int manaGained);
void HeroStats_OnResurrected(HeroStatsChampion who);

#endif
