// HeroStatsCatalogue.cpp : every number of the Statistics window, defined once
#include "HeroStatsCatalogue.h"
#include <ctype.h>
#include <algorithm>
#include "HeroStatsView.h"

static const HeroStatsNumberInfo NUMBERS[HERO_NUMBER_COUNT] = {
    // label, help, partyOnly, wholeRunOnly, isBad, isRatio, isTicks
    {"Damage dealt", "", false, false, false, false, false},
    {"Melee", "", false, false, false, false, false},
    {"Ranged", "", false, false, false, false, false},
    {"Magic", "", false, false, false, false, false},
    {"Kills", "Kills by the champions. Monsters killed by doors, falls, telefrags and landings are on the Monsters tab.", false, false, false, false, false},
    {"Attacks", "Blows and missiles aimed at a monster. Other actions are not counted here, but are in the Weapons table's Tries.", false, false, false, false, false},
    {"Hits", "", false, false, false, false, false},
    {"Hit rate", "Hits \xc3\xb7 Attacks.", false, false, false, true, false},
    {"Damage per hit", "Damage of blows and missiles \xc3\xb7 Hits. Spells are not in it.", false, false, false, true, false},
    {"Other actions", "Actions that cannot hit: War Cry, Calm, Brandish, Blow Horn, Confuse, shields and the like. Not part of Attacks.", false, false, false, false, false},
    {"Friendly fire caused", "Damage this champion's missiles, spells and poison clouds did to the own party. Not part of Damage dealt.", false, false, true, false, false},
    {"Mana generated", "", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Mana potions", "", false, false, false, false, false},
    {"Regeneration lost (mana full)", "Mana regeneration would have given had the champion's mana not been full: all of it at or above the maximum, the part over the maximum when nearly full. Spend mana before it fills up to use it. Full mana also spends no stamina on regenerating. Not part of Mana generated.", false, false, true, false, false},
    {"Mana spent", "Spent as each rune is entered, on the level it is entered on, whatever becomes of the spell; staffs, wands and the shield and heal actions as they are used. The lines below add up to it, Success rate aside.", false, false, false, false, false},
    {"Mana on successful casts", "Runes of spells that worked, staffs and wands, the shield and heal actions.", false, false, false, false, false},
    {"Mana success rate", "On successful casts \xc3\xb7 (On successful casts + Wasted): the part of the mana that bought a spell that worked, of all the mana whose runes came to an end. Not cast yet is left out: those runes may still work. Not one of the lines that add up to Mana spent. It can differ from the casts' Success rate both ways: a failed cast at a high power wastes more mana than a weak one; cleared runes waste mana but are no cast; a potion spell cast with no empty flask fails but keeps its runes for the next try, a wasted cast but no wasted mana; and staffs, wands and the shield and heal actions are successful mana but no casts. The MANA Rate of All spells in the Spells table. On one level the two can differ: here runes count where they were entered, the table counts a spell where it was cast.", false, false, false, true, false},
    {"Mana wasted", "Mana that bought nothing: runes of spells that failed or formed no spell, and runes cleared. The Wasted mana of All spells in the Spells table. On one level the two can differ: here runes count where they were entered, the table counts a spell where it was cast.", false, false, true, false, false},
    {"Mana on failed casts", "Runes of spells the champion needed more practice for.", false, false, true, false, false},
    {"Mana on meaningless spells", "Runes that form no spell: the champion mumbles a meaningless spell.", false, false, true, false, false},
    {"Runes cleared", "Runes entered and never cast: cleared, replaced by a new incantation, or lost when the caster died. The game gives no mana back. A line of its own in the Spells table.", false, false, true, false, false},
    {"Not cast yet", "Runes entered and waiting in an incantation, on the level they were entered on.", false, false, false, false, false},
    {"Spells cast", "Every cast: successful, failed and meaningless. Spells from staffs and wands are not among them.", false, false, false, false, false},
    {"Successful", "Casts that worked.", false, false, false, false, false},
    {"Cast success rate", "Successful \xc3\xb7 Spells cast: the part of the casts that worked. The CASTS Rate of All spells in the Spells table.", false, false, false, true, false},
    {"Wasted", "Casts that bought nothing: failed or meaningless. The Wasted casts of All spells in the Spells table.", false, false, true, false, false},
    {"Failed", "Casts of a real spell that failed: the champion needed more practice.", false, false, true, false, false},
    {"Meaningless", "Casts whose runes form no spell: the champion mumbles a meaningless spell.", false, false, true, false, false},
    {"Damage taken", "All hit points lost, whatever took them: monster attacks, their missiles and spells, friendly fire, poison and poison clouds, traps, pits, walls run into, closing doors, starvation, exhaustion and harmful potions. Survival lists each cause.", false, false, false, false, false},
    {"Healed", "Hit points won back. The lines below add up to it.", false, false, false, false, false},
    {"Regeneration", "", false, false, false, false, false},
    {"Potions", "", false, false, false, false, false},
    {"Own brews", "Potions the champion brewed and drank.", false, false, false, false, false},
    {"Brewed by others", "Potions another champion brewed.", false, false, false, false, false},
    {"Found", "Potions no champion brewed: found in the dungeon, or brewed before the statistics began.", false, false, false, false, false},
    {"Heal action", "Hit points the HEAL action of an item gave, for the mana it took.", false, false, false, false, false},
    {"Healing given", "HP healed by potions this champion brewed, whoever drank them.", false, false, false, false, false},
    {"Poisoned", "Times poisoned: each new dose of poison counts once, not each tick of it.", false, false, false, false, false},
    {"Wounds", "Parts of the body wounded that were whole before; a part wounded again does not count.", false, false, false, false, false},
    {"Monster attacks received", "Blows of monsters at this champion, whatever came of them. A theft and a monster's missile or spell are not among them.", false, false, false, false, false},
    {"Hits taken", "Attacks that did damage.", false, false, false, false, false},
    {"Dodged", "The champion was quick enough to get out of the way. Never while the party sleeps.", false, false, false, false, false},
    {"Lucky", "The champion's Luck turned the blow aside. Never while the party sleeps.", false, false, false, false, false},
    {"Too weak", "The monster's attack was too weak to get past the champion's skill at parrying.", false, false, false, false, false},
    {"Blocked", "The blow landed, but resistance, shield spells, protection or wisdom left no damage.", false, false, false, false, false},
    {"Damage reduced", "Damage the defences took off every hit, monster or not, counted from what the champion would have taken with no defence at all: with no protection a hit does about twice its roll, and with no resistance a fire or magic hit a third more. The lines below add up to it, but for rounding.", false, false, false, false, false},
    {"Resistance", "Anti-fire against fire, anti-magic against magic, counted from a champion with none: each point takes 1/128 of the roll off, even while a low resistance still leaves more than the roll. Always 0 in games recorded with CSBwin 9.8 to 12.5 or before 9.6: they ignored resistance, as on the Atari.", false, false, false, false, false},
    {"Fire and spell shields", "The party's Fire shield against fire, its Spellshield against magic.", false, false, false, false, false},
    {"Protection", "Against blows, missiles, fire and walls: the lines below make up the champion's protection on the parts of the body hit, and each gets its share. Protection takes off at most 77% of a hit.", false, false, false, false, false},
    {"Armour", "Armour worn on the parts of the body hit.", false, false, false, false, false},
    {"Shields in hand", "", false, false, false, false, false},
    {"Vitality", "A random share of Vitality, part of every champion's protection: some even without armour.", false, false, false, false, false},
    {"Shield potion", "The champion's own shield, from a Ya potion.", false, false, false, false, false},
    {"Party shield", "The party shield spell.", false, false, false, false, false},
    {"Stance", "The champion's last action: some raise protection for a moment, others lower it (below 0).", false, false, false, false, false},
    {"Wound penalty", "A wounded part of the body is less well protected (below 0).", false, false, false, false, false},
    {"Sleep penalty", "Sleep halves protection (below 0).", false, false, false, false, false},
    {"Wisdom", "Against psychic attacks.", false, false, false, false, false},
    {"Deaths", "", false, false, true, false, false},
    {"Resurrected", "Times this champion was brought back from the dead.", false, false, false, false, false},
    {"Party wiped out", "Times every champion was dead at once and the game was over.", true, true, true, false, false},
    {"Reloads", "Times a saved game of this run was loaded again, after a wipe or to try something once more.", true, true, true, false, false},
    {"Eaten", "Items eaten.", false, false, false, false, false},
    {"Drunk", "Drinks, the fountain included.", false, false, false, false, false},
    {"Potions drunk", "Potions drunk: the potions are part of Drunk. A Water Flask or a waterskin is water and no potion.", false, false, false, false, false},
    {"Potions brewed", "Potions this champion made: each spell that filled an empty flask, counted on the level of the cast.", false, false, false, false, false},
    {"Food gained", "Food that went into the champion, in the game's own units: a full stomach holds 2,048.", false, false, false, false, false},
    {"Food wasted (over full)", "Food that did not fit because the champion was full. Not part of Food gained.", false, false, true, false, false},
    {"Water gained", "Water that went into the champion, in the game's own units: a champion holds 2,048, a drink from a fountain gives 1,600.", false, false, false, false, false},
    {"Water wasted (over full)", "Water that did not fit because the champion was full. Not part of Water gained.", false, false, true, false, false},
    {"Items picked up", "Items taken from the dungeon into the hand, each only the first time.", true, false, false, false, false},
    {"Used in walls", "Items a wall took: keys in locks, coins and gems in slots.", true, false, false, false, false},
    {"Steps", "Moves the party made to another square, stairs included. Falls and teleports are no steps.", true, false, false, false, false},
    {"Tiles walked", "The different squares the party stood on; each counts once.", true, false, false, false, false},
    {"Tiles explored", "The squares the map shows as explored that are not solid wall.", true, false, false, false, false},
    {"Turns", "Turns left or right on a square. A turn on stairs takes the stairs, so it is a Step.", true, false, false, false, false},
    {"Wall bumps", "Moves that ran into a wall, a closed door or a solid false wall.", true, false, false, false, false},
    {"Monster bumps", "Moves that were stopped by a monster on the next square.", true, false, false, false, false},
    {"Teleports", "Each teleporter of a chain counts.", true, false, false, false, false},
    {"Telefrags", "Teleports that ended on a square with monsters: the whole group dies at once. The monsters are on the Monsters tab.", true, false, false, false, false},
    {"Telefragged", "The party was teleported onto their square: the whole group dies at once.", true, false, false, false, false},
    {"Landings", "Falls down a pit that ended on a square with monsters: the whole group dies at once. The monsters are on the Monsters tab.", true, false, false, false, false},
    {"Pit falls", "Each pit of a chain counts. Climbing down a rope is a Pit climb.", true, false, false, false, false},
    {"Pit climbs", "Climbs down a rope into a pit, unhurt. Not part of Pit falls.", true, false, false, false, false},
    {"Climb landings", "Climbs down a rope that ended on a square with monsters: the whole group dies at once. The monsters are on the Monsters tab.", true, false, false, false, false},
    {"Arrivals", "Each time the party came onto a level, by stairs, a teleporter, a pit or any other way. The level the party started on is no arrival.", true, false, false, false, false},
    {"Stair changes", "Each time the party went up or down stairs, by a step or a turn on the stairs. Counted on the level it left.", true, false, false, false, false},
    {"Stair dances", "Two stair changes or more one after the other, with no step away from the stairs between them; fights and spells in between are part of the dance. A step to a square next to the stairs and back keeps it going. Counted on the level it began on.", true, false, false, false, false},
    {"Dance moves", "The stair changes in the stair dances after the first, which only brought the party onto the stairs.", true, false, false, false, false},
    {"Strict stair dances", "Runs of stair changes that never left the stairs: any step off them ends one, and the next stair change begins a new one. So one stair dance can hold several of these, or none.", true, false, false, false, false},
    {"Strict dance moves", "The turns on the stairs in the strict stair dances, after the stair change that brought the party onto them.", true, false, false, false, false},
    {"Formation changes", "Times the player moved the champion to another place in the party's formation, by dragging their portrait. Swapped aside is apart; turning the party, recruiting and resurrection are none.", false, false, false, false, false},
    {"Swapped places", "The champion's portrait dropped on another champion: the two traded places. Part of Formation changes.", false, false, false, false, false},
    {"To an empty place", "The champion's portrait dropped on an empty place. Part of Formation changes.", false, false, false, false, false},
    {"Put back in place", "The champion's portrait picked up and dropped where it was: nothing changed. Not part of Formation changes.", false, false, false, false, false},
    {"Swapped aside", "Another champion's portrait dropped on the champion, who went to the place it came from. The other side of a swap: for the party the same count as Swapped places. Not part of Formation changes.", false, false, false, false, false},
    {"Time (ticks)", "Game ticks the party spent, on the level it was on, sleep included.", true, false, false, false, true},
    {"Asleep (ticks)", "Game ticks the party slept. Part of Time.", true, false, false, false, true},
    {"Doors bashed open", "Doors the champion broke: the lines below add up to it.", false, false, false, false, false},
    {"Smashed", "Broken with a blow.", false, false, false, false, false},
    {"Shot open", "Broken by a thrown item or a shot.", false, false, false, false, false},
    {"By spell", "Broken by a spell, a fireball or the like.", false, false, false, false, false},
    {"Opened with Zo", "Closed doors a Zo spell opened; the door stays whole. Not part of Doors bashed open.", false, false, false, false, false},
    {"Opened with button", "Door buttons pressed while the door was closed.", true, false, false, false, false},
    {"Closed with button", "Door buttons pressed while the door was open.", true, false, false, false, false},
    {"Chests opened", "The different chests opened; each counts once.", true, false, false, false, false},
    {"Scrolls read", "Scrolls read, each the first time it is opened.", true, false, false, false, false},
    {"Wall texts read", "The wall texts the map knows as read.", true, false, false, false, false},
    {"Champions seen", "Champion portraits in mirrors seen face to face; each mirror once.", true, false, false, false, false},
    {"Champions viewed", "Mirrors clicked to look at the champion; each mirror once.", true, false, false, false, false},
    {"By doors", "Monsters a closing door crushed.", true, false, false, false, false},
    {"Into pits", "Monsters killed by their own fall into a pit.", true, false, false, false, false},
    {"Other causes", "Clouds nobody is known to have made, traps, other monsters.", true, false, false, false, false},
    {"Monsters killed", "Every monster killed, whatever the cause. The lines below add up to it.", true, false, false, false, false},
    {"Landed on", "The party came down a pit onto their square: the whole group dies at once.", true, false, false, false, false},
    {"Experience gained", "Experience as the game added it: with the level's multiplier, the bonus in combat and the dungeon's own rules.", false, false, false, false, false},
    {"Fighter", "", false, false, false, false, false},
    {"Ninja", "", false, false, false, false, false},
    {"Priest", "", false, false, false, false, false},
    {"Wizard", "", false, false, false, false, false},
    {"Ranks gained", "", false, false, false, false, false},
    {"Biggest hit", "The most damage a blow, a thrown item or a shot did to one monster.", false, false, false, false, false, true},
    {"Biggest spell hit", "The most damage one monster took from one spell.", false, false, false, false, false, true},
    {"Biggest spell in all", "The most damage one spell did to a whole group.", false, false, false, false, false, true},
    {"Most kills at once", "Monsters killed by one blow, one explosion or one tick of a cloud.", false, false, false, false, false, true},
    {"Kills without a scratch", "The most kills in a row with no damage taken in between.", false, false, false, false, false, true},
    {"Biggest hit taken", "The most damage one monster attack did, after armour.", false, false, false, false, false, true},
    {"Biggest spell hit taken", "The most damage one monster spell or missile did.", false, false, false, false, false, true},
    {"Closest call", "The fewest hit points left after taking damage and living.", false, false, false, false, false, true},
    {"Biggest heal", "The most hit points one potion, one heal action or one tick of regeneration gave.", false, false, false, false, false, true},
    {"Most mana on one spell", "The mana of the runes of one spell that worked.", false, false, false, false, false, true},
    {"Biggest experience gain", "The most experience one action, spell or monster attack gave in one skill.", false, false, false, false, false, true},
    {"Heaviest load", "The most weight carried, and the share of what the champion could carry.", false, false, false, false, false, true},
    {"Longest fall", "Pits fallen through one after the other.", true, false, false, false, false, true},
    {"Longest teleport chain", "Teleporters passed one after the other without the party taking a step.", true, false, false, false, false, true},
    {"Longest sleep", "Ticks of the longest sleep without waking.", true, false, false, false, true, true},
    {"Most poison at once", "The most poison working on a champion at one time, all doses added up; it is the number of poison ticks still to come.", false, false, false, false, false, true},
    {"Longest poisoned", "Ticks of the longest time a champion stayed poisoned without a break, with the poison damage taken in it. Death or a cure ends it.", false, false, false, false, true, true},
    {"Most poisonings in a row", "The most new doses of poison taken within one stretch of being poisoned.", false, false, false, false, false, true},
    {"Most wounds at once", "The most wounded parts of the body of one champion at the same time.", false, false, false, false, false, true},
    {"Most wounds from one blow", "The most parts of the body one blow, missile or explosion newly wounded.", false, false, false, false, false, true},
    {"Poison doses suffered", "All the doses of poison a champion took in the run (or on the level): each new dose counts once.", false, false, false, false, false, true},
    {"Wounds suffered", "All the parts of the body wounded that were whole before, added up over the run (or the level).", false, false, false, false, false, true},
    {"Most poison doses at once", "The most doses of poison working on a champion at the same time.", false, false, false, false, false, true},
    {"Most poison per 36 ticks", "The most HP a champion's poison could take at one poison tick (every 36 ticks), all doses added up.", false, false, false, false, false, true},
    {"Biggest melee hit", "The most damage a blow at close range, with a weapon or an empty hand, did to one monster.", false, false, false, false, false, true},
    {"Biggest ranged hit", "The most damage a thrown item or a shot did to one monster.", false, false, false, false, false, true},
    {"Longest stair dance", "The most dance moves in one stair dance; a step to a square next to the stairs and back keeps it going.", true, false, false, false, false, true},
    {"Longest strict stair dance", "The most dance moves in one stair dance that never left the stairs.", true, false, false, false, false, true},
};

// The names csb.cfg knows the numbers by, in the order of HeroStatsNumber
static const char *const KEYS[] = {
    "damage", "melee", "ranged", "magic", "kills", "attacks", "hits", "hit_rate", "damage_per_hit", "other_actions",
    "friendly_fire",
    "mana_generated", "mana_regeneration", "mana_potions", "mana_regen_lost", "mana_spent", "mana_successful",
    "mana_success_rate", "mana_wasted", "mana_failed", "mana_meaningless",
    "runes_cleared", "runes_not_cast",
    "casts", "casts_successful", "casts_success_rate", "casts_wasted", "casts_failed", "casts_meaningless",
    "taken", "healed", "heal_regeneration", "heal_potions", "heal_potions_own",
    "heal_potions_others", "heal_potions_found", "heal_action", "healing_given", "poisoned", "wounds",
    "attacks_received", "hits_taken", "dodged", "lucky", "too_weak", "blocked",
    "damage_reduced", "reduced_resistance", "reduced_shield_spells", "reduced_protection", "reduced_armour",
    "reduced_hand_shields", "reduced_vitality", "reduced_shield_potion", "reduced_party_shield",
    "reduced_stance", "reduced_wounds", "reduced_asleep", "reduced_wisdom",
    "deaths", "resurrected", "wipes", "reloads",
    "eaten", "drunk", "potions_drunk", "potions_brewed", "food_gained", "food_wasted", "water_gained", "water_wasted",
    "picked_up", "used_in_walls",
    "steps", "tiles_walked", "tiles_explored", "turns", "wall_bumps", "monster_bumps", "teleports", "telefrags", "telefragged", "landings",
    "pit_falls", "pit_climbs", "climb_landings", "arrivals", "stair_changes",
    "stair_dances", "stair_dance_moves", "strict_stair_dances", "strict_stair_dance_moves",
    "formation_changes", "formation_swaps", "formation_moves", "formation_put_backs", "formation_swapped_aside", "ticks", "sleep_ticks",
    "doors_bashed", "doors_smashed", "doors_shot", "doors_by_spell", "doors_by_zo",
    "doors_button_opened", "doors_button_closed", "chests", "scrolls", "wall_texts",
    "champions_seen", "champions_viewed",
    "door_kills", "pit_kills", "other_kills", "monsters_killed", "landed_on",
    "experience", "fighter", "ninja", "priest", "wizard", "ranks_gained",
    "biggest_hit", "biggest_spell_hit", "biggest_spell_total", "most_kills_at_once", "kill_streak",
    "biggest_hit_taken", "biggest_spell_hit_taken", "closest_call", "biggest_heal", "most_mana_on_spell",
    "biggest_experience_gain", "heaviest_load", "longest_fall", "longest_teleport_chain", "longest_sleep",
    "most_poison", "longest_poisoned", "most_poisonings", "most_wounds", "most_wounds_one_blow",
    "poison_doses_suffered", "wounds_suffered", "most_poison_doses", "most_poison_per_36_ticks",
    "biggest_melee_hit", "biggest_ranged_hit", "longest_stair_dance", "longest_strict_stair_dance"};
static_assert(sizeof KEYS / sizeof KEYS[0] == HERO_NUMBER_COUNT, "a name for every number");

const char *const HERO_STATS_BY_CHAMPIONS_HELP =
    "The part of Monsters killed that the champions killed. The rest were crushed by doors, fell into pits, were telefragged, landed on or died some other way.";
const char *const HERO_STATS_BEST_RANK_HELP =
    "Best: the highest rank now in this class. Champions who share it are all named.";
const char *const HERO_STATS_AVG_HELP =
    "Damage \xc3\xb7 Hits, as Damage per hit on the Combat page. The spells a weapon or staff casts (FIREBALL, INVOKE...) are in it: "
    "a fireball that hurts a whole group is one hit with all of its damage, so they raise it. Open the weapon for each option's own.";
const char *const HERO_STATS_RECORDS_SORT_HELP =
    "Click a header to sort the records within each group; a click on Record puts them back in their order.";
const char *const HERO_STATS_WEAPON_DAMAGE_HELP =
    "Blows, missiles and the spells the weapon or staff casts. With the Damage of the Spells table it adds up to Damage dealt.";
const char *const HERO_STATS_SPELL_DAMAGE_HELP =
    "Damage of the spells entered as runes. The spells of staffs, wands and weapons count with their item in the Weapons table, "
    "so Spells from items has mana but no damage. With the Damage of the Weapons table it adds up to Damage dealt.";
const char *const HERO_STATS_SPELL_HELPS[HERO_STATS_SPELL_COLUMNS] = {
    "",
    "Casts of the spell, the wasted ones included.",
    "The casts that worked.",
    "Successful \xc3\xb7 All: the part of the casts that worked. A rate cannot be shared out: sorted by it, the Who bar splits All, and its hovertext has each champion's rate.",
    "Casts that bought nothing. On a spell's line they failed: the champion needed more practice. On a (no spell) line the "
    "runes formed no spell. The Wasted casts of All spells is Wasted under Spells cast in Numbers.",
    "The mana of the runes, the wasted casts' included. For the whole run, the MANA All of All spells is Mana spent without Not cast yet.",
    "The mana of the casts that worked.",
    "Successful \xc3\xb7 All: the part of the mana that bought a spell that worked. It can differ from the casts' Rate "
    "both ways: a failed cast at a high power wastes more mana than a weak one, and a potion spell cast with no empty "
    "flask fails but keeps its runes for the next try (a wasted cast, no wasted mana). The Runes cleared line has no "
    "rate: it is no cast. The MANA Rate of All spells is Success rate under Mana spent in Numbers. A rate cannot be shared out: "
    "sorted by it, the Who bar splits All, and its hovertext has each champion's rate.",
    "The mana of the wasted casts, and on the Runes cleared line the mana of runes never cast. The Wasted mana of All spells is Mana wasted in Numbers.",
    HERO_STATS_SPELL_DAMAGE_HELP,
    ""};

const HeroStatsNumberInfo &HeroStatsCatalogue_Info(HeroStatsNumber number)
{
  return NUMBERS[number];
}

const char *HeroStatsCatalogue_RowLabel(const HeroStatsTopicRow &row)
{
  return row.label != NULL ? row.label : NUMBERS[row.number].label;
}

// The topic's rows are those of its groups, in order
static void SetGroups(HeroStatsTopicInfo *topic, const std::vector<HeroStatsTopicGroup> &groups)
{
  topic->groups = groups;
  topic->rows.clear();
  for (size_t g = 0; g < groups.size(); g++) topic->rows.insert(topic->rows.end(), groups[g].rows.begin(), groups[g].rows.end());
}

static void SetRows(HeroStatsTopicInfo *topic, const std::vector<HeroStatsTopicRow> &rows)
{
  SetGroups(topic, {{"", false, rows}});
}

static std::vector<HeroStatsTopicInfo> MakeTopics(void)
{
  std::vector<HeroStatsTopicInfo> topics(HERO_TOPIC_COUNT);
  topics[HeroTopicOverview] = {"Overview", {}, {HeroNumDamage, HeroNumKills, HeroNumDeaths, HeroNumTilesExplored}, {}};
  SetRows(&topics[HeroTopicOverview],
          {{HeroNumDamage, 0}, {HeroNumKills, 0}, {HeroNumTaken, 0}, {HeroNumHealed, 0},
           {HeroNumManaSpent, 0}, {HeroNumDeaths, 0}, {HeroNumEaten, 0}, {HeroNumDrunk, 0},
           {HeroNumDoorsBashed, 0}, {HeroNumSteps, 0}, {HeroNumTilesExplored, 0}, {HeroNumTicks, 0}});
  topics[HeroTopicCombat] = {"Combat", {}, {HeroNumDamage, HeroNumKills, HeroNumHitRate, HeroNumFriendlyFire}, {}};
  SetGroups(&topics[HeroTopicCombat],
            {{"Dealing damage", false,
              {{HeroNumDamage, 0}, {HeroNumMelee, 1}, {HeroNumRanged, 1}, {HeroNumMagic, 1}, {HeroNumKills, 0}, {HeroNumFriendlyFire, 0}}},
             {"Attacks", false,
              {{HeroNumAttacks, 0}, {HeroNumHits, 1}, {HeroNumHitRate, 1}, {HeroNumDamagePerHit, 1}, {HeroNumOtherActions, 0}}}});
  topics[HeroTopicMagic] = {"Magic", {}, {HeroNumManaGenerated, HeroNumManaSpent, HeroNumCasts, HeroNumManaWasted}, {}};
  SetGroups(&topics[HeroTopicMagic],
            {{"Mana in", false,
              {{HeroNumManaGenerated, 0}, {HeroNumManaRegeneration, 1}, {HeroNumManaPotions, 1}, {HeroNumManaRegenLost, 0}}},
             {"Mana out", false,
              {{HeroNumManaSpent, 0}, {HeroNumManaSuccessful, 1, "On successful casts"}, {HeroNumManaSuccessRate, 1, "Success rate"},
               {HeroNumManaWasted, 1, "Wasted"}, {HeroNumManaFailed, 2, "On failed casts"},
               {HeroNumManaMeaningless, 2, "On meaningless spells"}, {HeroNumRunesCleared, 2, "On cleared runes"},
               {HeroNumRunesNotCast, 1}}},
             {"Casts", false,
              {{HeroNumCasts, 0}, {HeroNumCastsSuccessful, 1}, {HeroNumCastsSuccessRate, 1, "Success rate"}, {HeroNumCastsWasted, 1},
               {HeroNumCastsFailed, 2}, {HeroNumCastsMeaningless, 2}}}});
  topics[HeroTopicSurvival] = {"Survival", {}, {HeroNumTaken, HeroNumHealed, HeroNumDeaths, HeroNumResurrected}, {}};
  SetGroups(&topics[HeroTopicSurvival],
            {{"Harm", false, {{HeroNumTaken, 0}, {HeroNumPoisonings, 0}, {HeroNumWounds, 0}}},
             {"Monster attacks", false,
              {{HeroNumAttacksReceived, 0}, {HeroNumHitsTaken, 1}, {HeroNumDodged, 1}, {HeroNumLucky, 1}, {HeroNumTooWeak, 1},
               {HeroNumBlocked, 1}}},
             {"Damage reduced", false,
              {{HeroNumReduced, 0}, {HeroNumReducedResistance, 1}, {HeroNumReducedShieldSpells, 1}, {HeroNumReducedProtection, 1},
               {HeroNumReducedArmour, 2}, {HeroNumReducedHandShields, 2}, {HeroNumReducedVitality, 2},
               {HeroNumReducedChampionShield, 2}, {HeroNumReducedPartyShield, 2}, {HeroNumReducedStance, 2},
               {HeroNumReducedWounds, 2}, {HeroNumReducedAsleep, 2}, {HeroNumReducedWisdom, 1}}},
             {"Healing", false,
              {{HeroNumHealed, 0}, {HeroNumHealRegeneration, 1}, {HeroNumHealPotions, 1}, {HeroNumHealPotionsOwn, 2},
               {HeroNumHealPotionsOthers, 2}, {HeroNumHealPotionsFound, 2}, {HeroNumHealAction, 1}, {HeroNumHealingGiven, 0}}},
             {"Deaths", false, {{HeroNumDeaths, 0}, {HeroNumResurrected, 0}}},
             {"Wipes and reloads", true, {{HeroNumWipes, 0}, {HeroNumReloads, 0}}}});
  topics[HeroTopicSupplies] = {"Food & items", {}, {HeroNumFoodGained, HeroNumWaterGained, HeroNumPotionsDrunk, HeroNumPickedUp}, {}};
  SetGroups(&topics[HeroTopicSupplies],
            {{"Food and water", false,
              {{HeroNumEaten, 0}, {HeroNumDrunk, 0}, {HeroNumPotionsDrunk, 1}, {HeroNumPotionsBrewed, 0}, {HeroNumFoodGained, 0}, {HeroNumFoodWasted, 0},
               {HeroNumWaterGained, 0}, {HeroNumWaterWasted, 0}}},
             {"Items", true, {{HeroNumPickedUp, 0}, {HeroNumUsedInWalls, 0}}}});
  topics[HeroTopicExploration] = {"Exploration", {}, {HeroNumTilesExplored, HeroNumSteps, HeroNumTicks, HeroNumTeleports}, {}};
  SetGroups(&topics[HeroTopicExploration],
            {{"Movement", true,
              {{HeroNumSteps, 0}, {HeroNumTurns, 0}, {HeroNumTilesWalked, 0}, {HeroNumTilesExplored, 0}, {HeroNumBumps, 0}, {HeroNumMonsterBumps, 0}}},
             {"Travel", true,
              {{HeroNumTeleports, 0}, {HeroNumTelefrags, 1, "onto monsters"}, {HeroNumPitFalls, 0}, {HeroNumLandings, 1, "onto monsters"},
               {HeroNumPitClimbs, 0}, {HeroNumClimbLandings, 1, "onto monsters"}, {HeroNumArrivals, 0}, {HeroNumStairChanges, 0},
               {HeroNumStairDances, 0}, {HeroNumStairDanceMoves, 1}, {HeroNumStrictStairDances, 0}, {HeroNumStrictStairDanceMoves, 1}}},
             {"Time", true, {{HeroNumTicks, 0}, {HeroNumSleepTicks, 1, "Asleep"}}},
             {"Discoveries", true,
              {{HeroNumChests, 0}, {HeroNumScrolls, 0}, {HeroNumWallTexts, 0}, {HeroNumChampionsSeen, 0}, {HeroNumChampionsViewed, 0}}},
             {"Doors", false,
              {{HeroNumDoorsBashed, 0}, {HeroNumDoorsSmashed, 1}, {HeroNumDoorsShot, 1}, {HeroNumDoorsBySpell, 1}, {HeroNumDoorsByZo, 0}}},
             {"Door buttons", true, {{HeroNumDoorsButtonOpened, 0}, {HeroNumDoorsButtonClosed, 0}}},
             // Per champion, after the doors: a champion's tiles and summary begin with the doors
             {"Formation", false,
              {{HeroNumFormationChanges, 0}, {HeroNumFormationSwaps, 1}, {HeroNumFormationMoves, 1}, {HeroNumFormationPutBacks, 0},
               {HeroNumFormationSwappedAside, 0}}}});
  // The six causes add up to Monsters killed
  topics[HeroTopicMonsters] = {"Monsters", {}, {HeroNumMonstersKilled, HeroNumKills, HeroNumTelefragged, HeroNumLandedOn}, {}};
  SetRows(&topics[HeroTopicMonsters],
          {{HeroNumMonstersKilled, 0}, {HeroNumKills, 1, "By champions"}, {HeroNumDoorKills, 1}, {HeroNumPitKills, 1},
           {HeroNumTelefragged, 1}, {HeroNumLandedOn, 1}, {HeroNumOtherKills, 1}});
  topics[HeroTopicExperience] = {"Experience", {}, {HeroNumXpFighter, HeroNumXpNinja, HeroNumXpPriest, HeroNumXpWizard}, {}};
  SetRows(&topics[HeroTopicExperience],
          {{HeroNumExperience, 0}, {HeroNumXpFighter, 1}, {HeroNumXpNinja, 1}, {HeroNumXpPriest, 1}, {HeroNumXpWizard, 1}, {HeroNumRanksGained, 0}});
  topics[HeroTopicRecords] = {"Records", {},
                              {HeroNumRecBiggestMeleeHit, HeroNumRecBiggestRangedHit, HeroNumRecBiggestSpellTotal, HeroNumRecBiggestHitTaken}, {}};
  // The melee and ranged hits first, where the biggest hit was; that one is no longer shown
  std::vector<HeroStatsTopicRow> records;
  records.push_back({HeroNumRecBiggestMeleeHit, 0});
  records.push_back({HeroNumRecBiggestRangedHit, 0});
  for (int k = 0; k < HERO_RECORD_COUNT; k++)
    if (k != HeroRecBiggestHit && k != HeroRecBiggestMeleeHit && k != HeroRecBiggestRangedHit &&
        k != HeroRecLongestStairDance && k != HeroRecLongestStrictStairDance &&
        // Poisoned and Wounds on the Survival tab
        k != HeroRecTotalPoisonings && k != HeroRecTotalWounds)
    {
      records.push_back({(HeroStatsNumber)(HeroNumRecFirst + k), 0});
      // The stair dances with the other chains of moves
      if (k == HeroRecLongestTeleportChain)
      {
        records.push_back({HeroNumRecLongestStairDance, 0});
        records.push_back({HeroNumRecLongestStrictStairDance, 0});
      }
    }
  SetRows(&topics[HeroTopicRecords], records);
  return topics;
}

static const std::vector<HeroStatsNumber> CHAMPION_TILES = {HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumDeaths};

// As MakeTopics made them, with the tiles and Key numbers csb.cfg chose
static std::vector<HeroStatsTopicInfo> &Topics(void)
{
  static std::vector<HeroStatsTopicInfo> topics = MakeTopics();
  return topics;
}

static std::vector<HeroStatsNumber> &ChampionTiles(void)
{
  static std::vector<HeroStatsNumber> tiles = CHAMPION_TILES;
  return tiles;
}

const HeroStatsTopicInfo &HeroStatsCatalogue_Topic(int topic)
{
  if (topic < 0 || topic >= HERO_TOPIC_COUNT) topic = HeroTopicOverview;
  return Topics()[topic];
}

const std::vector<HeroStatsNumber> &HeroStatsCatalogue_ChampionTiles(void)
{
  return ChampionTiles();
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_DefaultTiles(int topic)
{
  if (topic < 0 || topic >= HERO_TOPIC_COUNT) topic = HeroTopicOverview;
  return MakeTopics()[topic].tiles;
}

static std::vector<HeroStatsNumber> DefaultKeyNumbers(void)
{
  std::vector<HeroStatsNumber> numbers;
  std::vector<HeroStatsTopicRow> rows = MakeTopics()[HeroTopicOverview].rows;
  for (size_t r = 0; r < rows.size(); r++) numbers.push_back(rows[r].number);
  return numbers;
}

std::string HeroStatsCatalogue_Key(HeroStatsNumber number)
{
  return KEYS[number];
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_ParseNumbers(const std::string &text, size_t most)
{
  std::vector<HeroStatsNumber> numbers;
  size_t at = 0;
  while (at < text.size() && numbers.size() < most)
  {
    size_t start = text.find_first_not_of(" ,\t", at);
    if (start == std::string::npos) break;
    size_t end = text.find_first_of(" ,\t", start);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(start, end - start);
    for (size_t i = 0; i < word.size(); i++) word[i] = (char)tolower((unsigned char)word[i]);
    // The biggest hit is no longer shown: a csb.cfg that names it gets the biggest melee hit
    if (word == "biggest_hit") word = "biggest_melee_hit";
    // Telefrag kills became Telefragged
    if (word == "telefrag_kills") word = "telefragged";
    for (int n = 0; n < HERO_NUMBER_COUNT; n++)
      if (word == KEYS[n] && std::find(numbers.begin(), numbers.end(), (HeroStatsNumber)n) == numbers.end())
        numbers.push_back((HeroStatsNumber)n);
    at = end;
  }
  return numbers;
}

std::string HeroStatsCatalogue_JoinNumbers(const std::vector<HeroStatsNumber> &numbers)
{
  std::string text;
  for (size_t i = 0; i < numbers.size(); i++) text += (i > 0 ? ", " : "") + std::string(KEYS[numbers[i]]);
  return text;
}

static const int MOST_TILES = 4, MOST_KEY_NUMBERS = 24;

struct SettingInfo { const char *entry; int topic; };  // topic -1: the champion's Overview tiles
static const SettingInfo SETTINGS[HERO_SETTING_COUNT] = {
    {"stats_tiles_overview", HeroTopicOverview}, {"stats_tiles_champion", -1}, {"stats_tiles_combat", HeroTopicCombat},
    {"stats_tiles_magic", HeroTopicMagic}, {"stats_tiles_survival", HeroTopicSurvival}, {"stats_tiles_food", HeroTopicSupplies},
    {"stats_tiles_exploration", HeroTopicExploration}, {"stats_tiles_monsters", HeroTopicMonsters},
    {"stats_tiles_experience", HeroTopicExperience}, {"stats_tiles_records", HeroTopicRecords},
    {"stats_key_numbers", HeroTopicOverview}};

const char *HeroStatsCatalogue_SettingEntry(HeroStatsSetting setting)
{
  return SETTINGS[setting].entry;
}

static std::vector<HeroStatsNumber> SettingDefaults(HeroStatsSetting setting)
{
  if (setting == HeroSettingKeyNumbers) return DefaultKeyNumbers();
  if (setting == HeroSettingChampionTiles) return CHAMPION_TILES;
  return HeroStatsCatalogue_DefaultTiles(SETTINGS[setting].topic);
}

std::string HeroStatsCatalogue_SettingDefault(HeroStatsSetting setting)
{
  return HeroStatsCatalogue_JoinNumbers(SettingDefaults(setting));
}

std::string HeroStatsCatalogue_CurrentSetting(HeroStatsSetting setting, const std::string &text)
{
  static const std::vector<HeroStatsNumber> monstersBefore = {HeroNumKills, HeroNumDoorKills, HeroNumPitKills, HeroNumOtherKills};
  static const std::vector<HeroStatsNumber> explorationBefore = {HeroNumTilesExplored, HeroNumSteps, HeroNumTicks, HeroNumDoorsBashed};
  static const std::vector<HeroStatsNumber> suppliesBefore = {HeroNumEaten, HeroNumDrunk, HeroNumFoodWasted, HeroNumPickedUp};
  const std::vector<HeroStatsNumber> *before = setting == HeroSettingMonstersTiles      ? &monstersBefore
                                               : setting == HeroSettingExplorationTiles ? &explorationBefore
                                               : setting == HeroSettingSuppliesTiles    ? &suppliesBefore
                                                                                        : NULL;
  if (before == NULL) return text;
  if (HeroStatsCatalogue_ParseNumbers(text, MOST_TILES + 1) != *before) return text;
  return HeroStatsCatalogue_SettingDefault(setting);
}

void HeroStatsCatalogue_ApplySetting(HeroStatsSetting setting, const std::string &text)
{
  bool keyNumbers = setting == HeroSettingKeyNumbers;
  std::vector<HeroStatsNumber> numbers = HeroStatsCatalogue_ParseNumbers(text, keyNumbers ? MOST_KEY_NUMBERS : MOST_TILES);
  if (numbers.empty()) numbers = SettingDefaults(setting);
  if (keyNumbers)
  {
    std::vector<HeroStatsTopicRow> rows;
    for (size_t i = 0; i < numbers.size(); i++) rows.push_back({numbers[i], 0});
    SetRows(&Topics()[HeroTopicOverview], rows);
  }
  else if (setting == HeroSettingChampionTiles) ChampionTiles() = numbers;
  else Topics()[SETTINGS[setting].topic].tiles = numbers;
}

// part / whole as a rounded whole number, -1 when whole is 0
static int RoundedRatio(long long part, long long whole)
{
  if (whole <= 0) return -1;
  return (int)((part * 2 + whole) / (whole * 2));
}

// The numbers come in groups that one look at the store fills together. Record fills them all;
// Value only the group of its number, which is what Per level needs for one number per champion.
typedef void (*FillGroup)(const HeroStats &stats, HeroStatsChampion who, int level, int *v);

static void FillDamage(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumMelee] = stats.DamageDealt(who, level, HeroDamageMelee);
  v[HeroNumRanged] = stats.DamageDealt(who, level, HeroDamageRanged);
  v[HeroNumMagic] = stats.DamageDealt(who, level, HeroDamageMagic);
  v[HeroNumDamage] = v[HeroNumMelee] + v[HeroNumRanged] + v[HeroNumMagic];
}

static void FillKills(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumKills] = stats.Kills(who, level);
}

// As the Weapons table counts them: spells entered as runes are no attacks; the actions that
// cannot hit are no attacks either, they are Other actions
static void FillAttacks(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  int attackDamage = 0;
  v[HeroNumAttacks] = v[HeroNumHits] = v[HeroNumOtherActions] = 0;
  std::vector<HeroStatsAttackRow> attacks = stats.Attacks(who, level);
  for (size_t i = 0; i < attacks.size(); i++)
  {
    if (attacks[i].weapon == HERO_STATS_NO_WEAPON) continue;
    v[HeroNumAttacks] += attacks[i].tries;
    v[HeroNumOtherActions] += attacks[i].uses;
    v[HeroNumHits] += attacks[i].hits;
    attackDamage += attacks[i].damage;
  }
  v[HeroNumHitRate] = RoundedRatio((long long)v[HeroNumHits] * 100, v[HeroNumAttacks]);
  v[HeroNumDamagePerHit] = RoundedRatio(attackDamage, v[HeroNumHits]);
}

static void FillFriendlyFire(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumFriendlyFire] = stats.FriendlyFireCaused(who, level);
}

