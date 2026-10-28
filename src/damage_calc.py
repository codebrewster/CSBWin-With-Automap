#!/usr/bin/env python3
"""
Average physical-attack damage, player -> monster.

Faithful Monte-Carlo port of the damage pipeline in this repo:
  Quickness()                 Character.cpp:2119
  IsCharacterLucky()          Character.cpp:2075  (luck kept fixed; the
                              game itself drifts it +/-2 per call)
  DetermineThrowingDistance() Character.cpp:2177
  DeterminePhysicalAttackDamage() Attack.cpp:532  (incl. the D6<=1 graze
                              chain Attack.cpp:711 and the final
                              randomize/shrink chain Attack.cpp:799)
  DamageMonster()             Code11f52.cpp:1052  (plain HP subtraction;
                              dragon HP is huge so death is ignored)

All game data below lives in the CONFIG section - change it for another
character, weapon or monster. Defaults: strong fighter vs Red Dragon.

Attack numbers (P7 = "strength required", P8 = damage factor) are the
decoded 7-column attack table values; monster armor/dex are the
"Defense"/"Dexterity" descriptor values (Red Dragon: 110 / 70).
"""

import random

TRIALS = 200_000
SEED = 12345

# When True, stamina is deducted for every swing (starting from
# the configured stamina, no regen modeled) and low stamina reduces
# power via StaminaAdjustedAttribute. When False, stamina stays as
# configured (no drain, no regen); entering stamina below half of max
# still models a tired fighter.
# Tip: with drain on, set TRIALS to a fight-sized number (e.g. 60) to
# model a single fight instead of an exhausted steady state.
STAMINA_DRAIN = False

# ---------------------------------------------------------------- CONFIG

CHARACTER = {
    "strength": 55,     # current Strength (weapon bonus NOT included;
                        # the weapon's own Strength value is added by
                        # DetermineThrowingDistance, see weapon ubyte2)
    "dexterity": 43,    # current Dexterity
    "luck": 50,         # current Luck (used by IsCharacterLucky)
    "load": 200,        # carried weight, same units as weapon weight
    "elven_boots": False,  # feet slot holds Elven Boots (+1/16 MaxLoad)
    "stamina": 750,    # current stamina
    "max_stamina": 750,  # full stamina -> no stamina penalty
    "ouches": 0,        # wounds; 0 = healthy
    # Effective skill masteries (DetermineMastery result, ~1..15).
    # 4,5,6,7 = Fighter family; 8..11 = Ninja family.
    "mastery": {4: 12, 5:10, 6: 10, 7:10, 8:10, 9: 10, 10: 12, 11: 0},
}

