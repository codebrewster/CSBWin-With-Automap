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
// that save by its signature.
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
//   4 bytes  length of the statistics block that follows (version 13), then in it:
//            4 bytes length S of the hero statistics, S bytes of them (HeroStats::Serialize),
//            the rest being the origins (HeroStatsOrigins::Serialize). Every table is a 4 byte
//            count and its records; numbers inside are lowest byte first and ints are 4 bytes:
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
//     movement (version 14): per level 4 bytes level, steps, turns, teleports, ticks, then
//              (version 15) ticks asleep, pits fallen into, pits climbed down, bumps
//     items (version 16): per row level, item (basic object type), picked up, used in walls
//     eaten and drunk (version 17): 2 bytes champion, level, item, kind (0 drunk, 1 eaten), count
//     doors bashed (version 17): 2 bytes champion, level, count, then (version 18) the method
//              (0 smashed, 1 shot open, 2 by spell, 4 a closed door opened by a Zo spell; 5 opened
//              and 6 closed with the door's button, champion 0xffff)
//     chests opened (version 17): level, count
//     food and water (version 18): 2 bytes champion, level, food gained, food wasted, water
//              gained, water wasted, fountain drinks
//     tiles walked (version 19): per cell level, 1 byte x, 1 byte y
//     scrolls read (version 20): level, count
//     origins: missiles and clouds (4 byte index, the blow), brewers (4 byte index, 2 bytes
//              champion), then (version 16) the held items, (version 17) the opened chests and
//              (version 20) the read scrolls (4 byte record key each)
//
// Numbers are stored lowest byte first. Version 1 files have no face bits and no facts,
// version 2 files no floor items, version 3 files no glimpses, version 4 files no glimpsed
// faces, version 5 files no read texts (a seen face counts as read), version 6 files no
// looked-into bits (a seen face counts as looked into), version 7 files no cell looks,
// version 8 files no player notes, version 9 files no damage, version 10 files no rims (a painted
// rim cell is taken from the floor beside it, AutoMapExplored::TakeRimMarksFromTheEdge), version 11
// files no seen floors (an explored cell counts as seen), version 12 files no statistics, version
// 13 files no missile kinds and no movement, version 14 files no sleep, pits or bumps, version 15
// files no items, version 16 files nothing eaten or drunk, no doors bashed and no chests opened,
// version 17 files no door methods and no food and water, version 18 files no tiles walked,
// version 19 files no scrolls read; they are still read.
class AutoMapFile
{
public:
  explicit AutoMapFile(const AutoMapSaveSignature &saveSignature);
  // The caller checks the stream for write errors
  void Write(std::ostream &out, const AutoMapExplored &explored,
             const AutoMapKnowledge &knowledge, const AutoMapItemMemory &items, const AutoMapCellMemory &cells,
             const AutoMapPlayerNotes &notes, const AutoMapMonsterDamage &damage, const HeroStats &stats,
             const HeroStatsOrigins &origins) const;
  // explored must already have the shape of the loaded dungeon. No store is changed unless the
  // whole file was read. *hadItemMemory is false for a file older than version 3: the caller
  // then has to fill the item memory itself; likewise *hadCellMemory for a file older than
  // version 8; likewise *notes is left as it is for a file older than version 9, and *damage
  // for a file older than version 10. For a file older than version 13 *stats and *origins come
  // out empty (Forget); the caller decides whether to keep the hardship it has in memory (HeroStats::TakeFromFile).
  AutoMapFileResult Read(std::istream &in, AutoMapExplored *explored,
                         AutoMapKnowledge *knowledge, AutoMapItemMemory *items,
                         bool *hadItemMemory, AutoMapCellMemory *cells,
                         bool *hadCellMemory, AutoMapPlayerNotes *notes,
                         AutoMapMonsterDamage *damage, HeroStats *stats,
                         HeroStatsOrigins *origins) const;

private:
  AutoMapSaveSignature m_saveSignature;
};

#endif