static void FillMana(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumManaRegeneration] = stats.ManaGainedBy(who, level, HeroManaRegeneration);
  v[HeroNumManaPotions] = stats.ManaGainedBy(who, level, HeroManaPotion);
  v[HeroNumManaRegenLost] = stats.ManaGainedBy(who, level, HeroManaRegenLost);
  v[HeroNumManaGenerated] = stats.ManaGained(who, level);
  v[HeroNumManaSpent] = stats.ManaSpent(who, level);
  v[HeroNumManaFailed] = stats.ManaWasted(who, level);
  v[HeroNumManaMeaningless] = stats.ManaMeaningless(who, level);
  v[HeroNumRunesCleared] = stats.RunesCleared(who, level);
  v[HeroNumManaSuccessful] = stats.ManaOnSuccessfulCasts(who, level);
  v[HeroNumRunesNotCast] = stats.RunesNotCastYet(who, level);
  // Wasted: all the mana that bought nothing; the success rate is of the mana whose runes came to
  // an end, so runes not cast yet leave it alone
  v[HeroNumManaWasted] = v[HeroNumManaFailed] + v[HeroNumManaMeaningless] + v[HeroNumRunesCleared];
  v[HeroNumManaSuccessRate] = RoundedRatio((long long)v[HeroNumManaSuccessful] * 100, v[HeroNumManaSuccessful] + v[HeroNumManaWasted]);
}

