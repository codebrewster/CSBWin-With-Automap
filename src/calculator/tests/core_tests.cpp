// Checks every exact table against the same formula played with dice, and
// the recovery simulation against hand-worked cases.  No Qt, no game files.
//   calc_tests [trials]
//   calc_tests --files GRAPHICS.DAT SAVEGAME...   the readers, on files of your own
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#include "../core/HeroAttack.h"
#include "../core/MonsterAttack.h"
#include "../core/Recovery.h"
#include "../core/Spells.h"
#include "../io/GraphicsDat.h"
#include "../io/SaveGame.h"

using namespace calc;

static int failures = 0, checks = 0;
static long trials = 400000;

static void expect(bool ok, const std::string &what)
{
  checks++;
  if (!ok) {
    failures++;
    std::printf("FAIL %s\n", what.c_str());
  }
}

// The exact table `d` (missing mass = 0) against `sample` drawn `trials` times
static void compare(const std::string &name, const Dist &d, const std::function<int()> &sample)
{
  std::vector<double> count;
  double sum = 0, sum2 = 0, hits = 0;
  for (long i = 0; i < trials; i++) {
    int v = sample();
    if (v < 0) v = 0;
    if (size_t(v) >= count.size()) count.resize(size_t(v) + 1, 0.0);
    count[size_t(v)] += 1;
    sum += v;
    sum2 += double(v) * v;
    if (v > 0) hits++;
  }
  const double n = double(trials);
  double eMean = 0, eHit = 0, eVar = 0;
  for (size_t i = 0; i < d.p.size(); i++) {
    int v = d.lo + int(i);
    if (v <= 0) continue;
    eMean += v * d.p[i];
    eVar += double(v) * v * d.p[i];
    eHit += d.p[i];
  }
  eVar -= eMean * eMean;
  double seMean = std::sqrt(std::max(eVar, 1e-12) / n);
  double zMean = (sum / n - eMean) / seMean;
  double seHit = std::sqrt(std::max(eHit * (1 - eHit), 1e-12) / n);
  double zHit = (hits / n - eHit) / seHit;
  // every value: observed share against the exact one
  double worst = 0;
  int worstAt = 0;
  size_t top = std::max(count.size(), size_t(std::max(0, d.hi()) + 1));
  for (size_t v = 1; v < top; v++) {
    double e = d.at(int(v)), o = v < count.size() ? count[v] / n : 0;
    double se = std::sqrt(std::max(e * (1 - e), 1e-9) / n);
    double z = std::fabs(o - e) / se;
    if (e * n < 30) z = 0;         // too rare for the normal approximation
    if (e == 0 && o > 0) z = 99;   // a value the exact table says cannot happen
    if (z > worst) {
      worst = z;
      worstAt = int(v);
    }
  }
  bool ok = std::fabs(zMean) < 4.5 && std::fabs(zHit) < 4.5 && worst < 5.5;
  checks++;
  if (!ok) failures++;
  std::printf("%s %-46s mean %8.3f (dice %8.3f, z %+5.1f)  hit %6.2f%% (z %+5.1f)  worst z %4.1f at %d\n",
              ok ? "ok  " : "FAIL", name.c_str(), eMean, sum / n, zMean, 100 * eHit, zHit, worst,
              worstAt);
}

static Hero fighter(const GameData &g)
{
  Hero h;
  h.name = "Fighter";
  int attr[7] = {49, 70, 74, 45, 60, 40, 35};
  for (int i = 0; i < 7; i++) {
    h.attr[i] = attr[i];
    h.attrMax[i] = attr[i];
  }
  h.attrMax[Luck] = 100;
  h.hp = h.maxHp = 420;
  h.stamina = h.maxStamina = 900;
  h.mana = h.maxMana = 60;
  h.load = 291;
  for (int &m : h.mastery) m = 9;
  h.mastery[SkillParry] = 7;
  h.weapon = 18;
  h.armour[SlotReadyHand] = 31;   // Small Shield
  h.handWeight[0] = g.armours[31].weight;
  h.handWeight[1] = g.weapons[18].weight;
  h.armour[SlotHead] = 27;
  h.armour[SlotTorso] = 32;
  h.armour[SlotLegs] = 33;
  h.armour[SlotFeet] = 37;
  return h;
}

