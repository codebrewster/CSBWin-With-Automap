#ifndef AUTOMAPKNOWLEDGE_H
#define AUTOMAPKNOWLEDGE_H

#include <cstdint>
#include <set>
#include <vector>
#include "AutoMapExplored.h"

// A dungeon record, as (database number << 16) | index. The position bits of the engine's RN
// are left out because they change when an item moves within its cell.
typedef uint32_t AutoMapRecordId;
const AutoMapRecordId AUTOMAP_NO_RECORD = 0xffffffffu;

enum AutoMapSpoilerLevel
{
  AutoMapSpoilersAll = 0,
  AutoMapSpoilersLearned = 1,
  AutoMapSpoilersSeen = 2
};

// What the party has found out by playing. The numbers are stored in the .map file and never change.
enum AutoMapFactKind
{
  AutoMapFactActuator = 1,          // record: the actuator fired where the party could tell
  AutoMapFactLockRequirement = 2,   // record: an item in hand opened this lock
  AutoMapFactChestOpened = 3,       // record
  AutoMapFactScrollRead = 4,        // record
  AutoMapFactFalseWall = 5,         // cell
  AutoMapFactPit = 6,               // cell
  AutoMapFactTeleporter = 7,        // cell: teleporter or spinner that moved or turned the party
  AutoMapFactLink = 8               // record (source actuator) and cell (target)
};

struct AutoMapFact
{
  AutoMapFactKind kind;
  AutoMapRecordId record;   // AUTOMAP_NO_RECORD for cell facts
  AutoMapCell cell;         // {-1, -1, -1} for record facts; level coordinates
};

AutoMapFact AutoMapFact_OfRecord(AutoMapFactKind kind, AutoMapRecordId record);
AutoMapFact AutoMapFact_OfCell(AutoMapFactKind kind, const AutoMapCell &cell);
AutoMapFact AutoMapFact_OfLink(AutoMapRecordId source, const AutoMapCell &target);
bool AutoMapFact_KindIsKnown(int kind);   // 1..8
bool operator<(const AutoMapFact &left, const AutoMapFact &right);   // kind, record, level, x, y

// Facts are never removed during play: what the party has learned stays learned.
class AutoMapKnowledge
{
public:
  void Forget(void);
  // Facts are only ever added; learning one twice keeps one
  void Learn(const AutoMapFact &fact);
  bool Knows(const AutoMapFact &fact) const;
  // Every fact in operator< order, for the .map file
  std::vector<AutoMapFact> Facts(void) const;

private:
  std::set<AutoMapFact> facts;
};

#endif
