#ifndef AUTOMAPFILE_H
#define AUTOMAPFILE_H

#include <cstdint>
#include <iosfwd>

#include "AutoMapCellMemory.h"
#include "AutoMapExplored.h"
#include "AutoMapItemMemory.h"
#include "AutoMapKnowledge.h"
#include "AutoMapMonsterDamage.h"
#include "AutoMapPlayerNotes.h"
#include "HeroStats.h"
#include "HeroStatsOrigins.h"

// The two words the engine's Signature() computes over a saved game file
struct AutoMapSaveSignature
{
  uint32_t first;
  uint32_t second;
};

enum AutoMapFileResult
{
  AutoMapFileRead,
  AutoMapFileIsNotAMapFile,
  AutoMapFileHasUnknownVersion,
  AutoMapFileBelongsToAnotherSave,
  AutoMapFileHasOtherLevels,
  AutoMapFileIsTruncated
};

const char *AutoMapFile_Describe(AutoMapFileResult result);

// The companion file of one saved game: what the party has explored, seen and learned, bound to
// that save by its signature. On disk this data is the first entry of a zip archive
// (AutoMapZip.h); the hero statistics are a second entry (AutoMapStatsFile, below).
//
//   4 bytes  "CSBM"
//   2 bytes  format version
//   4 bytes  first signature word of the saved game
//   4 bytes  second signature word of the saved game
//   2 bytes  number of levels
//   per level: 1 byte width, 1 byte height,
//              the explored bits (see AutoMapExplored),
//              the seen-face bits (4 per cell, see AutoMapExplored),
//              the glimpsed-face bits (4 per cell, version 5),
//              the read-text bits (4 per cell, version 6)
//              the looked-into bits (4 per cell, version 7),
//              the rim's explored, seen-face, glimpsed-face, read-text and looked-into bits
//              (1 and 4 per rim cell, see AutoMapExplored::RimBitsOfLevel, version 11)
//   4 bytes  number of facts
//   per fact: 1 byte kind, 4 bytes record, 1 byte level, 1 byte x, 1 byte y
//             (a record fact has 0xff for level, x and y; a cell fact has 0xffffffff for record)
//   4 bytes  number of remembered floor items (version 3)
//   per item: 4 bytes record, 2 bytes description index, 1 byte level, 1 byte x, 1 byte y,
//             1 byte 1 for a mere glimpse in the dark, else 0 (version 4)
//   4 bytes  number of remembered cell looks (version 8)
//   per look: 1 byte each level, x, y, kind, door state, pit open, pit hidden,
//             spinner field visible, 2 bytes floor picture, 2 bytes for each of the four
//             side pictures (0xffff for no picture)
//   4 bytes  number of player pit marks (version 9)
//   per mark: 1 byte each level, x, y, open (1 open, 0 closed)
//   4 bytes  number of player links (version 9)
//   per link: 1 byte each source level, x, y, side (0-3, 0xff for a floor source),
//             then target level, x, y (the target lies on the source's level)
//   4 bytes  number of damaged monster groups (version 10)
//   per group: 4 bytes record, 2 bytes damage of each of the four members
//   (versions 13 to 25 only) 4 bytes length of the statistics block that follows, then the
//            block as the statistics entry holds it after its header (AutoMapStatsFile), of the
//            layout numbered like the file's version
//
// Numbers are stored lowest byte first. Version 1 files have no face bits and no facts,
// version 2 files no floor items, version 3 files no glimpses, version 4 files no glimpsed
// faces, version 5 files no read texts (a seen face counts as read), version 6 files no
// looked-into bits (a seen face counts as looked into), version 7 files no cell looks,
// version 8 files no player notes, version 9 files no damage, version 10 files no rims (a painted
// rim cell is taken from the floor beside it, AutoMapExplored::TakeRimMarksFromTheEdge), version 11
// files no seen floors (an explored cell counts as seen), version 12 files no statistics, versions
// 13 to 25 the statistics inside them; they are all still read. From version 26 on the statistics
// are an entry of their own, so that a change to them leaves this format as it is.
class AutoMapFile
{
public:
  explicit AutoMapFile(const AutoMapSaveSignature &saveSignature);
  // The caller checks the stream for write errors
  void Write(std::ostream &out, const AutoMapExplored &explored,
             const AutoMapKnowledge &knowledge, const AutoMapItemMemory &items, const AutoMapCellMemory &cells,
             const AutoMapPlayerNotes &notes, const AutoMapMonsterDamage &damage) const;
  // explored must already have the shape of the loaded dungeon. No store is changed unless the
  // whole file was read. *hadItemMemory is false for a file older than version 3: the caller
  // then has to fill the item memory itself; likewise *hadCellMemory for a file older than
  // version 8; likewise *notes is left as it is for a file older than version 9, and *damage
  // for a file older than version 10. *hadStatistics is true for a file older than version 26,
  // which speaks for the statistics: they come out in *stats and *origins, empty (Forget) for a
  // file older than version 13; the caller decides whether to keep the hardship it has in memory
  // (HeroStats::TakeFromFile). For a newer file *hadStatistics is false and *stats and *origins
  // are left as they are: the statistics entry holds them.
  AutoMapFileResult Read(std::istream &in, AutoMapExplored *explored,
                         AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                         bool *hadItemMemory, AutoMapCellMemory *cells,
                         bool *hadCellMemory, AutoMapPlayerNotes *notes,
                         AutoMapMonsterDamage *damage, HeroStats *stats,
                         HeroStatsOrigins *origins, bool *hadStatistics) const;

private:
  AutoMapSaveSignature m_saveSignature;
};