static void FillCasts(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumCasts] = v[HeroNumCastsSuccessful] = v[HeroNumCastsWasted] = v[HeroNumCastsFailed] = v[HeroNumCastsMeaningless] = 0;
  std::vector<HeroStatsSpellRow> spells = stats.Spells(who, level);
  for (size_t i = 0; i < spells.size(); i++)
  {
    v[HeroNumCasts] += spells[i].casts + spells[i].failed;
    v[HeroNumCastsSuccessful] += spells[i].casts;
    v[HeroNumCastsWasted] += spells[i].failed;
    // Runes that form no spell fail as a meaningless spell, apart from the real spells that failed
    v[(spells[i].spell & HERO_SPELL_NO_SUCH_SPELL) != 0 ? HeroNumCastsMeaningless : HeroNumCastsFailed] += spells[i].failed;
  }
  v[HeroNumCastsSuccessRate] = RoundedRatio((long long)v[HeroNumCastsSuccessful] * 100, v[HeroNumCasts]);
}

static void FillTaken(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumTaken] = stats.Harm(who, level);
}

static void FillHealing(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumHealed] = stats.Healed(who, level);
  v[HeroNumHealRegeneration] = v[HeroNumHealPotions] = v[HeroNumHealAction] = 0;
  std::vector<HeroStatsHealRow> heals = stats.HealRows(who, level);
  for (size_t i = 0; i < heals.size(); i++)
  {
    if (heals[i].kind == HeroHealRegeneration) v[HeroNumHealRegeneration] += heals[i].amount;
    else if (heals[i].kind == HeroHealPotion) v[HeroNumHealPotions] += heals[i].amount;
    else v[HeroNumHealAction] += heals[i].amount;
  }
  stats.PotionHealingByBrewer(who, level, &v[HeroNumHealPotionsOwn], &v[HeroNumHealPotionsOthers], &v[HeroNumHealPotionsFound]);
  v[HeroNumHealingGiven] = stats.HealingGiven(who, level);
}

