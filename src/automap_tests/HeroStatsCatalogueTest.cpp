#include "TestSupport.h"
#include "../HeroStatsCatalogue.h"

static HeroStatsBlow ChampionBlow(HeroStatsChampion who, int weapon, HeroDamageKind kind)
{
  HeroStatsBlow blow = HeroStatsBlow_Nobody(HeroBlowByChampion);
  blow.champion = who;
  blow.weapon = weapon;
  blow.option = 1;
  blow.kind = kind;
  return blow;
}

// HALK (7) and SYRA (9) on levels 1 and 2
static HeroStats TwoChampions(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MarkLevelVisited(1);
  stats.MarkLevelVisited(2);
  HeroStatsBlow sword = ChampionBlow(7, 5, HeroDamageMelee);
  stats.CountAttackTry(1, sword);
  stats.CountAttackHit(1, sword);
  stats.CreditMonsterDamage(1, sword, 20);
  stats.CountMonsterKill(1, 6, 0, sword);
  HeroStatsBlow bow = ChampionBlow(9, 6, HeroDamageRanged);
  stats.CountAttackTry(2, bow);
  stats.CountAttackHit(2, bow);
  stats.CreditMonsterDamage(2, bow, 7);
  stats.CreditManaGained(7, 1, HeroManaRegeneration, 30);
  stats.CreditManaGained(9, 2, HeroManaPotion, 12);
  stats.CountCast(9, 2, 0x0405, 10);
  stats.CountFailedCast(9, 2, 0x0405, 4);
  stats.CountConsumed(7, 1, 3, HeroConsumeEaten);
  stats.CreditFood(7, 1, 500, 300);
  stats.CountDoorBashed(7, 2, HeroDoorSmashed);
  stats.CountStep(1);
  stats.CountStep(2);
  stats.CountTick(2, false);
  return stats;
}

static void EveryNumberHasOneTopic(void)
{
  int seen[HERO_NUMBER_COUNT] = {0};
  for (int topic = HeroTopicCombat; topic < HERO_TOPIC_COUNT; topic++)
  {
    const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(topic);
    for (size_t r = 0; r < info.rows.size(); r++) seen[info.rows[r].number]++;
    for (size_t t = 0; t < info.tiles.size(); t++)
    {
      bool inRows = false;
      for (size_t r = 0; r < info.rows.size(); r++) inRows = inRows || info.rows[r].number == info.tiles[t];
      CHECK(inRows);
    }
  }
  for (int n = 0; n < HERO_NUMBER_COUNT; n++)
  {
    // Kills belongs to Combat and is the first row of Monsters as well; the biggest hit, split
    // into melee and ranged, is in none
    int expected = n == HeroNumKills ? 2 : n == HeroNumRecBiggestHit ? 0 : 1;
    if (seen[n] != expected) std::printf("number %d (%s) is in %d topics\n", n, HeroStatsCatalogue_Info((HeroStatsNumber)n).label, seen[n]);
    CHECK(seen[n] == expected);
  }
  CHECK(std::string(HeroStatsCatalogue_Topic(HeroTopicSupplies).label) == "Food & items");
}

static void PartySumsItsChampions(void)
{
  HeroStats stats = TwoChampions();
  int levels[3] = {HERO_STATS_WHOLE_RUN, 1, 2};
  for (int l = 0; l < 3; l++)
  {
    HeroStatsRecord party = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, levels[l]);
    HeroStatsRecord halk = HeroStatsCatalogue_Record(stats, 7, levels[l]);
    HeroStatsRecord syra = HeroStatsCatalogue_Record(stats, 9, levels[l]);
    for (int n = 0; n < HERO_NUMBER_COUNT; n++)
    {
      HeroStatsNumber number = (HeroStatsNumber)n;
      if (!HeroStatsCatalogue_Summable(number) || HeroStatsCatalogue_Info(number).partyOnly) continue;
      CHECK(party.value[n] == halk.value[n] + syra.value[n]);
    }
  }
}

