#include "TestSupport.h"
#include "../AutoMapKnowledge.h"

static void NothingIsKnownAtFirst(void)
{
  AutoMapKnowledge knowledge;
  CHECK(!knowledge.Knows(AutoMapFact_OfRecord(AutoMapFactActuator, 7)));
  CHECK(knowledge.Facts().empty());
}

static void LearnedRecordFactIsKnownOnlyForItsKind(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 7));
  CHECK(knowledge.Knows(AutoMapFact_OfRecord(AutoMapFactActuator, 7)));
  CHECK(!knowledge.Knows(AutoMapFact_OfRecord(AutoMapFactLockRequirement, 7)));
  CHECK(!knowledge.Knows(AutoMapFact_OfRecord(AutoMapFactActuator, 8)));
}

static void CellFactsKeepLevelAndPosition(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{2, 5, 6}));
  CHECK(knowledge.Knows(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{2, 5, 6})));
  CHECK(!knowledge.Knows(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{2, 6, 5})));
  CHECK(!knowledge.Knows(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{1, 5, 6})));
  CHECK(!knowledge.Knows(AutoMapFact_OfCell(AutoMapFactFalseWall, AutoMapCell{2, 5, 6})));
}

static void LinkNeedsBothEnds(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfLink(7, AutoMapCell{0, 3, 4}));
  CHECK(knowledge.Knows(AutoMapFact_OfLink(7, AutoMapCell{0, 3, 4})));
  CHECK(!knowledge.Knows(AutoMapFact_OfLink(7, AutoMapCell{0, 3, 5})));
  CHECK(!knowledge.Knows(AutoMapFact_OfLink(8, AutoMapCell{0, 3, 4})));
}

static void LearningTwiceKeepsOne(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 3));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactChestOpened, 3));
  CHECK(knowledge.Facts().size() == 1);
}

static void FactsComeOutInOrder(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfCell(AutoMapFactPit, AutoMapCell{0, 1, 1}));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactScrollRead, 9));
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 2));
  std::vector<AutoMapFact> facts = knowledge.Facts();
  CHECK(facts.size() == 3);
  CHECK(facts[0].kind == AutoMapFactActuator);
  CHECK(facts[1].kind == AutoMapFactScrollRead);
  CHECK(facts[2].kind == AutoMapFactPit);
}

static void ForgetClearsEverything(void)
{
  AutoMapKnowledge knowledge;
  knowledge.Learn(AutoMapFact_OfRecord(AutoMapFactActuator, 1));
  knowledge.Learn(AutoMapFact_OfCell(AutoMapFactTeleporter, AutoMapCell{0, 0, 0}));
  knowledge.Forget();
  CHECK(knowledge.Facts().empty());
}

static void OnlyTheEightKindsAreKnown(void)
{
  CHECK(!AutoMapFact_KindIsKnown(0));
  for (int kind = 1; kind <= 8; kind++)
    CHECK(AutoMapFact_KindIsKnown(kind));
  CHECK(!AutoMapFact_KindIsKnown(9));
}

void TestAutoMapKnowledge(void)
{
  NothingIsKnownAtFirst();
  LearnedRecordFactIsKnownOnlyForItsKind();
  CellFactsKeepLevelAndPosition();
  LinkNeedsBothEnds();
  LearningTwiceKeepsOne();
  FactsComeOutInOrder();
  ForgetClearsEverything();
  OnlyTheEightKindsAreKnown();
}