static void FillDeaths(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumDeaths] = 0;
  std::vector<HeroStatsDeath> deaths = stats.Deaths(who);
  for (size_t i = 0; i < deaths.size(); i++)
    if (level == HERO_STATS_WHOLE_RUN || deaths[i].level == level) v[HeroNumDeaths]++;
  v[HeroNumResurrected] = stats.Resurrections(who, level);
}

static void FillFood(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumEaten] = stats.Eaten(who, level);
  HeroStatsNourishment food = stats.Nourishment(who, level);
  v[HeroNumDrunk] = stats.Drunk(who, level) + food.fountainDrinks;  // the fountain is a way of drinking
  v[HeroNumPotionsDrunk] = stats.Potions(who, level);
  v[HeroNumFoodGained] = food.foodGained;
  v[HeroNumFoodWasted] = food.foodWasted;
  v[HeroNumWaterGained] = food.waterGained;
  v[HeroNumWaterWasted] = food.waterWasted;
}

static void FillDoors(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumDoorsBashed] = stats.DoorsBashed(who, level);
  v[HeroNumDoorsSmashed] = stats.DoorsBashed(who, level, HeroDoorSmashed);
  v[HeroNumDoorsShot] = stats.DoorsBashed(who, level, HeroDoorShot);
  v[HeroNumDoorsBySpell] = stats.DoorsBashed(who, level, HeroDoorBySpell);
  v[HeroNumDoorsByZo] = stats.DoorsOpenedByZo(who, level);
}