// The statistics entry of the .map file: the hero statistics of one saved game, bound to that
// save by its signature like the map. Its layout number is HeroStats::LAYOUT and goes up on its
// own; it continues the map's version numbers, so layout N reads what a map file of version N
// held.
//
//   4 bytes  "CSBS"
//   2 bytes  layout (25 or later)
//   4 bytes  first signature word of the saved game
//   4 bytes  second signature word of the saved game
//   4 bytes  length S of the hero statistics, S bytes of them (HeroStats::Serialize), the rest
//            being the origins (HeroStatsOrigins::Serialize). Every table is a 4 byte count and
//            its records; numbers inside are lowest byte first and ints are 4 bytes:
//     champions: per champion 2 bytes id, 1 byte name length, the name
//     visited levels: per level 4 bytes
//     attacks: per row 2 bytes champion, level, weapon, 1 byte thrown, option, tries, hits,
//              damage, kills, then (version 15) uses of actions that cannot hit
//     damage dealt: champion, level, kind, amount; kills: champion, level, amount
//     monster kills: level, monster type, variant, blow-by, 2 bytes champion, count
//     harm: 2 bytes victim, level, source, 2 bytes causer, monster type, amount, then (version
//           14) the 4 byte missile kind of a monster's missile (HERO_SPELL_MONSTER_MISSILE | kind)
//     healing: 2 bytes champion, level, kind, 2 bytes brewer, amount
//     mana gained: 2 bytes champion, level, gain, amount
//     spells: 2 bytes champion, level, 4 byte spell key, casts, mana, failed, wasted, damage, kills
//     deaths: 2 bytes champion, level, cause, then the blow (1 byte by, 2 champion, 1 kind,
//             weapon, 1 byte thrown, option, 4 byte spell, monster type)
//     resurrections: 2 bytes champion, level, count
//     then the party wipes and the reloads, 4 bytes each
//     movement (layout 14): per level 4 bytes level, steps, turns, teleports, ticks, then
//              (layout 15) ticks asleep, pits fallen into, pits climbed down, bumps
//     items (layout 16): per row level, item (basic object type), picked up, used in walls,
//              then (layout 24) taken from chests (a part of picked up)
//     eaten and drunk (layout 17): 2 bytes champion, level, item, kind (0 drunk, 1 eaten), count
//     doors bashed (layout 17): 2 bytes champion, level, count, then (layout 18) the method
//              (0 smashed, 1 shot open, 2 by spell, 4 a closed door opened by a Zo spell; 5 opened
//              and 6 closed with the door's button, champion 0xffff)
//     chests opened (layout 17): level, count
//     food and water (layout 18): 2 bytes champion, level, food gained, food wasted, water
//              gained, water wasted, fountain drinks
//     tiles walked (layout 19): per cell level, 1 byte x, 1 byte y
//     scrolls read (layout 20): level, count
//     rune mana (layout 21 only, skipped when read): 2 bytes champion, level, mana
//     rune outcomes (layout 22): 2 bytes champion, level the runes were entered on, outcome
//              (0 cast, 1 failed, 2 cleared), mana; then the runes kept: per champion 2 bytes
//              champion, rune count, per rune level entered on, mana
//     experience (layout 23): per champion, level, 1 byte skill and source (1 byte kind,
//              weapon, option, 4 bytes spell, monster type) the amount; the ranks gained (2 bytes
//              champion, level, 1 byte class, 1 byte rank, 4 bytes game time, source); the start
//              ranks (2 bytes champion, 1 byte class, 1 byte rank)
//     records (layout 23): per record 1 byte kind and its entry (value, 2 bytes champion, level,
//              4 bytes game time, source, monster type, variant, count, extra, first level, 4
//              bytes first time, two (monster type, kills)); the kill streaks running (2 bytes
//              champion, then as the chains); the fall, teleport and sleep chains running (count,
//              first level, level, 4 bytes first time, 4 bytes time, (monster type, kills) list);
//              then (layout 25) the stretches of poison running (2 bytes champion, first level,
//              level, 4 bytes first time, 4 bytes time, poison damage, doses)
//     events (layout 25): 2 bytes champion, level, kind (0 poisoned, 1 wounded), count
//     origins: missiles and clouds (4 byte index, the blow), brewers (4 byte index, 2 bytes
//              champion), then (layout 16) the held items, (layout 17) the opened chests and
//              (layout 20) the read scrolls (4 byte record key each)
//
// Layout 13 has no missile kinds and no movement, layout 14 no sleep, pits or bumps, layout 15
// no items, layout 16 nothing eaten or drunk, no doors bashed and no chests opened, layout 17 no
// door methods and no food and water, layout 18 no tiles walked, layout 19 no scrolls read,
// layouts 20 and 21 no rune outcomes (taken from the rune spells' rows), layout 22 no experience
// and no records, layout 23 no items taken from chests, layout 24 no poisonings, wounds or
// stretches of poison. These older layouts come only inside map files of those versions.
class AutoMapStatsFile
{
public:
  explicit AutoMapStatsFile(const AutoMapSaveSignature &saveSignature);
  // The caller checks the stream for write errors
  void Write(std::ostream &out, const HeroStats &stats, const HeroStatsOrigins &origins) const;
  // Reads the rest of the stream as the entry. Neither store is changed unless all of it was read.
  AutoMapFileResult Read(std::istream &in, HeroStats *stats, HeroStatsOrigins *origins) const;

private:
  AutoMapSaveSignature m_saveSignature;
};

#endif