static void RecordMatchesTheStore(void)
{
  HeroStats stats = TwoChampions();
  HeroStatsRecord halk = HeroStatsCatalogue_Record(stats, 7, HERO_STATS_WHOLE_RUN);
  CHECK(halk.value[HeroNumDamage] == 20 && halk.value[HeroNumMelee] == 20);
  CHECK(halk.value[HeroNumKills] == 1);
  CHECK(halk.value[HeroNumManaRegeneration] == 30 && halk.value[HeroNumManaGenerated] == 30);
  CHECK(halk.value[HeroNumDoorsBashed] == stats.DoorsBashed(7, HERO_STATS_WHOLE_RUN));
  CHECK(halk.value[HeroNumDoorsSmashed] == 1);
  CHECK(halk.value[HeroNumEaten] == 1 && halk.value[HeroNumFoodGained] == 300 && halk.value[HeroNumFoodWasted] == 200);
  HeroStatsRecord syra = HeroStatsCatalogue_Record(stats, 9, 2);
  CHECK(syra.value[HeroNumManaPotions] == 12);
  CHECK(syra.value[HeroNumCasts] == 2 && syra.value[HeroNumCastsSuccessful] == 1 && syra.value[HeroNumCastsFailed] == 1);
  CHECK(syra.value[HeroNumManaWasted] == stats.ManaWasted(9, 2));
  CHECK(syra.value[HeroNumRunesCleared] == stats.RunesCleared(9, 2));
  // Spells is its two parts, and with the cleared runes and those not cast yet makes Mana spent
  CHECK(syra.value[HeroNumManaOnSpells] == syra.value[HeroNumManaSuccessful] + syra.value[HeroNumManaWasted]);
  CHECK(syra.value[HeroNumManaSpent] == syra.value[HeroNumManaOnSpells] + syra.value[HeroNumRunesCleared] +
                                        syra.value[HeroNumRunesNotCast]);
  HeroStatsRecord party = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 2);
  CHECK(party.value[HeroNumTicks] == stats.Movement(2).ticks);
  CHECK(party.value[HeroNumSteps] == 1);
}

static void RatiosAreUndefinedWithoutAttacks(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  HeroStatsRecord none = HeroStatsCatalogue_Record(stats, 7, HERO_STATS_WHOLE_RUN);
  CHECK(none.value[HeroNumHitRate] == -1 && none.value[HeroNumDamagePerHit] == -1);
  CHECK(HeroStatsCatalogue_Format(HeroNumHitRate, -1) == "-");
  HeroStatsBlow sword = ChampionBlow(7, 5, HeroDamageMelee);
  for (int i = 0; i < 3; i++) stats.CountAttackTry(1, sword);
  stats.CountAttackHit(1, sword);
  stats.CountAttackHit(1, sword);
  stats.CreditMonsterDamage(1, sword, 9);
  HeroStatsRecord some = HeroStatsCatalogue_Record(stats, 7, HERO_STATS_WHOLE_RUN);
  CHECK(some.value[HeroNumHitRate] == 67);
  CHECK(HeroStatsCatalogue_Format(HeroNumHitRate, 67) == "67%");
  CHECK(some.value[HeroNumDamagePerHit] == 5);  // 9 / 2, rounded
  CHECK(!HeroStatsCatalogue_Summable(HeroNumHitRate) && HeroStatsCatalogue_Summable(HeroNumDamage));
  CHECK(HeroStatsCatalogue_Format(HeroNumDamage, 4412) == "4,412");
}

static void PartyOnlyNumbersSkipChampions(void)
{
  CHECK(!HeroStatsCatalogue_Applies(HeroNumSteps, 7, HERO_STATS_WHOLE_RUN));
  CHECK(HeroStatsCatalogue_Applies(HeroNumSteps, HERO_STATS_PARTY, 3));
  CHECK(!HeroStatsCatalogue_Applies(HeroNumWipes, HERO_STATS_PARTY, 3));
  CHECK(HeroStatsCatalogue_Applies(HeroNumWipes, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN));
  CHECK(!HeroStatsCatalogue_Applies(HeroNumWipes, 7, HERO_STATS_WHOLE_RUN));
  CHECK(HeroStatsCatalogue_Applies(HeroNumDamage, 7, 3));
  // Steps count before the first champion joins
  HeroStats stats;
  stats.MarkLevelVisited(0);
  stats.CountStep(0);
  stats.CountStep(0);
  HeroStatsRecord party = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(party.value[HeroNumSteps] == 2);
  CHECK(party.value[HeroNumDamage] == 0);
  stats.CountPartyWipe();
  CHECK(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN).value[HeroNumWipes] == 1);
  CHECK(HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, 0).value[HeroNumWipes] == 0);
}

