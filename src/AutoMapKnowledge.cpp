// AutoMapKnowledge.cpp : remembers what the party has found out by playing.
#include "AutoMapKnowledge.h"

AutoMapFact AutoMapFact_OfRecord(AutoMapFactKind kind, AutoMapRecordId record)
{
  AutoMapFact fact;
  fact.kind = kind;
  fact.record = record;
  fact.cell = AutoMapCell{-1, -1, -1};
  return fact;
}

AutoMapFact AutoMapFact_OfCell(AutoMapFactKind kind, const AutoMapCell &cell)
{
  AutoMapFact fact;
  fact.kind = kind;
  fact.record = AUTOMAP_NO_RECORD;
  fact.cell = cell;
  return fact;
}

AutoMapFact AutoMapFact_OfLink(AutoMapRecordId source, const AutoMapCell &target)
{
  AutoMapFact fact;
  fact.kind = AutoMapFactLink;
  fact.record = source;
  fact.cell = target;
  return fact;
}

bool AutoMapFact_KindIsKnown(int kind)
{
  return kind >= AutoMapFactActuator && kind <= AutoMapFactLink;
}

bool operator<(const AutoMapFact &left, const AutoMapFact &right)
{
  if (left.kind != right.kind)
    return left.kind < right.kind;
  if (left.record != right.record)
    return left.record < right.record;
  if (left.cell.level != right.cell.level)
    return left.cell.level < right.cell.level;
  if (left.cell.x != right.cell.x)
    return left.cell.x < right.cell.x;
  return left.cell.y < right.cell.y;
}

void AutoMapKnowledge::Forget(void)
{
  facts.clear();
}

void AutoMapKnowledge::Learn(const AutoMapFact &fact)
{
  facts.insert(fact);
}

bool AutoMapKnowledge::Knows(const AutoMapFact &fact) const
{
  return facts.count(fact) != 0;
}

std::vector<AutoMapFact> AutoMapKnowledge::Facts(void) const
{
  return std::vector<AutoMapFact>(facts.begin(), facts.end());
}
