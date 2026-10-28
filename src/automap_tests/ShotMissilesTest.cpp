#include "TestSupport.h"
#include "../ShotMissiles.h"

static const int ARROW_RECORD = 3;
static const int SECOND_ARROW_RECORD = 4;
static const int LIGHTNING_BOLT_RECORD = 5;

static void MissileIsShotFromRememberedUntilForgotten(void)
{
  ForgetAllShotMissiles();
  CHECK(!IsShotMissile(ARROW_RECORD));
  RememberShotMissile(ARROW_RECORD);
  CHECK(IsShotMissile(ARROW_RECORD));
  ForgetShotMissile(ARROW_RECORD);
  CHECK(!IsShotMissile(ARROW_RECORD));
}

// The engine asks for the damage of a hit, and a door may then let the missile fly on
static void AskingWhetherAMissileIsShotLeavesItShot(void)
{
  ForgetAllShotMissiles();
  RememberShotMissile(ARROW_RECORD);
  CHECK(IsShotMissile(ARROW_RECORD));
  CHECK(IsShotMissile(ARROW_RECORD));
}

// The lightning bolt of a Wizard Eye hits while an arrow of the party is still flying
static void OtherMissileIsNotShotWhileAShotOneFlies(void)
{
  ForgetAllShotMissiles();
  RememberShotMissile(ARROW_RECORD);
  CHECK(!IsShotMissile(LIGHTNING_BOLT_RECORD));
  ForgetShotMissile(LIGHTNING_BOLT_RECORD);
  CHECK(IsShotMissile(ARROW_RECORD));
}

static void EachOfSeveralShotMissilesIsForgottenOnItsOwn(void)
{
  ForgetAllShotMissiles();
  RememberShotMissile(ARROW_RECORD);
  RememberShotMissile(SECOND_ARROW_RECORD);
  ForgetShotMissile(SECOND_ARROW_RECORD);
  CHECK(IsShotMissile(ARROW_RECORD));
  CHECK(!IsShotMissile(SECOND_ARROW_RECORD));
  ForgetShotMissile(ARROW_RECORD);
  CHECK(!IsShotMissile(ARROW_RECORD));
}

// A mark is not counted: one forgetting clears a record however often it was remembered
static void RecordRememberedTwiceIsForgottenOnce(void)
{
  ForgetAllShotMissiles();
  RememberShotMissile(ARROW_RECORD);
  RememberShotMissile(ARROW_RECORD);
  ForgetShotMissile(ARROW_RECORD);
  CHECK(!IsShotMissile(ARROW_RECORD));
}

static void NothingIsShotAfterForgettingAll(void)
{
  ForgetAllShotMissiles();
  RememberShotMissile(ARROW_RECORD);
  RememberShotMissile(SECOND_ARROW_RECORD);
  ForgetAllShotMissiles();
  CHECK(!IsShotMissile(ARROW_RECORD));
  CHECK(!IsShotMissile(SECOND_ARROW_RECORD));
}

// A shot that finds no free record must not mark the next missile
static void ExpectedShotIsTakenOnce(void)
{
  ForgetAllShotMissiles();
  CHECK(!TakeExpectedShotMissile());
  ExpectShotMissile();
  CHECK(TakeExpectedShotMissile());
  CHECK(!TakeExpectedShotMissile());
}

static void NoShotIsExpectedAfterForgettingAll(void)
{
  ExpectShotMissile();
  ForgetAllShotMissiles();
  CHECK(!TakeExpectedShotMissile());
}

void TestShotMissiles(void)
{
  MissileIsShotFromRememberedUntilForgotten();
  AskingWhetherAMissileIsShotLeavesItShot();
  OtherMissileIsNotShotWhileAShotOneFlies();
  EachOfSeveralShotMissilesIsForgottenOnItsOwn();
  RecordRememberedTwiceIsForgottenOnce();
  NothingIsShotAfterForgettingAll();
  ExpectedShotIsTakenOnce();
  NoShotIsExpectedAfterForgettingAll();
}