static void GridOffersChampionNumbersOnly(void)
{
  std::vector<HeroStatsNumber> doors = HeroStatsCatalogue_GridNumbers(HeroTopicExploration);
  HeroStatsNumber expected[5] = {HeroNumDoorsBashed, HeroNumDoorsSmashed, HeroNumDoorsShot, HeroNumDoorsBySpell, HeroNumDoorsByZo};
  CHECK(doors == std::vector<HeroStatsNumber>(expected, expected + 5));
  std::vector<HeroStatsNumber> combat = HeroStatsCatalogue_GridNumbers(HeroTopicCombat);
  for (size_t i = 0; i < combat.size(); i++) CHECK(combat[i] != HeroNumHitRate && combat[i] != HeroNumDamagePerHit);
  CHECK(!combat.empty() && combat[0] == HeroNumDamage);
}

static void OldPagesMapToTopics(void)
{
  CHECK(HeroStatsCatalogue_TopicOfOldPage(0) == HeroTopicOverview);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(1) == HeroTopicOverview);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(2) == HeroTopicOverview);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(3) == HeroTopicMonsters);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(4) == HeroTopicSupplies);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(-1) == HeroTopicOverview);
  CHECK(HeroStatsCatalogue_TopicOfOldPage(9) == HeroTopicOverview);
}

