#include "TestSupport.h"
#include "../AutoMapMonsterDamage.h"

static const AutoMapRecordId GROUP = 0x40007;

static void CreditAddsUp(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 1, 12);
  damage.Credit(GROUP, 1, 5);
  CHECK(damage.DealtTo(GROUP, 1) == 17);
  CHECK(damage.DealtTo(GROUP, 0) == 0);
}

static void NothingIsCreditedForNoDamage(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 0, 0);
  damage.Credit(GROUP, 0, -2);
  damage.Credit(GROUP, 4, 9);
  damage.Credit(GROUP, -1, 9);
  damage.Credit(AUTOMAP_NO_RECORD, 0, 9);
  CHECK(damage.Groups().empty());
}

static void DamageStopsAtTheMost(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 0, 0xfff0);
  damage.Credit(GROUP, 0, 0x100);
  CHECK(damage.DealtTo(GROUP, 0) == AUTOMAP_MOST_DAMAGE);
}

static void RemovedMemberShiftsTheOthersDown(void)
{
  AutoMapMonsterDamage damage;
  for (int member = 0; member < 4; member++) damage.Credit(GROUP, member, 10 * (member + 1));
  damage.RemoveMember(GROUP, 1);
  CHECK(damage.DealtTo(GROUP, 0) == 10);
  CHECK(damage.DealtTo(GROUP, 1) == 30);
  CHECK(damage.DealtTo(GROUP, 2) == 40);
  CHECK(damage.DealtTo(GROUP, 3) == 0);
}

static void RemovingTheLastDamagedMemberDropsTheGroup(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 2, 8);
  damage.RemoveMember(GROUP, 2);
  CHECK(damage.Groups().empty());
}

static void ANewGroupInAFreedRecordStartsClean(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 0, 8);
  damage.ForgetGroup(GROUP);
  CHECK(damage.DealtTo(GROUP, 0) == 0);
  CHECK(damage.Groups().empty());
}

static void GroupsAreOrderedByRecord(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(0x40009, 0, 3);
  damage.Credit(0x40002, 1, 4);
  std::vector<std::pair<AutoMapRecordId, std::vector<int> > > groups = damage.Groups();
  CHECK(groups.size() == 2);
  CHECK(groups[0].first == 0x40002);
  CHECK(groups[1].first == 0x40009);
  CHECK(groups[0].second.size() == 4);
  CHECK(groups[1].second.size() == 4);
  CHECK(groups[0].second[1] == 4);
}

static void RemovingFromAnUnknownGroupChangesNothing(void)
{
  AutoMapMonsterDamage damage;
  damage.RemoveMember(GROUP, 0);
  CHECK(damage.Groups().empty());
}

static void ForgetClearsEveryGroup(void)
{
  AutoMapMonsterDamage damage;
  damage.Credit(GROUP, 0, 8);
  damage.Forget();
  CHECK(damage.Groups().empty());
}

void TestAutoMapMonsterDamage(void)
{
  CreditAddsUp();
  NothingIsCreditedForNoDamage();
  DamageStopsAtTheMost();
  RemovedMemberShiftsTheOthersDown();
  RemovingTheLastDamagedMemberDropsTheGroup();
  ANewGroupInAFreedRecordStartsClean();
  GroupsAreOrderedByRecord();
  RemovingFromAnUnknownGroupChangesNothing();
  ForgetClearsEveryGroup();
}