// The party's only: who is HERO_STATS_PARTY
static void FillHardship(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  bool wholeRun = level == HERO_STATS_WHOLE_RUN;
  v[HeroNumWipes] = wholeRun ? stats.PartyWipes() : 0;
  v[HeroNumReloads] = wholeRun ? stats.Reloads() : 0;
}

static void FillItems(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumPickedUp] = v[HeroNumUsedInWalls] = 0;
  std::vector<HeroStatsItemRow> items = stats.Items(level);
  for (size_t i = 0; i < items.size(); i++)
  {
    v[HeroNumPickedUp] += items[i].pickedUp;
    v[HeroNumUsedInWalls] += items[i].usedInWall;
  }
}

static void FillMovement(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  HeroStatsMovement move = stats.Movement(level);
  v[HeroNumSteps] = move.steps;
  v[HeroNumTilesWalked] = move.tilesWalked;
  v[HeroNumTilesExplored] = move.tilesExplored;
  v[HeroNumTurns] = move.turns;
  v[HeroNumBumps] = move.bumps;
  v[HeroNumMonsterBumps] = move.monsterBumps;
  v[HeroNumTeleports] = move.teleports;
  v[HeroNumTelefrags] = move.telefrags;
  v[HeroNumLandings] = move.landings;
  v[HeroNumPitFalls] = move.pitFalls;
  v[HeroNumPitClimbs] = move.pitClimbs;
  v[HeroNumClimbLandings] = move.climbLandings;
  v[HeroNumArrivals] = move.arrivals;
  v[HeroNumStairChanges] = move.stairChanges;
  v[HeroNumStairDances] = move.stairDances;
  v[HeroNumStairDanceMoves] = move.stairDanceMoves;
  v[HeroNumStrictStairDances] = move.strictStairDances;
  v[HeroNumStrictStairDanceMoves] = move.strictStairDanceMoves;
  v[HeroNumTicks] = move.ticks;
  v[HeroNumSleepTicks] = move.sleepTicks;
}