# ubyte2 = weapon Strength bonus added to the throw base (DM items data:
# Diamond Edge 62, Hardcleave 70). ubyte1 = weapon class: 0/2 adds
# mastery(4); 1..15 adds mastery(10); 16..111 adds mastery(11).
# weight in 0.1 kg units (3.7 kg -> 37). armor_div: Diamond Edge cuts
# monster armor by 1/4 (Attack.cpp:676), Hardcleave/Executioner by 1/8
# (Attack.cpp:686), else None. vorpal: True only for the Vorpal Blade --
# the only weapon that harms non-material beings (Attack.cpp:595); its
# damage is halved vs material ones (Attack.cpp:845).
WEAPONS = {
    "falchion":   {"weight": 33, "ubyte1": 0, "ubyte2": 30,
                     "armor_div": None},
    "sword":   {"weight": 32, "ubyte1": 0, "ubyte2": 34,
                     "armor_div": None},
    "rapier":   {"weight": 26, "ubyte1": 0, "ubyte2": 38,
                     "armor_div": None},
    "sabre":   {"weight": 35, "ubyte1": 0, "ubyte2": 42,
                     "armor_div": None},
    "samurai_sword":   {"weight": 36, "ubyte1": 0, "ubyte2": 46,
                     "armor_div": None},
    "vorpal_blade":   {"weight": 30, "ubyte1": 0, "ubyte2": 48,
                     "armor_div": None, "vorpal": True},
    "axe":   {"weight": 43, "ubyte1": 2, "ubyte2": 49,
                     "armor_div": None},
    "bolt_blade":   {"weight": 30, "ubyte1": 0, "ubyte2": 49,
                     "armor_div": None},
    "delta":   {"weight": 33, "ubyte1": 0, "ubyte2": 50,
                     "armor_div": None},
    "fury":   {"weight": 47, "ubyte1": 0, "ubyte2": 55,
                     "armor_div": None},
    "the_inquisitor":   {"weight": 39, "ubyte1": 0, "ubyte2": 58,
                     "armor_div": None},
    # Amiga 3.6 WEAPONDESC (Reference/Original/A36M): diamond_edge
    # ubyte1 is 0, not 1 (no +mastery(10); ubyte1 0/2 adds mastery(4));
    # axe ubyte1 is 2, functionally identical to 0.
    "diamond_edge": {"weight": 37, "ubyte1": 0, "ubyte2": 62,
                     "armor_div": 4},
    "hardcleave":   {"weight": 65, "ubyte1": 2, "ubyte2": 70,
                     "armor_div": 8},
    # WEAPONDESC from the Amiga 3.6 binary (Reference/Original/A36M):
    # mace/mace_of_order/morningstar are consecutive 6-byte entries
    # right after hardcleave (weight, uByte1, uByte2, ...). No armor_div
    # or vorpal special-casing in Attack.cpp for these.
    "mace":   {"weight": 31, "ubyte1": 0, "ubyte2": 32,
                     "armor_div": None},
    "mace_of_order":   {"weight": 41, "ubyte1": 0, "ubyte2": 42,
                     "armor_div": None},
    "morningstar":   {"weight": 50, "ubyte1": 0, "ubyte2": 60,
                     "armor_div": None},
}

# P7 = strength-required (luck fallback threshold 75-P7),
# P8 = damage factor (D7 = D7*P8/32). skill = skill improved / +10 bonus.
# time = recovery ticks before the next attack (6 ticks = 1 sec).
# A missed attack halves the recovery (Attack.cpp:1832).
# energy = base stamina cost of the attempt (Byte20046).
# exp = base experience for the improved skill (Byte20178, actions doc);
# an unsuccessful attack earns half (Attack.cpp:1829-1832, C++ truncating
# division). The xp columns assume a real fight (AdjustSkills,
# Magic.cpp:211-240): no peaceful halving, times the level multiplier
# (0 = no-op), times 2 for danger.
ATTACKS = {
    "stab":    {"P7": 30, "P8": 48, "skill": 9, "time": 5, "energy": 3,
                "exp": 15},
    "chop":    {"P7": 48, "P8": 48, "skill": 6, "time": 8, "energy": 10,
                "exp": 10},
    "berzerk": {"P7": 46, "P8": 96, "skill": 4, "time": 30, "energy": 40,
                "exp": 40},
    "jab": {"P7": 70, "P8": 8, "skill": 5, "time": 2, "energy": 3,
            "exp": 11},
    "thrust": {"P7": 57, "P8": 66, "skill": 5, "time": 16, "energy": 17,
                "exp": 19},
    "swing": {"P7": 32, "P8": 16, "skill": 4, "time": 6, "energy": 2,
              "exp": 6},
    "slash": {"P7": 26, "P8": 16, "skill": 4, "time": 4, "energy": 2,
              "exp": 9},
    "melee": {"P7": 64, "P8": 60, "skill": 6, "time": 20, "energy": 25,
              "exp": 24},
    "cleave": {"P7": 40, "P8": 48, "skill": 4, "time": 12, "energy": 10,
               "exp": 12},
    "bash":   {"P7": 32, "P8": 50, "skill": 6, "time": 20, "energy": 9,
               "exp": 11},
    "stun":   {"P7": 50, "P8": 16, "skill": 6, "time": 7, "energy": 2,
               "exp": 10},
    "disrupt": {"P7": 46, "P8": 55, "skill": 17, "time": 9, "energy": 5,
                "exp": 10, "disrupt": True},
}