static Target monster(const GameData &g, int id, int level)
{
  Target t;
  t.type = g.monsters[size_t(id)];
  t.level = level;
  t.hp = monsterHpMin(t.type, level);
  return t;
}

static void heroAttacks(const GameData &g)
{
  Rng rng(1);
  Hero h = fighter(g);
  struct Case { int weapon, action, monster, level; bool attacking, weak, bias; };
  const Case cases[] = {
    {18, 2, 21, 5, false, false, true},    // Axe chop on an Oitu
    {18, 25, 10, 1, false, false, true},   // Axe melee on a Mummy
    {15, 29, 9, 3, false, false, true},    // Diamond Edge cleave on a Stone Golem
    {19, 19, 24, 6, true, false, true},    // Executioner berzerk on a Dragon that attacks
    {16, 24, 8, 2, false, false, true},    // Vorpal Blade disrupt on a Ghost
    {16, 16, 21, 5, false, false, true},   // Vorpal Blade jab on an Oitu (halved)
    {10, 13, 8, 2, false, false, true},    // Sword on a Ghost: nothing
    {-1, 6, 16, 1, false, false, true},    // Punch on a Trolin
    {20, 30, 7, 2, false, true, true},     // Mace bash on a Rockpile, tired and wounded
    {11, 15, 17, 2, false, false, false},  // Rapier thrust on a Wasp, even dice
  };
  for (const Case &c : cases) {
    Hero hero = h;
    if (c.weak) {
      hero.stamina = 300;
      hero.wounds = WoundActionHand | WoundLegs;
      hero.attr[Luck] = 30;
    }
    Rules rules;
    rules.moduloBias = c.bias;
    rules.luckDrift = false;
    rules.vulnerableOnAttack = true;
    WeaponType w = c.weapon < 0 ? g.bareHands() : g.weapons[size_t(c.weapon)];
    const ActionType &a = g.actions[size_t(c.action)];
    Target t = monster(g, c.monster, c.level);
    t.attacking = c.attacking;
    SwingResult r = heroAttack(hero, w, a, t, rules, SwingOptions());
    // a swing at a monster lasts the full delay plus 2 clicking ticks, hit or miss
    expect(std::fabs(r.ticks - swingPeriod(a.ticks)) < 1e-9, "hero: swing period " + a.name);
    Draw d{rng, c.bias};
    compare("hero: " + w.name.substr(0, 14) + " " + a.name + " / " + t.type.name.substr(0, 12),
            r.damage, [&] {
              SwingState st{hero.stamina, hero.luck()};
              return sampleSwing(d, hero, w, a, t, rules, false, st).damage;
            });
  }
  // measured in the game with a key held down: Eye of Time 1 + 2, Hardcleave 8 + 2 ticks
  expect(swingPeriod(1) == 3 && swingPeriod(8) == 10, "swing period: delay plus 2");
  expect(swingPeriod(1, true) == 2 && swingPeriod(8, true) == 6, "swing period: halved delay, integer");
  // strength stops counting once the throwing power reaches its cap of 100
  {
    Hero hero = h;
    Rules rules;
    rules.luckDrift = false;
    Target t = monster(g, 21, 5);
    const ActionType &a = g.actions[2];
    hero.attr[Strength] = 150;
    double at150 = heroAttack(hero, g.weapons[18], a, t, rules, SwingOptions()).perSwing;
    hero.attr[Strength] = 255;
    double at255 = heroAttack(hero, g.weapons[18], a, t, rules, SwingOptions()).perSwing;
    expect(at150 == at255, "an axe does no more damage at strength 255 than at 150");
    hero.attr[Strength] = 30;
    expect(heroAttack(hero, g.weapons[18], a, t, rules, SwingOptions()).perSwing < at150,
           "but a weak hero does less");
  }
  // a fight: stamina drains and luck drifts from swing to swing
  {
    Rules rules;
    SwingOptions opt;
    opt.swings = 40;
    opt.staminaDrain = true;
    Hero hero = h;
    hero.stamina = 500;
    WeaponType w = g.weapons[19];
    const ActionType &a = g.actions[19];
    Target t = monster(g, 21, 5);
    SwingResult r = heroAttack(hero, w, a, t, rules, opt);
    Draw d{rng, true};
    double dmg = 0, end = 0, cost = 0;
    long fights = std::max(2000L, trials / 40);
    for (long f = 0; f < fights; f++) {
      SwingState st{hero.stamina, hero.luck()};
      for (int i = 0; i < opt.swings; i++) {
        SwingSample s = sampleSwing(d, hero, w, a, t, rules, true, st);
        dmg += s.damage;
        cost += s.cost;
      }
      end += st.stamina;
    }
    double n = double(fights) * opt.swings;
    std::printf("     fight of 40 berzerks: damage/swing %.3f (dice %.3f)  stamina/swing %.3f (dice %.3f)  end stamina %.2f (dice %.2f)\n",
                r.perSwing, dmg / n, r.staminaPerSwing, cost / n, r.endStamina, end / fights);
    expect(std::fabs(r.perSwing - dmg / n) < 0.03 * r.perSwing + 0.05, "fight: damage per swing");
    expect(std::fabs(r.staminaPerSwing - cost / n) < 0.02 * r.staminaPerSwing, "fight: stamina per swing");
    expect(std::fabs(r.endStamina - end / fights) < 3.0, "fight: end stamina");
  }
  // kill time: the mean of a geometric-like sum
  {
    Dist one;
    one.add(5, 0.5);
    expect(std::fabs(meanTriesToReach(one, 10) - 4.0) < 1e-9, "tries to reach: two hits at 50%");
    std::vector<double> curve = killCurve(one, 10, 3);
    expect(std::fabs(curve[2] - 0.25) < 1e-9 && std::fabs(curve[3] - 0.5) < 1e-9, "kill curve");
  }
}