static void FillDungeon(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumDoorsButtonOpened] = stats.DoorsOpenedByButton(level);
  v[HeroNumDoorsButtonClosed] = stats.DoorsClosedByButton(level);
  v[HeroNumChests] = stats.ChestsOpened(level);
  v[HeroNumScrolls] = stats.ScrollsRead(level);
  v[HeroNumWallTexts] = stats.WallTextsRead(level);
  v[HeroNumChampionsSeen] = stats.ChampionsSeen(level);
  v[HeroNumChampionsViewed] = stats.ChampionsViewed(level);
}

// Every kill by its cause: a door, a fall, a telefrag, a landing, or any other cause but a
// champion's blow. The causes and the champions' Kills add up to Monsters killed.
static void FillKillCauses(const HeroStats &stats, HeroStatsChampion, int level, int *v)
{
  v[HeroNumDoorKills] = v[HeroNumPitKills] = v[HeroNumOtherKills] = v[HeroNumTelefragged] = v[HeroNumLandedOn] = 0;
  v[HeroNumMonstersKilled] = 0;
  int ofNoKnownCause = 0;
  std::vector<HeroStatsMonsterRow> kills = stats.MonsterKills(level);
  for (size_t i = 0; i < kills.size(); i++)
  {
    v[HeroNumMonstersKilled] += kills[i].count;
    if (kills[i].by == HeroBlowByOther) ofNoKnownCause += kills[i].count;
    if (kills[i].by == HeroBlowByDoor) v[HeroNumDoorKills] += kills[i].count;
    else if (kills[i].by == HeroBlowByPit) v[HeroNumPitKills] += kills[i].count;
    else if (kills[i].by == HeroBlowByTelefrag) v[HeroNumTelefragged] += kills[i].count;
    else if (kills[i].by == HeroBlowByLanding) v[HeroNumLandedOn] += kills[i].count;
    else if (kills[i].by != HeroBlowByChampion) v[HeroNumOtherKills] += kills[i].count;
  }
  // A game saved before layout 29 counted a telefrag's kills beside its kills of no known cause:
  // they are telefrags, as many of them as there are such kills (a cloud's or a trap's never were)
  int before = std::min(stats.Movement(level).telefragKills, ofNoKnownCause);
  if (before > 0)
  {
    v[HeroNumTelefragged] += before;
    v[HeroNumOtherKills] -= before;
  }
}