static void SummaryIsTheFirstThreeTiles(void)
{
  HeroStatsRecord record = {};
  record.value[HeroNumDamage] = 4412;
  record.value[HeroNumKills] = 31;
  record.value[HeroNumHitRate] = 63;
  CHECK(HeroStatsCatalogue_Summary(HeroTopicCombat, record, 7, HERO_STATS_WHOLE_RUN) ==
        "4,412 damage dealt \xc2\xb7 31 kills \xc2\xb7 63% hit rate");
  // An undefined ratio is left out: the next tile takes its place
  record.value[HeroNumHitRate] = -1;
  CHECK(HeroStatsCatalogue_Summary(HeroTopicCombat, record, 7, HERO_STATS_WHOLE_RUN) ==
        "4,412 damage dealt \xc2\xb7 31 kills \xc2\xb7 0 friendly fire caused");
  // When no tile applies (Exploration's are the party's numbers), a champion gets the topic's own top rows
  record.value[HeroNumDoorsBashed] = 2;
  CHECK(HeroStatsCatalogue_Summary(HeroTopicExploration, record, 7, HERO_STATS_WHOLE_RUN).find("2 doors bashed open") == 0);
  std::vector<HeroStatsNumber> own = HeroStatsCatalogue_TilesFor(HeroStatsCatalogue_Topic(HeroTopicExploration).tiles, HeroTopicExploration, 7, HERO_STATS_WHOLE_RUN);
  CHECK(!own.empty() && own[0] == HeroNumDoorsBashed);
  for (size_t i = 0; i < own.size(); i++) CHECK(HeroStatsCatalogue_Applies(own[i], 7, HERO_STATS_WHOLE_RUN));
  // Tiles that do apply are kept as they are, the others skipped
  std::vector<HeroStatsNumber> party = HeroStatsCatalogue_TilesFor(HeroStatsCatalogue_Topic(HeroTopicExploration).tiles, HeroTopicExploration, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(party.size() == 4 && party[3] == HeroNumTeleports);
}

// One number alone is what the whole record says (Per level asks for one number per champion)
static void ValueMatchesTheRecord(void)
{
  HeroStats stats = TwoChampions();
  stats.CountPartyWipe();
  HeroStatsChampion who[3] = {HERO_STATS_PARTY, 7, 9};
  int levels[3] = {HERO_STATS_WHOLE_RUN, 1, 2};
  for (int w = 0; w < 3; w++)
    for (int l = 0; l < 3; l++)
    {
      HeroStatsRecord record = HeroStatsCatalogue_Record(stats, who[w], levels[l]);
      for (int n = 0; n < HERO_NUMBER_COUNT; n++)
      {
        int value = HeroStatsCatalogue_Value(stats, who[w], levels[l], (HeroStatsNumber)n);
        if (value != record.value[n]) std::printf("number %d: %d, record %d\n", n, value, record.value[n]);
        CHECK(value == record.value[n]);
      }
    }
}

// Every number has a name of its own for csb.cfg, and the name finds it again
static void EveryNumberHasAName(void)
{
  for (int n = 0; n < HERO_NUMBER_COUNT; n++)
  {
    std::string key = HeroStatsCatalogue_Key((HeroStatsNumber)n);
    CHECK(!key.empty() && key.find_first_of(" ,") == std::string::npos);
    std::vector<HeroStatsNumber> found = HeroStatsCatalogue_ParseNumbers(key, 1);
    // The biggest hit is not shown: its name finds the biggest melee hit
    HeroStatsNumber expected = n == HeroNumRecBiggestHit ? HeroNumRecBiggestMeleeHit : (HeroStatsNumber)n;
    CHECK(found.size() == 1 && found[0] == expected);
  }
  CHECK(HeroStatsCatalogue_Key(HeroNumHitRate) == "hit_rate");
  CHECK(HeroStatsCatalogue_Key(HeroNumOtherActions) == "other_actions");
}

// "damage, Kills  hit_rate,,bogus kills": commas or spaces, any case; unknown names and
// repeats are left out; no more than asked for
static void NumberListsAreForgiving(void)
{
  std::vector<HeroStatsNumber> list = HeroStatsCatalogue_ParseNumbers("damage, Kills  hit_rate,,bogus kills", 4);
  CHECK(list.size() == 3 && list[0] == HeroNumDamage && list[1] == HeroNumKills && list[2] == HeroNumHitRate);
  CHECK(HeroStatsCatalogue_ParseNumbers("damage kills taken healed deaths", 4).size() == 4);
  CHECK(HeroStatsCatalogue_ParseNumbers("", 4).empty());
  CHECK(HeroStatsCatalogue_JoinNumbers(list) == "damage, kills, hit_rate");
}

// The tiles and the Key numbers come from the settings; nothing usable keeps the defaults
static void TilesAndKeyNumbersFollowTheSettings(void)
{
  std::vector<HeroStatsNumber> combat = HeroStatsCatalogue_DefaultTiles(HeroTopicCombat);
  CHECK(combat == HeroStatsCatalogue_Topic(HeroTopicCombat).tiles);
  HeroStatsCatalogue_ApplySetting(HeroSettingCombatTiles, "attacks, other_actions");
  CHECK(HeroStatsCatalogue_Topic(HeroTopicCombat).tiles == std::vector<HeroStatsNumber>({HeroNumAttacks, HeroNumOtherActions}));
  HeroStatsCatalogue_ApplySetting(HeroSettingCombatTiles, "nonsense");
  CHECK(HeroStatsCatalogue_Topic(HeroTopicCombat).tiles == combat);

  HeroStatsCatalogue_ApplySetting(HeroSettingKeyNumbers, "steps, damage");
  const HeroStatsTopicInfo &overview = HeroStatsCatalogue_Topic(HeroTopicOverview);
  CHECK(overview.rows.size() == 2 && overview.rows[0].number == HeroNumSteps && overview.rows[1].depth == 0);
  HeroStatsCatalogue_ApplySetting(HeroSettingChampionTiles, "taken");
  CHECK(HeroStatsCatalogue_ChampionTiles() == std::vector<HeroStatsNumber>({HeroNumTaken}));

  // Every setting has its csb.cfg entry and writes its defaults out as names
  for (int s = 0; s < HERO_SETTING_COUNT; s++)
  {
    CHECK(std::string(HeroStatsCatalogue_SettingEntry((HeroStatsSetting)s)).rfind("stats_", 0) == 0);
    std::string defaults = HeroStatsCatalogue_SettingDefault((HeroStatsSetting)s);
    CHECK(!defaults.empty() && defaults.size() < 200);
    HeroStatsCatalogue_ApplySetting((HeroStatsSetting)s, defaults);
  }
  CHECK(HeroStatsCatalogue_Topic(HeroTopicCombat).tiles == combat);
  CHECK(HeroStatsCatalogue_Topic(HeroTopicOverview).rows.size() == 12);
  CHECK(HeroStatsCatalogue_ChampionTiles() == std::vector<HeroStatsNumber>({HeroNumDamage, HeroNumKills, HeroNumTaken, HeroNumDeaths}));
}

static void ExperienceAndRecordsHaveTopics(void)
{
  const HeroStatsTopicInfo &xp = HeroStatsCatalogue_Topic(HeroTopicExperience);
  CHECK(std::string(xp.label) == "Experience");
  CHECK(xp.rows.size() == 6 && xp.rows[0].number == HeroNumExperience && xp.rows[0].depth == 0);
  CHECK(xp.rows[1].number == HeroNumXpFighter && xp.rows[1].depth == 1 && xp.rows[4].number == HeroNumXpWizard);
  CHECK(xp.rows[5].number == HeroNumRanksGained && xp.rows[5].depth == 0);
  HeroStatsNumber xpTiles[4] = {HeroNumXpFighter, HeroNumXpNinja, HeroNumXpPriest, HeroNumXpWizard};
  CHECK(xp.tiles == std::vector<HeroStatsNumber>(xpTiles, xpTiles + 4));
  const HeroStatsTopicInfo &records = HeroStatsCatalogue_Topic(HeroTopicRecords);
  // Every record but the biggest hit (melee and ranged took its place, first)
  CHECK(std::string(records.label) == "Records" && records.rows.size() == HERO_RECORD_COUNT - 1);
  CHECK(records.rows.size() >= 3 && records.rows[0].number == HeroNumRecBiggestMeleeHit &&
        records.rows[1].number == HeroNumRecBiggestRangedHit && records.rows[2].number == HeroNumRecBiggestSpellHit);
  for (size_t r = 0; r < records.rows.size(); r++) CHECK(records.rows[r].number != HeroNumRecBiggestHit);
  HeroStatsNumber recordTiles[4] = {HeroNumRecBiggestMeleeHit, HeroNumRecBiggestRangedHit, HeroNumRecBiggestSpellTotal, HeroNumRecBiggestHitTaken};
  CHECK(records.tiles == std::vector<HeroStatsNumber>(recordTiles, recordTiles + 4));
  CHECK(HeroStatsCatalogue_Key(HeroNumRecKillStreak) == "kill_streak" && HeroStatsCatalogue_Key(HeroNumXpPriest) == "priest");
  CHECK(HeroStatsCatalogue_RecordOf(HeroNumRecClosestCall) == HeroRecClosestCall && HeroStatsCatalogue_IsRecord(HeroNumRecLongestSleep));
  CHECK(!HeroStatsCatalogue_IsRecord(HeroNumExperience));
  CHECK(std::string(HeroStatsCatalogue_SettingEntry(HeroSettingRecordsTiles)) == "stats_tiles_records");
  // A csb.cfg from before the split names the biggest hit: it gets the biggest melee hit
  std::vector<HeroStatsNumber> old = HeroStatsCatalogue_ParseNumbers("biggest_hit, closest_call", 4);
  CHECK(old.size() == 2 && old[0] == HeroNumRecBiggestMeleeHit && old[1] == HeroNumRecClosestCall);
  CHECK(HeroStatsCatalogue_SettingDefault(HeroSettingExperienceTiles) == "fighter, ninja, priest, wizard");
}

static HeroRecordEntry Record(HeroStatsChampion who, int value)
{
  HeroRecordEntry entry = HeroRecordEntry_Empty();
  entry.who = who;
  entry.level = 1;
  entry.value = value;
  return entry;
}

static void RecordValuesAreTheBest(void)
{
  HeroStats stats;
  stats.MeetChampion(7, "HALK");
  stats.MeetChampion(9, "SYRA");
  stats.MarkLevelVisited(1);
  stats.Records().Offer(HeroRecBiggestHit, Record(7, 120));
  stats.Records().Offer(HeroRecBiggestHit, Record(9, 150));
  stats.Records().Offer(HeroRecHeaviestLoad, Record(7, 384));
  stats.Records().Offer(HeroRecClosestCall, Record(9, 2));
  HeroStatsRecord party = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(party.value[HeroNumRecBiggestHit] == 150);
  CHECK(HeroStatsCatalogue_Record(stats, 7, 1).value[HeroNumRecBiggestHit] == 120);
  CHECK(party.value[HeroNumRecMostKillsAtOnce] == -1);
  CHECK(HeroStatsCatalogue_Format(HeroNumRecMostKillsAtOnce, -1) == "-");
  CHECK(HeroStatsCatalogue_Format(HeroNumRecHeaviestLoad, 384) == "38.4 kg");
  CHECK(HeroStatsCatalogue_Format(HeroNumRecClosestCall, 2) == "2 HP left");
  CHECK(HeroStatsCatalogue_Value(stats, 9, HERO_STATS_WHOLE_RUN, HeroNumRecClosestCall) == 2);
  stats.Experience().Credit(7, 1, 4, HeroXpSource_Other(), 30);
  stats.Experience().Credit(9, 1, 17, HeroXpSource_Other(), 12);
  party = HeroStatsCatalogue_Record(stats, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(party.value[HeroNumExperience] == 42 && party.value[HeroNumXpFighter] == 30 && party.value[HeroNumXpWizard] == 12);
}

static void RecordsAreNotSummable(void)
{
  CHECK(!HeroStatsCatalogue_Summable(HeroNumRecBiggestHit) && HeroStatsCatalogue_Summable(HeroNumExperience));
  CHECK(!HeroStatsCatalogue_Applies(HeroNumRecLongestFall, 7, HERO_STATS_WHOLE_RUN));
  CHECK(HeroStatsCatalogue_Applies(HeroNumRecLongestFall, HERO_STATS_PARTY, 3));
  CHECK(HeroStatsCatalogue_GridNumbers(HeroTopicRecords).size() == HERO_RECORD_COUNT - 1);
  CHECK(HeroStatsCatalogue_GridNumbers(HeroTopicExperience).size() == 6);
}

// The rows of a topic are the rows of its groups, in order; the groups the pages draw by
static void TopicsHaveGroups(void)
{
  for (int t = 0; t < HERO_TOPIC_COUNT; t++)
  {
    const HeroStatsTopicInfo &info = HeroStatsCatalogue_Topic(t);
    size_t at = 0;
    for (size_t g = 0; g < info.groups.size(); g++)
      for (size_t r = 0; r < info.groups[g].rows.size(); r++, at++)
        CHECK(at < info.rows.size() && info.rows[at].number == info.groups[g].rows[r].number);
    CHECK(at == info.rows.size());
  }
  const HeroStatsTopicInfo &exploration = HeroStatsCatalogue_Topic(HeroTopicExploration);
  CHECK(exploration.groups.size() == 5);
  if (exploration.groups.size() != 5) return;
  CHECK(std::string(exploration.groups[0].title) == "Movement" && exploration.groups[0].panel);
  CHECK(std::string(exploration.groups[1].title) == "Travel" && exploration.groups[1].rows[1].number == HeroNumTelefrags &&
        exploration.groups[1].rows[1].depth == 1 && std::string(HeroStatsCatalogue_RowLabel(exploration.groups[1].rows[1])) == "onto monsters");
  CHECK(std::string(HeroStatsCatalogue_RowLabel(exploration.groups[1].rows[0])) == "Teleports");
  CHECK(std::string(exploration.groups[4].title) == "Doors" && !exploration.groups[4].panel);
  CHECK(HeroStatsCatalogue_Topic(HeroTopicCombat).groups.size() == 2 && HeroStatsCatalogue_Topic(HeroTopicMagic).groups.size() == 3);
  CHECK(HeroStatsCatalogue_Topic(HeroTopicMonsters).groups.size() == 1 && std::string(HeroStatsCatalogue_Topic(HeroTopicMonsters).groups[0].title).empty());
  // The Key numbers csb.cfg chooses are one group too
  HeroStatsCatalogue_ApplySetting(HeroSettingKeyNumbers, "kills, damage");
  const HeroStatsTopicInfo &overview = HeroStatsCatalogue_Topic(HeroTopicOverview);
  CHECK(overview.groups.size() == 1 && overview.groups[0].rows.size() == 2 && overview.groups[0].rows[0].number == HeroNumKills);
  HeroStatsCatalogue_ApplySetting(HeroSettingKeyNumbers, "");
}

// The six causes add up to Monsters killed; an older save's telefrag kills leave Other causes
static void KillCausesAddUp(void)
{
  HeroStats killed;
  killed.CountMonsterKill(1, 3, 0, ChampionBlow(7, 5, HeroDamageMelee));
  killed.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByDoor));
  killed.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByPit));
  killed.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByTelefrag));
  killed.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByLanding));
  for (int i = 0; i < 3; i++) killed.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByOther));
  killed.SetLegacyTelefragKills(1, 2);
  HeroStatsRecord k = HeroStatsCatalogue_Record(killed, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(k.value[HeroNumTelefragged] == 3 && k.value[HeroNumLandedOn] == 1 && k.value[HeroNumOtherKills] == 1);
  CHECK(k.value[HeroNumMonstersKilled] == 8);
  CHECK(k.value[HeroNumMonstersKilled] == k.value[HeroNumKills] + k.value[HeroNumDoorKills] + k.value[HeroNumPitKills] +
        k.value[HeroNumTelefragged] + k.value[HeroNumLandedOn] + k.value[HeroNumOtherKills]);
  CHECK(HeroStatsCatalogue_Value(killed, HERO_STATS_PARTY, 1, HeroNumMonstersKilled) == 8);
  CHECK(HeroStatsCatalogue_Value(killed, HERO_STATS_PARTY, 1, HeroNumLandedOn) == 1);
  // More stored telefrag kills than "other" kills: Other causes is 0, the sum still holds
  killed.SetLegacyTelefragKills(1, 9);
  k = HeroStatsCatalogue_Record(killed, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(k.value[HeroNumOtherKills] == 0 && k.value[HeroNumTelefragged] == 4 && k.value[HeroNumMonstersKilled] == 8);
  // Only kills of no known cause can have been telefrags: a cloud's or a trap's never were
  HeroStats clouds;
  for (int i = 0; i < 2; i++) clouds.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByCloud));
  clouds.CountMonsterKill(1, 3, 0, HeroStatsBlow_Nobody(HeroBlowByOther));
  clouds.SetLegacyTelefragKills(1, 5);
  k = HeroStatsCatalogue_Record(clouds, HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(k.value[HeroNumTelefragged] == 1 && k.value[HeroNumOtherKills] == 2 && k.value[HeroNumMonstersKilled] == 3);
  // Nothing killed: every cause is 0
  HeroStatsRecord none = HeroStatsCatalogue_Record(HeroStats(), HERO_STATS_PARTY, HERO_STATS_WHOLE_RUN);
  CHECK(none.value[HeroNumMonstersKilled] == 0 && none.value[HeroNumOtherKills] == 0 && none.value[HeroNumTelefragged] == 0);
  // Landings are the events, on the level
  killed.CountLanding(1);
  CHECK(HeroStatsCatalogue_Value(killed, HERO_STATS_PARTY, 1, HeroNumLandings) == 1);
  // A climb down a rope onto monsters is a number of its own, no landing
  HeroStats climbed;
  climbed.CountClimbLanding(1);
  CHECK(HeroStatsCatalogue_Value(climbed, HERO_STATS_PARTY, 1, HeroNumClimbLandings) == 1);
  CHECK(HeroStatsCatalogue_Value(climbed, HERO_STATS_PARTY, 1, HeroNumLandings) == 0);
  CHECK(HeroStatsCatalogue_Key(HeroNumClimbLandings) == "climb_landings" && HeroStatsCatalogue_Key(HeroNumPitClimbs) == "pit_climbs");
  CHECK(!HeroStatsCatalogue_Applies(HeroNumClimbLandings, 7, HERO_STATS_WHOLE_RUN));
  // It stands under Pit climbs as "onto monsters"
  const HeroStatsTopicInfo &exploration = HeroStatsCatalogue_Topic(HeroTopicExploration);
  bool under = false;
  for (size_t i = 0; i + 1 < exploration.rows.size(); i++)
    if (exploration.rows[i].number == HeroNumPitClimbs)
      under = exploration.rows[i + 1].number == HeroNumClimbLandings && exploration.rows[i + 1].depth == 1 &&
              std::string(exploration.rows[i + 1].label) == "onto monsters";
  CHECK(under);
}