static void monsterAttacks(const GameData &g)
{
  Rng rng(2);
  Rules rules;
  rules.luckDrift = false;
  Party party;
  party.partyShield = 6;
  const int ids[] = {10, 0, 21, 24, 6, 3, 9, 11, 17, 16};
  int level = 1;
  for (int id : ids) {
    Hero h = fighter(g);
    if (id == 9) h.wounds = WoundTorso | WoundHead;
    if (id == 16) {   // an unarmoured novice
      h = Hero();
      h.hp = h.maxHp = 60;
    }
    Target t = monster(g, id, level);
    level = level % 6 + 1;
    MonsterAttackResult r = monsterAttack(h, party, t, g, rules);
    Draw d{rng, true};
    compare("monster: " + t.type.name.substr(0, 20) + " L" + std::to_string(t.level), r.damage,
            [&] {
              int luck = h.luck();
              return sampleMonsterAttack(d, h, party, t, g, rules, luck);
            });
    // wounds and poison per attack
    long wounded = 0, poisoned = 0, n = trials / 4;
    for (long i = 0; i < n; i++) {
      int luck = h.luck(), wounds = 0;
      bool poison = false;
      sampleMonsterAttack(d, h, party, t, g, rules, luck, &poison, &wounds);
      if (wounds) wounded++;
      if (poison) poisoned++;
    }
    double se = std::sqrt(std::max(r.woundChance * (1 - r.woundChance), 1e-9) / n);
    expect(std::fabs(double(wounded) / n - r.woundChance) < 5 * se + 1e-4,
           "wound chance of " + t.type.name);
    se = std::sqrt(std::max(r.poisonChance * (1 - r.poisonChance), 1e-9) / n);
    expect(std::fabs(double(poisoned) / n - r.poisonChance) < 5 * se + 1e-4,
           "poison chance of " + t.type.name);
  }
  // each way DamageCharacter reduces a blow
  for (int kind = 0; kind <= 7; kind++) {
    Hero h = fighter(g);
    Party p;
    p.spellShield = 3;
    p.fireShield = 4;
    Dice dice(true);
    Dist raw = dice.dist(40, 5);
    int mask = kind % 2 ? 63 : (WoundHead | WoundTorso);
    HarmResult hr = damageHero(raw, mask, kind, h, p, g, dice);
    Draw d{rng, true};
    compare(std::string("harm: ") + damageKindName(kind), hr.damage,
            [&] { return sampleDamageHero(d, 5 + d.rnd(40), mask, kind, h, p, g); });
  }
}