static void FillExperience(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  const HeroStatsExperience &xp = stats.Experience();
  v[HeroNumExperience] = xp.Gained(who, level, -1);
  for (int cls = 0; cls < HERO_XP_CLASS_COUNT; cls++) v[HeroNumXpFighter + cls] = xp.Gained(who, level, cls);
  v[HeroNumRanksGained] = xp.RanksGained(who, level, -1);
}

static void FillEvents(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumPoisonings] = stats.Events(who, level, HeroEventPoisoned);
  v[HeroNumWounds] = stats.Events(who, level, HeroEventWounded);
  v[HeroNumPotionsBrewed] = stats.Events(who, level, HeroEventBrewed);
}

// A champion's from the events; the party's from the movement, which also holds the changes of a
// game saved with layout 37, before they were kept per champion
static void FillFormation(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  if (who == HERO_STATS_PARTY)
  {
    HeroStatsMovement move = stats.Movement(level);
    v[HeroNumFormationSwaps] = move.formationSwaps;
    v[HeroNumFormationMoves] = move.formationMoves;
    v[HeroNumFormationPutBacks] = move.formationPutBacks;
  }
  else
  {
    v[HeroNumFormationSwaps] = stats.Events(who, level, HeroEventFormationSwap);
    v[HeroNumFormationMoves] = stats.Events(who, level, HeroEventFormationMove);
    v[HeroNumFormationPutBacks] = stats.Events(who, level, HeroEventFormationPutBack);
  }
  v[HeroNumFormationChanges] = v[HeroNumFormationSwaps] + v[HeroNumFormationMoves];
  v[HeroNumFormationSwappedAside] = stats.Events(who, level, HeroEventSwappedAside);
}

static void FillMonsterAttacks(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  v[HeroNumAttacksReceived] = stats.MonsterAttacks(who, level);
  v[HeroNumHitsTaken] = stats.MonsterAttacks(who, level, HeroAttackHit);
  v[HeroNumDodged] = stats.MonsterAttacks(who, level, HeroAttackDodged);
  v[HeroNumLucky] = stats.MonsterAttacks(who, level, HeroAttackLucky);
  v[HeroNumTooWeak] = stats.MonsterAttacks(who, level, HeroAttackTooWeak);
  v[HeroNumBlocked] = stats.MonsterAttacks(who, level, HeroAttackBlocked);
}

// 64ths of a hit point, rounded to the nearest whole one (halves away from 0)
static int WholePoints(long long sixtyFourths)
{
  long long whole = (sixtyFourths < 0 ? -sixtyFourths + 32 : sixtyFourths + 32) / 64;
  return (int)(sixtyFourths < 0 ? -whole : whole);
}

static void FillDefences(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  static const HeroStatsNumber PROTECTION[HERO_DEFENCE_PROTECTION_PARTS] = {
      HeroNumReducedArmour, HeroNumReducedHandShields, HeroNumReducedVitality, HeroNumReducedChampionShield,
      HeroNumReducedPartyShield, HeroNumReducedStance, HeroNumReducedWounds, HeroNumReducedAsleep};
  long long parts[HERO_DEFENCE_PART_COUNT], all = 0, protection = 0;
  for (int k = 0; k < HERO_DEFENCE_PART_COUNT; k++)
  {
    parts[k] = stats.Defence(who, level, k);
    all += parts[k];
    if (k >= HERO_DEFENCE_FIRST_PROTECTION) protection += parts[k];
  }
  v[HeroNumReduced] = WholePoints(all);
  v[HeroNumReducedResistance] = WholePoints(parts[HeroDefenceResistance]);
  v[HeroNumReducedShieldSpells] = WholePoints(parts[HeroDefenceShieldSpell]);
  v[HeroNumReducedWisdom] = WholePoints(parts[HeroDefenceWisdom]);
  v[HeroNumReducedProtection] = WholePoints(protection);
  for (int k = 0; k < HERO_DEFENCE_PROTECTION_PARTS; k++)
    v[PROTECTION[k]] = WholePoints(parts[HERO_DEFENCE_FIRST_PROTECTION + k]);
}

static void FillRecords(const HeroStats &stats, HeroStatsChampion who, int level, int *v)
{
  for (int k = 0; k < HERO_RECORD_COUNT; k++)
  {
    HeroRecordEntry entry;
    v[HeroNumRecFirst + k] = stats.Records().Best((HeroRecordKind)k, who, level, &entry) ? entry.value : -1;
  }
}

static const FillGroup CHAMPION_GROUPS[] = {FillDamage, FillKills, FillAttacks, FillFriendlyFire, FillMana, FillCasts,
                                            FillTaken, FillHealing, FillDeaths, FillFood, FillDoors, FillExperience,
                                            FillEvents, FillFormation, FillMonsterAttacks, FillDefences, FillRecords};
static const FillGroup PARTY_GROUPS[] = {FillHardship, FillItems, FillMovement, FillDungeon, FillKillCauses};

static FillGroup GroupOf(HeroStatsNumber number)
{
  switch (number)
  {
  case HeroNumDamage: case HeroNumMelee: case HeroNumRanged: case HeroNumMagic: return FillDamage;
  case HeroNumKills: return FillKills;
  case HeroNumAttacks: case HeroNumHits: case HeroNumHitRate: case HeroNumDamagePerHit: case HeroNumOtherActions: return FillAttacks;
  case HeroNumFriendlyFire: return FillFriendlyFire;
  case HeroNumManaGenerated: case HeroNumManaRegeneration: case HeroNumManaPotions: case HeroNumManaSpent:
  case HeroNumManaFailed: case HeroNumRunesCleared: case HeroNumManaSuccessful: case HeroNumRunesNotCast:
  case HeroNumManaSuccessRate: case HeroNumManaRegenLost: case HeroNumManaMeaningless: case HeroNumManaWasted:
    return FillMana;
  case HeroNumCasts: case HeroNumCastsSuccessful: case HeroNumCastsSuccessRate: case HeroNumCastsWasted: case HeroNumCastsFailed: case HeroNumCastsMeaningless: return FillCasts;
  case HeroNumTaken: return FillTaken;
  case HeroNumHealed: case HeroNumHealRegeneration: case HeroNumHealPotions: case HeroNumHealAction:
  case HeroNumHealPotionsOwn: case HeroNumHealPotionsOthers: case HeroNumHealPotionsFound:
  case HeroNumHealingGiven: return FillHealing;
  case HeroNumDeaths: case HeroNumResurrected: return FillDeaths;
  case HeroNumPoisonings: case HeroNumWounds: case HeroNumPotionsBrewed: return FillEvents;
  case HeroNumAttacksReceived: case HeroNumHitsTaken: case HeroNumDodged: case HeroNumLucky: case HeroNumTooWeak:
  case HeroNumBlocked: return FillMonsterAttacks;
  case HeroNumReduced: case HeroNumReducedResistance: case HeroNumReducedShieldSpells: case HeroNumReducedProtection:
  case HeroNumReducedArmour: case HeroNumReducedHandShields: case HeroNumReducedVitality: case HeroNumReducedChampionShield:
  case HeroNumReducedPartyShield: case HeroNumReducedStance: case HeroNumReducedWounds: case HeroNumReducedAsleep:
  case HeroNumReducedWisdom: return FillDefences;
  case HeroNumWipes: case HeroNumReloads: return FillHardship;
  case HeroNumEaten: case HeroNumDrunk: case HeroNumPotionsDrunk: case HeroNumFoodGained: case HeroNumFoodWasted: case HeroNumWaterGained:
  case HeroNumWaterWasted: return FillFood;
  case HeroNumPickedUp: case HeroNumUsedInWalls: return FillItems;
  case HeroNumSteps: case HeroNumTilesWalked: case HeroNumTilesExplored: case HeroNumTurns: case HeroNumBumps:
  case HeroNumMonsterBumps: case HeroNumTeleports: case HeroNumTelefrags: case HeroNumLandings: case HeroNumPitFalls: case HeroNumPitClimbs: case HeroNumClimbLandings: case HeroNumArrivals: case HeroNumStairChanges:
  case HeroNumStairDances: case HeroNumStairDanceMoves: case HeroNumStrictStairDances: case HeroNumStrictStairDanceMoves:
  case HeroNumTicks: case HeroNumSleepTicks:
    return FillMovement;
  case HeroNumFormationChanges: case HeroNumFormationSwaps: case HeroNumFormationMoves: case HeroNumFormationPutBacks:
  case HeroNumFormationSwappedAside: return FillFormation;
  case HeroNumDoorsBashed: case HeroNumDoorsSmashed: case HeroNumDoorsShot: case HeroNumDoorsBySpell:
  case HeroNumDoorsByZo: return FillDoors;
  case HeroNumDoorsButtonOpened: case HeroNumDoorsButtonClosed: case HeroNumChests: case HeroNumScrolls:
  case HeroNumWallTexts: case HeroNumChampionsSeen: case HeroNumChampionsViewed: return FillDungeon;
  case HeroNumExperience: case HeroNumXpFighter: case HeroNumXpNinja: case HeroNumXpPriest: case HeroNumXpWizard:
  case HeroNumRanksGained: return FillExperience;
  case HeroNumDoorKills: case HeroNumPitKills: case HeroNumOtherKills: case HeroNumMonstersKilled: case HeroNumTelefragged:
  case HeroNumLandedOn: return FillKillCauses;
  default: return FillRecords;
  }
}