MONSTER1 = {
    "name": "Mummy",
    "armor": 25,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 40,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 1,  # dungeon level experience multiplier
    "base_health": 33,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 4,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER2 = {
    "name": "Screamer",
    "armor": 5,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 5,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 1,  # dungeon level experience multiplier
    "base_health": 165,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 0,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER3 = {
    "name": "Rockpile",
    "armor": 170,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 10,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 1,  # dungeon level experience multiplier
    "base_health": 50,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 5,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER4 = {
    "name": "Trolin",
    "armor": 28,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 41,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 1,  # dungeon level experience multiplier
    "base_health": 20,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 1,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER5 = {
    "name": "Magenta Worm",
    "armor": 72,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 35,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 70,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 5,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER6 = {
    "name": "Giant Wasp",
    "armor": 180,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 150,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 8,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 9,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER7 = {
    "name": "Ghost",
    "armor": 15,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 80,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 1,  # dungeon level experience multiplier
    "base_health": 30,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 6,  # word16 bits 8-11 ("Experience")
    "non_material": True,
}

MONSTER8 = {
    "name": "Swamp Slime",
    "armor": 20,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 20,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 110,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 3,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER9 = {
    "name": "Couatl",
    "armor": 42,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 88,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 39,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 7,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER10 = {
    "name": "Wizard Eye",
    "armor": 30,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 80,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 40,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 6,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER11 = {
    "name": "Skeleton",
    "armor": 22,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 80,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 2,  # dungeon level experience multiplier
    "base_health": 20,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 5,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER12 = {
    "name": "Stone Golem",
    "armor": 240,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 35,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 3,  # dungeon level experience multiplier
    "base_health": 120,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 11,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER13 = {
    "name": "Giggler",
    "armor": 50,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 100,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 3,  # dungeon level experience multiplier
    "base_health": 10,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 1,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER14 = {
    "name": "Pain Rat",
    "armor": 45,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 65,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 3,  # dungeon level experience multiplier
    "base_health": 101,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 8,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER15 = {
    "name": "Vexirk",
    "armor": 47,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 90,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 3,  # dungeon level experience multiplier
    "base_health": 44,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 9,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER16 = {
    "name": "Ruster",
    "armor": 100,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 30,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 3,  # dungeon level experience multiplier
    "base_health": 60,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 3,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER17 = {
    "name": "Giant Scorpion",
    "armor": 55,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 55,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 4,  # dungeon level experience multiplier
    "base_health": 150,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 9,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER18 = {
    "name": "Water Elemental",
    "armor": 75,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 50,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 5,  # dungeon level experience multiplier
    "base_health": 144,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 6,  # word16 bits 8-11 ("Experience")
    "non_material": True,
}

MONSTER19 = {
    "name": "Animated Armor",
    "armor": 140,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 70,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 5,  # dungeon level experience multiplier
    "base_health": 60,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 10,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER20 = {
    "name": "Oitu",
    "armor": 33,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 60,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 5,  # dungeon level experience multiplier
    "base_health": 77,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 9,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER21 = {
    "name": "Materializer",
    "armor": 15,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 65,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 5,  # dungeon level experience multiplier
    "base_health": 33,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 12,  # word16 bits 8-11 ("Experience")
    "non_material": True,
}

MONSTER22 = {
    "name": "Black Flame",
    "armor": 45,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 60,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 6,  # dungeon level experience multiplier
    "base_health": 80,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 5,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER23 = {
    "name": "Demon",
    "armor": 68,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 75,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 6,  # dungeon level experience multiplier
    "base_health": 100,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 13,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER24 = {
    "name": "Red Dragon",
    "armor": 110,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 70,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 6,  # dungeon level experience multiplier
    "base_health": 255,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 15,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}

MONSTER25 = {
    "name": "Lord Chaos",
    "armor": 255,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 130,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 6,  # dungeon level experience multiplier
    "base_health": 180,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 15,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}


MONSTER = {
    "name": "Oitu",
    "armor": 33,        # uByte8[0] ("Defense"): subtracted from damage
    "dex": 60,           # uByte8[4] ("Dexterity"): hit difficulty
    "level_difficulty": 5,  # dungeon level experience multiplier
    "base_health": 77,  # uByte8[1] ("Base Health"), creature-details doc
    "exp": 9,  # word16 bits 8-11 ("Experience")
    "non_material": False,
}


# (weapon, attack) pairs to evaluate
COMBOS = [
#    ("falchion", "swing"),
#    ("falchion", "chop"),
#    ("sword", "swing"),
#    ("sword", "chop"),
#    ("rapier", "jab"),
#    ("rapier", "thrust"),
#    ("sabre", "slash"),
#    ("sabre", "melee"),
#    ("samurai_sword", "slash"),
#    ("samurai_sword", "melee"),
#    ("vorpal_blade", "jab"),
#    ("vorpal_blade", "cleave"),
#    ("vorpal_blade", "disrupt"),
#    ("axe", "swing"),
#    ("axe", "chop"),
#    ("axe", "melee"),
#    ("bolt_blade", "jab"),
#    ("bolt_blade", "chop"),
#    ("delta", "chop"),
#    ("delta", "melee"),
#    ("delta", "thrust"),
#    ("fury", "chop"),
#    ("fury", "melee"),
#    ("the_inquisitor", "swing"),
#    ("the_inquisitor", "thrust"),
#    ("the_inquisitor", "berzerk"),
    ("diamond_edge", "stab"),
    ("diamond_edge", "chop"),
    ("diamond_edge", "cleave"),
    ("mace", "swing"),
    ("mace", "bash"),
    ("mace", "stun"),
    ("mace_of_order", "swing"),
    ("mace_of_order", "bash"),
    ("mace_of_order", "stun"),
    ("morningstar", "swing"),
    ("morningstar", "stun"),
    ("morningstar", "melee"),
#    ("hardcleave", "chop"),
#    ("hardcleave", "berzerk"),
]

# ---------------------------------------------------------------- engine


def monster_hp_range(mon, mult=None):
    """Return (min, max) hit points for a monster of this type.

    Port of CreateMonster (Monster.cpp:201-206):
        HP = BaseHealth * hitPointMultiplier + STRandom() % (BaseHealth/4+1)
    i.e. BaseHealth*mult .. BaseHealth*mult + BaseHealth//4 (C++ truncating
    division; each group member rolls separately).  hitPointMultiplier is
    the generator's (DB3 word6 bits 4-7, CSB.h:1041), defaulting to the
    level's experienceMultiplier when the generator's is 0
    (Timer.cpp:1759-1763).  With mult=None the monster's level_difficulty
    is used, matching how one_attack() treats it (Attack.cpp:584).
    Base Health values: creature-details doc (graphics.dat item 559).
    """
    base = mon["base_health"]
    m = mon["level_difficulty"] if mult is None else mult
    lo = base * m
    return lo, lo + base // 4


def max_load(ch):
    ml = 8 * ch["strength"] + 100
    ml = stamina_adjust(ch, ml)
    if ch.get("elven_boots"):
        # Elven Boots in the feet slot (Code11f52.cpp:4289): +1/16,
        # applied before the +9 and rounding like everything downstream
        # (throw thresholds, quickness) which all call MaxLoad().
        ml = ml + (ml >> 4)
    ml += 9
    return ml - (ml % 10)  # rounded down to multiple of 10


def stamina_adjust(ch, base):
    if ch["stamina"] < ch["max_stamina"] // 2:
        half = base // 2
        return half + (half * ch["stamina"]) // (ch["max_stamina"] // 2)
    return base


def quickness(ch, ml):
    q = ch["dexterity"] + random.randrange(8)
    q = q - ((q // 2) * ch["load"]) // ml
    q = q // 2
    return min(max(q, random.randrange(8) + 1), 100 - random.randrange(8))


def is_lucky(ch, need):
    if random.randrange(2) and random.randrange(100) > need:
        return True
    cur = ch["luck"]
    roll = random.randrange(cur) if cur else 0
    return roll > need


def throwing_distance(ch, wpn, ml):
    d7 = ch["strength"] + random.randrange(16)
    w = wpn["weight"]
    d5 = ml // 16
    if w <= d5:
        d7 = d7 + w - 12
    else:
        w2 = (d5 - 12) // 2 + d5
        if w <= w2:
            d7 = d7 + (w - d5) // 2
        else:
            d7 = d7 - 2 * (w - w2)
    d7 = d7 + wpn["ubyte2"]          # weapon Strength bonus
    d6 = 0
    u1 = wpn["ubyte1"]
    m = ch["mastery"]
    if u1 == 0 or u1 == 2:
        d6 += m.get(4, 0)
    if 0 < u1 < 16:
        d6 += m.get(10, 0)
    if 16 <= u1 < 112:
        d6 += m.get(11, 0)
    d7 = d7 + 2 * d6
    d7 = stamina_adjust(ch, d7)
    if ch["ouches"]:
        d7 = d7 // 2                 # weapon hand wounded (hand==1&D1==2)
    return min(max(d7 // 2, 0), 100)


def deduct(ch, amount):
    # AdjustStamina (Code11f52.cpp:4399), clamped at 0. The game would
    # spill overflow into HP damage; that is not modeled here.
    ch["stamina"] = max(ch["stamina"] - amount, 0)


def one_attack(ch, wpn, atk, mon):
    """One attack attempt. Returns (damage, hit_flag, stamina_cost).

    Stamina cost mirrors the engine: energy + coin flip per attempt
    (Attack.cpp:1716,2148) plus exertion via PhysicalAttackFilter --
    4..7 on a clean hit, 2..3 on a miss, 0 on a graze-hit."""
    ml = max_load(ch)  # MaxLoad is stamina-adjusted, so it drops as
                       # the character tires (Code11f52.cpp:4266)
    # Non-material beings ignore everything except the Vorpal Blade
    # (any attack) and the Disrupt action (Attack.cpp:595,1150).
    vorpal_hit = wpn.get("vorpal", False) or atk.get("disrupt", False)
    if mon["non_material"] and not vorpal_hit:
        return 0, False, 0
    if atk.get("disrupt", False) and not mon["non_material"]:
        # Disrupt fizzles vs material beings (Attack.cpp:1117);
        # only the base stamina cost is paid.
        cost = atk["energy"] + random.randrange(2)
        if STAMINA_DRAIN:
            deduct(ch, cost)
        return 0, False, cost
    # --- to-hit gate (Attack.cpp:601-634)
    q = quickness(ch, ml)
    need = mon["dex"] + random.randrange(32) + mon["level_difficulty"] - 16
    sadj = 0  # exertion cost applied via PhysicalAttackFilter
    if q <= need:
        if random.randrange(4) != 0 and not is_lucky(ch, 75 - atk["P7"]):
            sadj = 2 + random.randrange(2)   # clean miss (Attack.cpp:621)
            cost = atk["energy"] + random.randrange(2) + sadj
            if STAMINA_DRAIN:
                deduct(ch, cost)
            return 0, False, cost
    # --- base power (Attack.cpp:652-703)
    t = throwing_distance(ch, wpn, ml)
    if t != 0:
        d7 = t + random.randrange(t // 2 + 1)
        d7 = (d7 * atk["P8"]) // 32
        d4 = random.randrange(32) + mon["armor"] + mon["level_difficulty"]
        if wpn["armor_div"]:
            d4 = d4 - d4 // wpn["armor_div"]
        d7 = d7 + random.randrange(32) - d4
        d6 = d7
    else:
        d6 = 0
    # --- graze chain (Attack.cpp:711-798)
    if t == 0 or d6 <= 1:
        d7 = random.randrange(4)
        if d7 == 0:
            sadj = 2 + random.randrange(2)
            cost = atk["energy"] + random.randrange(2) + sadj
            if STAMINA_DRAIN:
                deduct(ch, cost)
            return 0, False, cost
        d6 = d6 + random.randrange(16)
        if d6 > 0 or random.randrange(2):
            d7 = d7 + random.randrange(4)
            if random.randrange(4) == 0:
                d7 = d7 + max(0, random.randrange(16) + d6)
    else:
        sadj = 4 + random.randrange(4)       # clean hit (Attack.cpp:901)
    # --- randomize / shrink (Attack.cpp:799-878)
    d7 = d7 // 2
    d0 = random.randrange(d7) if d7 else 0
    d7 = d7 + random.randrange(4) + d0
    if d7:
        d7 = d7 + random.randrange(d7)
    d7 = d7 // 4
    d7 = d7 + random.randrange(4) + 1
    if wpn.get("vorpal", False) and not mon["non_material"]:
        # Vorpal Blade damage is halved vs material beings; if nothing
        # is left the blow fails (Attack.cpp:845). Vs non-material
        # beings (ghosts etc.) it deals full damage.
        d7 = d7 // 2
        if d7 == 0:
            sadj = 2 + random.randrange(2)
            cost = atk["energy"] + random.randrange(2) + sadj
            if STAMINA_DRAIN:
                deduct(ch, cost)
            return 0, False, cost
    if random.randrange(64) < ch["mastery"].get(atk["skill"], 0):
        d7 += 10                     # mastery bonus
    cost = atk["energy"] + random.randrange(2) + sadj
    if STAMINA_DRAIN:
        deduct(ch, cost)
    return max(d7, 0), True, cost


def main():
    random.seed(SEED)
    print(f"STAMINA_DRAIN={STAMINA_DRAIN}  TRIALS={TRIALS}")
    print(f"MaxLoad={max_load(CHARACTER)}  (str={CHARACTER['strength']}, "
          f"dex={CHARACTER['dexterity']}, luck={CHARACTER['luck']}, "
          f"load={CHARACTER['load']})")
    print(f"Monster: {MONSTER['name']} "
          f"(armor={MONSTER['armor']}, dex={MONSTER['dex']}, "
          f"level_diff={MONSTER['level_difficulty']})")
    print(f"{'combo':24s} {'hit%':>6s} {'dmg/try':>8s} {'dmg/hit':>8s} "
          f"{'min_hit':>8s} {'max_hit':>8s} "
          f"{'dps':>8s} {'dmg/sta':>8s} {'sta/try':>8s} "
          f"{'xp/try':>8s} {'xps':>8s} {'end_stam':>8s}")
    for wn, an in COMBOS:
        ch = {k: (dict(v) if k == "mastery" else v)
              for k, v in CHARACTER.items()}
        ch["stamina"] = ch["max_stamina"]  # fresh fighter per combo
        hits = tot = 0
        ticks = spent = xp = 0
        min_hit = None
        max_hit = 0
        atk = ATTACKS[an]
        for _ in range(TRIALS):
            dmg, hit, cost = one_attack(ch, WEAPONS[wn], atk, MONSTER)
            tot += dmg
            hits += hit
            spent += cost
            ticks += atk["time"] if hit else atk["time"] / 2
            gained = atk["exp"] if hit else atk["exp"] // 2
            if hit:
                gained += (dmg * MONSTER["exp"]) // 16 + 3
            mult = MONSTER["level_difficulty"]
            if mult != 0:
                gained *= mult
            xp += gained * 2
            if hit:
                if min_hit is None or dmg < min_hit:
                    min_hit = dmg
                if dmg > max_hit:
                    max_hit = dmg
        print(f"{wn+'+'+an:24s} {100*hits/TRIALS:6.1f} "
              f"{tot/TRIALS:8.2f} {tot/max(hits, 1):8.2f} "
              f"{(min_hit if min_hit is not None else 0):8d} {max_hit:8d} "
              f"{6*tot/max(ticks, 1):8.2f} {tot/max(spent, 1):8.2f} "
              f"{spent/TRIALS:8.2f} {xp/TRIALS:8.2f} "
              f"{6*xp/max(ticks, 1):8.2f} "
              f"{ch['stamina']:8d}")


if __name__ == "__main__":
    main()