static void spells(const GameData &g)
{
  Rng rng(3);
  Dice dice(true);
  Draw d{rng, true};
  Hero caster = fighter(g);
  for (int missile : damageMissiles()) {
    for (int power : {1, 4, 6}) {
      Cast c = castSpell(g, caster, missile, power);
      expect(c.known, std::string("spell known: ") + missileName(missile));
      for (int id : {10, 21, 8, 9, 24}) {
        Target t = monster(g, id, 3);
        for (int steps : {0, 3}) {
          MissileState at = flyMissile(c.range, c.damage, c.decay, steps);
          MonsterHit hit = missileOnMonster(missile, at, t, dice);
          compare(std::string("spell: ") + missileName(missile) + " " + powerName(power) + " / " +
                    t.type.name.substr(0, 10) + (steps ? " far" : ""),
                  hit.struck, [&] { return sampleMissileOnMonster(d, missile, at, t, true); });
          if (!hit.others.empty() && missile != MissilePoisonCloud && steps == 0 && power == 4)
            compare(std::string("  ... the others of the group"), hit.others,
                    [&] { return sampleMissileOnMonster(d, missile, at, t, false); });
        }
      }
    }
  }
  // a monster's missiles on a hero
  Hero h = fighter(g);
  Party party;
  party.fireShield = 2;
  for (int missile : {MissileFireball, MissileLightning, MissilePoison, MissilePoisonCloud}) {
    for (int range : {20, 60, 150}) {
      MissileState at{true, range, 70};
      HeroHit hit = missileOnHero(missile, at, h, party, g, dice);
      compare(std::string("on hero: ") + missileName(missile) + " range " + std::to_string(range),
              hit.struck, [&] { return sampleMissileOnHero(d, missile, at, h, party, g, true); });
      if (!hit.others.empty() && missile != MissilePoisonCloud)
        compare("  ... the other heroes", hit.others,
                [&] { return sampleMissileOnHero(d, missile, at, h, party, g, false); });
    }
  }
  // fading in flight
  MissileState m = flyMissile(30, 90, 8, 3);
  expect(m.arrives && m.range == 6 && m.damage == 66, "missile fades by its decay each step");
  expect(!flyMissile(30, 90, 8, 4).arrives, "a missile out of range is gone");
  expect(flightSteps(1, false) == 0 && flightSteps(3, true) == 5, "flight steps");
  // mana: FUL IR at MON = 6 + (5*28>>3) + (7*28>>3)
  Cast c = castSpell(g, caster, MissileFireball, 6);
  expect(c.mana == 6 + ((5 * 28) >> 3) + ((7 * 28) >> 3), "mana of a MON fireball");
  expect(c.range == (2 * 9 + 4) * 8 && c.decay == 10 - 60 / 8, "range and decay of a MON fireball");
}