HeroStatsRecord HeroStatsCatalogue_Record(const HeroStats &stats, HeroStatsChampion who, int level)
{
  HeroStatsRecord r = {};
  for (size_t g = 0; g < sizeof CHAMPION_GROUPS / sizeof CHAMPION_GROUPS[0]; g++) CHAMPION_GROUPS[g](stats, who, level, r.value);
  if (who == HERO_STATS_PARTY)
    for (size_t g = 0; g < sizeof PARTY_GROUPS / sizeof PARTY_GROUPS[0]; g++) PARTY_GROUPS[g](stats, who, level, r.value);
  return r;
}

int HeroStatsCatalogue_Value(const HeroStats &stats, HeroStatsChampion who, int level, HeroStatsNumber number)
{
  if (NUMBERS[number].partyOnly && who != HERO_STATS_PARTY) return NUMBERS[number].isRecord ? -1 : 0;
  HeroStatsRecord r = {};
  GroupOf(number)(stats, who, level, r.value);
  return r.value[number];
}

bool HeroStatsCatalogue_Applies(HeroStatsNumber number, HeroStatsChampion who, int level)
{
  const HeroStatsNumberInfo &info = NUMBERS[number];
  if (info.partyOnly && who != HERO_STATS_PARTY) return false;
  if (info.wholeRunOnly && level != HERO_STATS_WHOLE_RUN) return false;
  return true;
}

bool HeroStatsCatalogue_Summable(HeroStatsNumber number)
{
  return !NUMBERS[number].isRatio && !NUMBERS[number].isRecord;
}

bool HeroStatsCatalogue_IsPercent(HeroStatsNumber number)
{
  return number == HeroNumHitRate || number == HeroNumManaSuccessRate || number == HeroNumCastsSuccessRate;
}

bool HeroStatsCatalogue_IsRecord(HeroStatsNumber number)
{
  return NUMBERS[number].isRecord;
}

HeroRecordKind HeroStatsCatalogue_RecordOf(HeroStatsNumber number)
{
  return (HeroRecordKind)(number - HeroNumRecFirst);
}

const char *HeroStatsCatalogue_Unit(HeroStatsNumber number, bool plural)
{
  // What a record counts, where its number alone does not say
  switch (number)
  {
  case HeroNumRecLongestFall: return plural ? "levels" : "level";
  case HeroNumRecLongestTeleportChain: return plural ? "teleporters" : "teleporter";
  case HeroNumRecLongestSleep: case HeroNumRecLongestPoisoned: return plural ? "ticks" : "tick";
  case HeroNumRecLongestStairDance: case HeroNumRecLongestStrictStairDance: return plural ? "moves" : "move";
  default: return "";
  }
}

std::string HeroStatsCatalogue_Format(HeroStatsNumber number, int value)
{
  if ((NUMBERS[number].isRatio || NUMBERS[number].isRecord) && value < 0) return "-";
  if (number == HeroNumRecHeaviestLoad) return HeroStatsView_Number(value / 10) + "." + std::to_string(value % 10) + " kg";
  if (number == HeroNumRecClosestCall) return HeroStatsView_Number(value) + " HP left";
  if (HeroStatsCatalogue_IsPercent(number)) return std::to_string(value) + "%";
  const char *unit = HeroStatsCatalogue_Unit(number, value != 1);
  if (unit[0] != 0) return HeroStatsView_Number(value) + " " + unit;
  return HeroStatsView_Number(value);
}

std::string HeroStatsCatalogue_RunShare(HeroStatsNumber number, int level, int value, int runValue)
{
  if (number != HeroNumTicks || level == HERO_STATS_WHOLE_RUN || value <= 0 || runValue <= 0) return "";
  if ((long long)value * 200 < runValue) return "<1%";  // what would round to 0%
  return HeroStatsView_Percent(std::min(value, runValue), runValue);
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_GridNumbers(int topic)
{
  std::vector<HeroStatsNumber> numbers;
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  for (size_t i = 0; i < info.rows.size(); i++)
  {
    HeroStatsNumber number = info.rows[i].number;
    // Records: one at a time, a column per champion (their best) and the party's best
    if (NUMBERS[number].isRecord || (!NUMBERS[number].partyOnly && !NUMBERS[number].isRatio)) numbers.push_back(number);
  }
  return numbers;
}

int HeroStatsCatalogue_TopicOfOldPage(int oldPage)
{
  if (oldPage == 3) return HeroTopicMonsters;
  if (oldPage == 4) return HeroTopicSupplies;
  return HeroTopicOverview;
}

std::vector<HeroStatsNumber> HeroStatsCatalogue_TilesFor(const std::vector<HeroStatsNumber> &tiles, int topic, HeroStatsChampion who, int level)
{
  std::vector<HeroStatsNumber> shown;
  for (size_t i = 0; i < tiles.size(); i++)
    if (HeroStatsCatalogue_Applies(tiles[i], who, level)) shown.push_back(tiles[i]);
  // As many tiles as were chosen: the topic's own top rows that apply fill in for the others
  size_t wanted = std::min(tiles.size(), (size_t)4);
  const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
  for (size_t i = 0; i < info.rows.size() && shown.size() < wanted; i++)
  {
    HeroStatsNumber number = info.rows[i].number;
    if (info.rows[i].depth == 0 && HeroStatsCatalogue_Applies(number, who, level) &&
        std::find(shown.begin(), shown.end(), number) == shown.end())
      shown.push_back(number);
  }
  return shown;
}

std::string HeroStatsCatalogue_Singular(const std::string &label)
{
  size_t end = label.find(" (");
  if (end == std::string::npos) end = label.size();
  size_t start = 0;
  while (start < end)
  {
    size_t stop = label.find(' ', start);
    if (stop == std::string::npos || stop > end) stop = end;
    // A plural ends in a single s: not "meaningless", not a word of two letters
    if (stop - start > 2 && label[stop - 1] == 's' && label[stop - 2] != 's') return label.substr(0, stop - 1) + label.substr(stop);
    start = stop + 1;
  }
  return label;
}

// Every tile the page shows, as a line of text: "4,412 damage dealt · 1 kill · 63% hit rate"
std::string HeroStatsCatalogue_Summary(int topic, const HeroStatsRecord &record, HeroStatsChampion who, int level)
{
  std::string summary;
  int shown = 0;
  std::vector<HeroStatsNumber> tiles = HeroStatsCatalogue_TilesFor(HeroStatsCatalogue_Topic(topic).tiles, topic, who, level);
  for (size_t i = 0; i < tiles.size() && shown < MOST_TILES; i++)
  {
    HeroStatsNumber number = tiles[i];
    const HeroStatsNumberInfo &info = NUMBERS[number];
    if ((info.isRatio || info.isRecord) && record.value[number] < 0) continue;  // "- hit rate" says nothing
    std::string label = info.label;
    label[0] = (char)tolower((unsigned char)label[0]);
    if (record.value[number] == 1 && !info.isRatio && !info.isRecord && !info.isTicks) label = HeroStatsCatalogue_Singular(label);
    if (shown > 0) summary += " \xc2\xb7 ";
    summary += HeroStatsCatalogue_Format(number, record.value[number]) + " " + label;
    shown++;
  }
  return summary;
}