// A hovertext only where the label and its place do not say it
static void HovertextsSayWhatLabelsDoNot(void)
{
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumFoodGained).help).empty());
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumDoorsBashed).help).empty());
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumDoorKills).help).empty());
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumRecLongestSleep).help).empty());
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumTaken).help).find("walls run into") != std::string::npos);
  CHECK(std::string(HeroStatsCatalogue_Info(HeroNumHitRate).help) == "Hits \xc3\xb7 Attacks.");
  CHECK(!std::string(HeroStatsCatalogue_Info(HeroNumLandedOn).help).empty());
  CHECK(HeroStatsCatalogue_ParseNumbers("telefrag_kills", 4) == std::vector<HeroStatsNumber>(1, HeroNumTelefragged));
  CHECK(HeroStatsCatalogue_Key(HeroNumMonstersKilled) == "monsters_killed" && HeroStatsCatalogue_Key(HeroNumLandings) == "landings");
  // A csb.cfg that still holds the Monsters tiles the game once wrote as defaults gets the new ones;
  // a list the player chose stays
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingMonstersTiles, "kills, door_kills, pit_kills, other_kills") ==
        "monsters_killed, kills, telefragged, landed_on");
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingMonstersTiles, "Kills door_kills,pit_kills , other_kills") ==
        "monsters_killed, kills, telefragged, landed_on");
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingMonstersTiles, "kills, other_kills") == "kills, other_kills");
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingCombatTiles, "kills, door_kills, pit_kills, other_kills") ==
        "kills, door_kills, pit_kills, other_kills");
  // The Exploration tiles the game once wrote as defaults get Teleports; a chosen list stays
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingExplorationTiles, "tiles_explored, steps, ticks, doors_bashed") ==
        "tiles_explored, steps, ticks, teleports");
  CHECK(HeroStatsCatalogue_CurrentSetting(HeroSettingExplorationTiles, "steps, doors_bashed") == "steps, doors_bashed");
  CHECK(HeroStatsCatalogue_SettingDefault(HeroSettingExplorationTiles) == "tiles_explored, steps, ticks, teleports");
}

void TestHeroStatsCatalogue(void)
{
  TopicsHaveGroups();
  KillCausesAddUp();
  HovertextsSayWhatLabelsDoNot();
  ExperienceAndRecordsHaveTopics();
  RecordValuesAreTheBest();
  RecordsAreNotSummable();
  EveryNumberHasAName();
  NumberListsAreForgiving();
  TilesAndKeyNumbersFollowTheSettings();
  ValueMatchesTheRecord();
  EveryNumberHasOneTopic();
  PartySumsItsChampions();
  RecordMatchesTheStore();
  RatiosAreUndefinedWithoutAttacks();
  PartyOnlyNumbersSkipChampions();
  GridOffersChampionNumbersOnly();
  OldPagesMapToTopics();
  SummaryIsTheFirstThreeTiles();
}