static void recovery()
{
  Hero h;
  h.maxHp = 200; h.hp = 100;
  h.maxStamina = 1000; h.stamina = 1000;
  h.maxMana = 80; h.mana = 80;
  h.attr[Vitality] = 52;   // 52 + 12 = 64: the clock values 0, 16, 32, 48 pass
  h.food = 2000; h.water = 2000;
  RecoveryResult r = recover(h, RestStanding, 0);
  expect(r.hpUpdates == 4 && r.hpGain == 2, "hit points: 4 of 8 updates give 2");
  // 100 hit points at 2 per giving update = 50 giving updates; they come 4 in a
  // every 512 ticks (at 128, 256, 384, 512): 12 rounds and two more
  expect(r.hp.reached, "hit points come back");
  expect(r.hp.ticks == 512 * 12 + 256, "hit points: the tick they are full");
  // at full stamina every update takes 2 food and 1 water
  expect(r.hp.food == 2 * (r.hp.ticks / 64) && r.hp.water == r.hp.ticks / 64,
         "food and water while waiting at full stamina");
  // sleeping: updates every 16 ticks, gains doubled
  RecoveryResult s = recover(h, RestSleeping, 0);
  expect(s.updateEvery == 16 && s.hpGain == 4 && s.hp.ticks < r.hp.ticks / 4,
         "sleeping heals more than four times as fast");
  // stamina: a tired hero eats more
  Hero t = h;
  t.hp = t.maxHp;
  t.stamina = 100;
  RecoveryResult rs = recover(t, RestStanding, 0);
  expect(rs.stamina.reached && rs.stamina.food > 2 * (rs.stamina.ticks / 64),
         "regaining stamina costs extra food");
  // no food, no water: stamina falls, then hit points
  Hero starving = h;
  starving.food = -600;
  starving.water = -600;
  starving.stamina = 20;
  RecoveryResult rd = recover(starving, RestStanding, 0, 4000);
  expect(rd.starving && rd.hpLostToExhaustion > 0 && !rd.stamina.reached, "starving hurts");
  // mana burns stamina
  Hero m = h;
  m.hp = m.maxHp;
  m.mana = 0;
  m.attr[Wisdom] = 60;
  m.mastery[SkillWizard] = 5;
  m.mastery[SkillPriest] = 4;   // 9 + 60 = 69: 5 of 8 updates; cost 3 * max(7, 16 - 9) = 21
  RecoveryResult rm = recover(m, RestStanding, 0);
  expect(rm.manaUpdates == 5 && rm.manaGain == 3 && rm.manaStaminaCost == 21, "mana step");
  expect(rm.mana.reached && rm.staminaForMana == 21 * 27, "mana: 27 giving updates burn stamina");
  std::printf("     recovery: hp full after %d ticks awake, %d asleep; mana after %d ticks, %d stamina burned\n",
              r.hp.ticks, s.hp.ticks, rm.mana.ticks, rm.staminaForMana);
}

// The readers cannot be checked without game files, which are not part of the sources
static int checkFiles(int argc, char **argv)
{
  try {
    GameData g = loadGraphicsDat(argv[2]);
    bool dm = g.actions[19].energy == builtinDungeonMaster().actions[19].energy;
    const GameData &b = dm ? builtinDungeonMaster() : builtinChaosStrikesBack();
    bool same = g.monsters.size() == b.monsters.size();
    for (size_t i = 0; same && i < g.monsters.size(); i++)
      same = g.monsters[i].defense == b.monsters[i].defense && g.monsters[i].attack == b.monsters[i].attack &&
             g.monsters[i].woundNibbles == b.monsters[i].woundNibbles;
    for (size_t i = 0; same && i < g.actions.size(); i++)
      same = g.actions[i].energy == b.actions[i].energy && g.actions[i].damage == b.actions[i].damage;
    std::printf("%s: %s tables, %s the built-in copy\n", argv[2], dm ? "Dungeon Master" : "Chaos Strikes Back",
                same ? "the same as" : "different from");
    for (int i = 3; i < argc; i++) {
      SaveGame s = loadSaveGame(argv[i], g);
      size_t groups = 0;
      for (auto &l : s.levels) groups += l.groups.size();
      expect(!s.heroes.empty() && s.warnings.empty(), std::string("savegame reads in full: ") + argv[i]);
      std::printf("%s: %s, %zu heroes, %zu levels, %zu monster groups, %zu warnings\n", argv[i],
                  s.format.c_str(), s.heroes.size(), s.levels.size(), groups, s.warnings.size());
    }
  } catch (const std::exception &e) {
    std::printf("FAIL %s\n", e.what());
    return 1;
  }
  return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
  if (argc > 2 && std::string(argv[1]) == "--files") return checkFiles(argc, argv);
  if (argc > 1) trials = std::atol(argv[1]);
  const GameData &g = builtinDungeonMaster();
  expect(g.monsters.size() == 27 && g.weapons.size() == 46 && g.armours.size() == 58 &&
           g.actions.size() == 44 && g.spells.size() == 25,
         "built-in tables are complete");
  expect(builtinChaosStrikesBack().actions[19].energy == 40 && g.actions[19].energy == 26,
         "Berzerk costs 40 stamina in Chaos Strikes Back, 26 in Dungeon Master");
  heroAttacks(g);
  monsterAttacks(g);
  spells(g);
  recovery();
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
