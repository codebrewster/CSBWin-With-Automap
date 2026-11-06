// SmartDiscard as it was in CSBwin up to 11.051.
//
// SmartDiscard runs on every clock tick and, among other things, draws
// random numbers.  Version 11.052 rewrote it, so a playfile recorded with
// an older version only replays in step when the old algorithm runs.  This
// file is that old algorithm, taken from the CSBwin 11.014 source, and it
// is used only while such a playfile is replayed (legacySmartDiscard).

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

RN FindFirstObject(i32 level, i32 x, i32 y);
extern bool legacySmartDiscardSlow;

namespace
{

struct LegacySDEntry
{
  i32   minimum;
  float level;
  float y_intercept;
  float slope;
};

// The built-in defaults of the old versions (CSBUI.cpp SmartDiscards[]).
// Each entry is  name  minimum  level  y-intercept  slope.
struct LegacySDDefault
{
  const char *name;
  i32   minimum;
  float level;
  float y_intercept;
  float slope;
};

const LegacySDDefault legacyDefaults[] =
{
  {"DEFAULT",       30, 50, 20.0f, 1.0f},
  {"WOODEN SHIELD", 10, 50, 20.0f, 1.0f},
  {"ROCK",          10, 50, 20.0f, 1.0f},
  {"SWORD",         10, 50, 20.0f, 1.0f},
  {"DAGGER",        10, 50, 20.0f, 1.0f},
  {"FALCHION",      10, 50, 20.0f, 1.0f},
  {"CLUB",          10, 50, 20.0f, 1.0f},
  {"STONE CLUB",    10, 50, 20.0f, 1.0f},
  {"ARMET",         10, 50, 20.0f, 1.0f},
  {"TORSO PLATE",   10, 50, 20.0f, 1.0f},
  {"LEG PLATE",     10, 50, 20.0f, 1.0f},
  {"FOOT PLATE",    10, 50, 20.0f, 1.0f},
  {"BOULDER",       20, 50, 70.0f, 1.0f},
  {"DRAGON STEAK", 150,  0, 20.0f, 0.0f},
};

struct LegacyInterim
{
  const char *name;
  i32   num;
};

const LegacyInterim interimTable[] =
{
  {"DRAGON STEAK" , 0xaf},
  {"BOULDER"      , 0x80},
  {"FOOT PLATE"   , 0x74},
  {"LEG PLATE"    , 0x5d},
  {"TORSO PLATE"  , 0x4d},
  {"ARMET"        , 0x64},
  {"STONE CLUB"   , 0x30},
  {"CLUB"         , 0x2f},
  {"DAGGER"       , 0x20},
  {"SWORD"        , 0x22},
  {"ROCK"         , 0x36},
  {"WOODEN SHIELD", 0x6c},
  {"FALCHION"     , 0x21}
};

LegacySDEntry sdTable[199];

i32 currentDBNum;
i32 currentLevel;
i32 currentX;
i32 currentY;
i32 currentIndex;
i32 discardFailure;//consecutive discards that failed
enum {
  ModeNotActive,
  ModeNewDB,  //currentDB is the new DB to process
  ModeCount,  //count number of each type of object.
  ModeScan,   //scan dungeon for objects; put in priority-sorted list
  ModeDiscard //discard objects based on priority
} currentMode = ModeNotActive;

i32 objcount[180]; //how many of each exists.
i32 totalCount;

FILE *m_trace;

class SDOBJ
{
  i32 m_object;
public:
  void Get(i32 *level, i32 *x, i32 *y, i32 *objIndex);
  SDOBJ(void) {m_object = 0;};
  SDOBJ(i32 level, i32 x, i32 y, i32 objIndex);
};

SDOBJ::SDOBJ(i32 level, i32 x, i32 y, i32 objIndex)
{
  m_object = (((((level<<6)+x)<<6)+y)<<8)+objIndex;
}

void SDOBJ::Get(i32 *level, i32 *x, i32 *y, i32 *objIndex)
{
  *level    = (m_object >> 20) & 0x3f;
  *x        = (m_object >> 14) & 0x3f;
  *y        = (m_object >>  8) & 0x3f;
  *objIndex = (m_object >>  0) & 0xff;
}

float ComputePriority(SDOBJ *pSDobj)
{
  i32 level, x, y, objIndex;
  LegacySDEntry *psdEnt;
  float priority;
  pSDobj->Get(&level, &x, &y, &objIndex);
  psdEnt = &sdTable[objIndex];
  // 11.014 wrote  priority += float(STRandom(100000))/100000;  The old
  // Windows builds computed that as y_intercept + r * 1e-5f in x87 precision
  // and rounded the sum to float once.  Doing the same here reproduces their
  // priorities to the last bit (checked against an 11.022 trace), so the
  // heap orders ties the same way and draws the same random numbers.
  priority = (float)((double)psdEnt->y_intercept
                     + (double)STRandom(100000) * (double)1e-5f);
  if (m_trace)
  {
    fprintf(m_trace,"ComputePriority for SmartDiscard priority = %f",
             priority);
  };
  if (abs(level - d.partyLevel) > 1)
  {
    priority += psdEnt->level;
    if (m_trace)
    {
      fprintf(m_trace," add level=%f",priority);
    };
  };
  if (objcount[objIndex] > psdEnt->minimum)
  {
    priority += psdEnt->slope * (objcount[objIndex]-psdEnt->minimum);
    if (m_trace)
    {
      fprintf(m_trace," add slope=%f",priority);
    };
  }
  else
  {
    priority = -1.0f;
    if (m_trace)
    {
      fprintf(m_trace," set to %f",priority);
    };
  };
  if (level == d.partyLevel) priority = -1.0f;
  if (m_trace)
  {
    fprintf(m_trace," party level so %f",priority);
  };
  if (m_trace)
  {
    fprintf(m_trace,"\n");
  };
  return priority;
}

class SDOBJLIST
{
  i32   m_numEntry;
  SDOBJ m_sdobj[1050];  // 11.014 declared 65536 but never stores more than 1050
public:
  SDOBJLIST(void) {m_numEntry = 0;};
  void Clear(void) {m_numEntry = 0;};
  void AddObject(SDOBJ *pSDobj);
  bool DeleteObject(SDOBJ *pSDobj);
  float TopPriority(void) {return ComputePriority(&m_sdobj[0]);};
};

void SDOBJLIST::AddObject(SDOBJ *pSDobj)
{
  float priority, parentPriority;
  i32 index, parent;
  if (m_numEntry > 1049) return; //ignore it.
  priority = ComputePriority(pSDobj);
  index = m_numEntry;
  m_numEntry++;
  while (index != 0)
  {
    parent = (index-1)/2;
    parentPriority = ComputePriority(&m_sdobj[parent]);
    if (parentPriority >= priority) break;
    m_sdobj[index] = m_sdobj[parent];
    index = parent;
  };
  m_sdobj[index] = *pSDobj;
}

bool SDOBJLIST::DeleteObject(SDOBJ *pSDobj)
{ // Deletes the topmost object
  i32 index, c1Index, c2Index, parent;
  float priority, parentPriority, c1Priority, c2Priority;
  ASSERT(m_numEntry >= 0,"numEntry");
  if (m_numEntry == 0) return false;
  *pSDobj = m_sdobj[0];
  index = 0; // entry to be replaced.
  while (index < m_numEntry)
  {
    c1Index = 2*index+1;
    c2Index = c1Index+1;
    if (c1Index >= m_numEntry) break;
    if (c2Index >= m_numEntry)
    {
      m_sdobj[index] = m_sdobj[c1Index];
      m_numEntry--;
      return true;
    };
    c1Priority = ComputePriority(&m_sdobj[c1Index]);
    c2Priority = ComputePriority(&m_sdobj[c2Index]);
    if (c1Priority > c2Priority)
    {
      m_sdobj[index] = m_sdobj[c1Index];
      index = c1Index;
    }
    else
    {
      m_sdobj[index] = m_sdobj[c2Index];
      index = c2Index;
    };
  };
  m_numEntry--;
  if (index == m_numEntry) return true;
  priority = ComputePriority(&m_sdobj[m_numEntry]);
  while (index != 0)
  {
    parent = (index-1)/2;
    parentPriority = ComputePriority(&m_sdobj[parent]);
    if (parentPriority >= priority) break;
    m_sdobj[index] = m_sdobj[parent];
    index = parent;
  };
  m_sdobj[index] = m_sdobj[m_numEntry];
  return true;
}

SDOBJLIST objlist;

int ScanDungeon(void)
{
  //Return value is number of objects examined.
  //Return value is 99999 if scan is complete.
  i32 result;
  i32 numLevel, levelWidth, levelHeight;
  RN obj;
  SDOBJ sdobj;
  DB3  *pActuatorDB;
  DB5  *pWeaponDB;
  DB6  *pClothingDB;
  DB10 *pMiscDB;
  result = 0;
  numLevel = d.dungeonDatIndex->NumLevel();
  if (currentLevel >= numLevel)
  {
    currentMode = ModeDiscard;
    discardFailure = 0;
    if (m_trace!=NULL)
    {
      fprintf(m_trace,"switch to Discard Mode\n");
    };
    return 99999;
  };
  levelWidth = d.pLevelDescriptors[currentLevel].LastColumn() + 1;
  if (currentX >= levelWidth)
  {
    currentX = 0;
    currentLevel++;
    if (m_trace!=NULL)fprintf(m_trace,"set level to %02x\n",currentLevel);
    return 0;
  };
  levelHeight = d.pLevelDescriptors[currentLevel].LastRow() + 1;
  if (currentY >= levelHeight)
  {
    currentX++;
    currentY = 0;
    if (m_trace!=NULL)fprintf(m_trace,"set X to %02x\n",currentX);
    return 0;
  };
  for (obj = FindFirstObject(currentLevel, currentX, currentY);
       obj != RNeof;
       obj = GetDBRecordLink(obj))
  {
    result++;
    if (obj.dbType() == dbACTUATOR)
    {
      pActuatorDB = GetRecordAddressDB3(obj);
      if (pActuatorDB->actuatorType() != 0) break;
    };
    if (obj.dbNum() != currentDBNum) continue;
    switch (currentDBNum)
    {
    case dbCLOTHING:
      pClothingDB = GetRecordAddressDB6(obj);
      if (m_trace!=NULL)
      {
        fprintf(m_trace,"scan clothing item \n");
      };
      if (pClothingDB->important()) break;
      sdobj = SDOBJ(currentLevel,
                    currentX,
                    currentY,
                    pClothingDB->clothingType() + 69);
      objlist.AddObject(&sdobj);
      break;
    case dbWEAPON:
      pWeaponDB = GetRecordAddressDB5(obj);
      if (m_trace!=NULL)
      {
        fprintf(m_trace,"scan weapon item \n");
      };
      if (pWeaponDB->important()) break;
      sdobj = SDOBJ(currentLevel,
                    currentX,
                    currentY,
                    pWeaponDB->weaponType() + 23);
      objlist.AddObject(&sdobj);
      break;
    case dbMISC:
      pMiscDB = GetRecordAddressDB10(obj);
      if (m_trace!=NULL)
      {
        fprintf(m_trace,"scan misc item \n");
      };
      if (pMiscDB->important()) break;
      sdobj = SDOBJ(currentLevel,
                    currentX,
                    currentY,
                    pMiscDB->miscType() + 127);
      objlist.AddObject(&sdobj);
      break;
    };
  };
  currentY++;
  if (m_trace!=NULL)fprintf(m_trace,"SmartDiscard set Y to %02x\n",currentY);
  return result;
}

void DiscardObject(void)
{
  float priority;
  i32 level, x, y, objIndex;
  i32 index, oldLevel;
  SDOBJ sdobj;
  RN obj;
  if (!objlist.DeleteObject(&sdobj))
  {
    currentMode = ModeNewDB;
    return;
  };
  priority = ComputePriority(&sdobj);
  if (   (priority < 0)
      || (priority < objlist.TopPriority()))
  {
    discardFailure++;
    objlist.AddObject(&sdobj);
    if (discardFailure > 500) currentMode = ModeNewDB;
    return;
  };
  sdobj.Get(&level, &x, &y, &objIndex);
  for (obj = FindFirstObject(level, x, y);
       obj != RNeof;
       obj = GetDBRecordLink(obj))
  {
    if (obj.dbNum() != currentDBNum) continue;
    switch(currentDBNum)
    {
    case dbMISC:
      index = GetRecordAddressDB10(obj)->miscType() + 127;
      break;
    case dbWEAPON:
      index = GetRecordAddressDB5(obj)->weaponType() + 23;
      break;
    case dbCLOTHING:
      index = GetRecordAddressDB6(obj)->clothingType() + 69;
      break;
    default:
      UI_MessageBox("Unknown DB type in SD",
                    "Warning",
                    MESSAGE_OK | MESSAGE_ICONWARNING);
      currentMode = ModeNotActive;
      return;
    };
    if (objIndex != index) continue;
  // It appears we can discard this object.
    oldLevel = d.LoadedLevel;
    LoadLevel(level);
    MoveObject(obj, x, y, -1, 0, NULL, NULL);
    DeleteDBEntry(db.GetCommonAddress(obj));
    LoadLevel(oldLevel);
    discardFailure = 0;
    totalCount--;
    return;
  };
  discardFailure++;
  totalCount--;
  return;
}

// BuildSmartDiscardTable of the old versions, fed with the built-in
// defaults.  The old list was built by prepending, so it was walked from
// the last entry to the first and DEFAULT came last.
void BuildTable(void)
{
  i32 n, i, j, oType;
  for (i=0; i<199; i++) sdTable[i].minimum = -1;
  for (n = (i32)(sizeof(legacyDefaults)/sizeof(legacyDefaults[0])) - 1; n >= 0; n--)
  {
    const LegacySDDefault *psd = &legacyDefaults[n];
    for (i=0; i<(i32)(sizeof(interimTable)/sizeof(interimTable[0])); i++)
    {
      if (strcmp(psd->name, interimTable[i].name) == 0)
      {
        oType = interimTable[i].num;
        for (j=0; j<180; j++)
        {
          if (d.ObjDesc[j].objectType() == oType) break;
        };
        if (j==180) j=0; //scroll...we don't use it
        sdTable[j].minimum     = psd->minimum;
        sdTable[j].level       = psd->level;
        sdTable[j].y_intercept = psd->y_intercept;
        sdTable[j].slope       = psd->slope;
        break;
      };
    };
    if (i < (i32)(sizeof(interimTable)/sizeof(interimTable[0]))) continue;
    // DEFAULT
    for (i=0; i<199; i++)
    {
      if (sdTable[i].minimum == -1)
      {
        sdTable[i].minimum     = psd->minimum;
        sdTable[i].level       = psd->level;
        sdTable[i].y_intercept = psd->y_intercept;
        sdTable[i].slope       = psd->slope;
      };
    };
  };
}

} // namespace

void SmartDiscardLegacyTrace(FILE *f)
{
  m_trace = f;
}

void SmartDiscardLegacy(bool Initialize)
{
  i32 i;
  i32 repeat;
  DBTYPE newType;
  DBCOMMON *pCommon;
  i32 objIndex;
  if (m_trace!=NULL)
  {
    fprintf(GETFILE(TraceFile),"SmartDiscard ");
  };
  if (Initialize)
  {
    if (m_trace!=NULL)
    {
      fprintf(m_trace,"Initialize\n");
    };
    BuildTable();
    currentDBNum = 10;
    currentMode = ModeNewDB;
    return;
  };
  switch (currentMode)
  {
  case ModeNotActive:
    if (m_trace!=NULL) fprintf(m_trace,"not active\n");
    if (TimerTraceActive)
    {
      fprintf(GETFILE(TraceFile),"SmartDiscard - Not Active\n");
    };
    return;
  case ModeNewDB:
    if (TimerTraceActive)
    {
      fprintf(GETFILE(TraceFile),"SmartDiscard - NewDB\n");
    };
    for (i=currentDBNum+1; i<16; i++)
    {
      if (i == currentDBNum) break;
    };
    if (i > 15)
    {
      switch (currentDBNum)
      {
      case dbMISC:     newType = dbWEAPON;   break;
      case dbWEAPON:   newType = dbCLOTHING; break;
      case dbCLOTHING: newType = dbMISC;     break;
      default: UI_MessageBox("Illegal DB",
                             "Warning",
                             MESSAGE_OK | MESSAGE_ICONWARNING);
               currentMode = ModeNotActive;
               return;
      };
      for (i=0; i<16; i++)
      {
        if (i == newType) break;
      };
    };
    currentDBNum = i;
    if (m_trace!=NULL) fprintf(m_trace,"set DB to %d\n",currentDBNum);
    memset(objcount, 0, 180 * sizeof (i32));
    totalCount = 0;
    currentIndex = 0;
    currentMode = ModeCount;
    return;
  case ModeCount:
    if (TimerTraceActive)
    {
      fprintf(GETFILE(TraceFile),"SmartDiscard - Mode Count\n");
    };
    // Before 9.7 one object per tick
    for (repeat=0; repeat<(legacySmartDiscardSlow ? 1 : 50); repeat++)
    {
      if (currentIndex >= db.NumEntry(currentDBNum))
      { // We have finished counting
        currentLevel = 0;
        currentX = 0;
        currentY = 0;
        objlist.Clear(); // Clear list of prioritized objects
        currentMode = ModeScan;
        if (m_trace!=0)
        {
          fprintf(m_trace,"switch to Scan Mode\n");
        };
        return;
      };
      pCommon = db.GetCommonAddress(DBTYPE(currentDBNum),currentIndex);
      currentIndex++;
      // The old code ends this tick's batch at the first free entry.
      if (pCommon->link() == RNnul) return;
      switch (currentDBNum)
      {
      case dbMISC:
        objIndex = pCommon->CastToDB10()->miscType() + 127;
        if (m_trace!=NULL)fprintf(m_trace,"count MISC ");
        break;
      case dbWEAPON:
        objIndex = pCommon->CastToDB5()->weaponType() + 23;
        if (m_trace!=NULL)fprintf(m_trace,"count WEAPON ");
        break;
      case dbCLOTHING:
        objIndex = pCommon->CastToDB6()->clothingType() + 69;
        if (m_trace!=NULL)fprintf(m_trace,"count CLOTHING ");
        break;
      default:
        UI_MessageBox("SmartDiscard Unknown DB Type",
                      "Warning",
                      MESSAGE_OK | MESSAGE_ICONWARNING);
        currentMode = ModeNotActive;
        return;
      };
      ASSERT(objIndex < 180,"objIndex");
      objcount[objIndex]++;
      totalCount++;
      if (m_trace!=NULL)
      {
        fprintf(m_trace,"index %02x count = %04x total=%04x\n",
                      objIndex, objcount[objIndex], totalCount);
      };
    }; //repeat
    return;
  case ModeScan:
    if (TimerTraceActive)
    {
      fprintf(GETFILE(TraceFile),"SmartDiscard - ModeScan\n");
    };
    // Before 9.7 one cell per tick
    if (legacySmartDiscardSlow) ScanDungeon();
    else for (repeat=50; repeat>0; repeat-=ScanDungeon());
    return;
  case ModeDiscard:
    if (TimerTraceActive)
    {
      fprintf(GETFILE(TraceFile),"SmartDiscard - ModeDiscard\n");
    };
    if (totalCount < (legacySmartDiscardSlow ? 950 : ((currentDBNum==dbMISC)?32000:7000)))
    {
      currentMode = ModeNewDB;
      if (m_trace!=NULL)
      {
        fprintf(m_trace, legacySmartDiscardSlow ? "fewer than 950. Set mode to NewDB\n"
                                                : "fewer than 7000. Set mode to NewDB\n");
      };
      return;
    };
    DiscardObject();
    if (m_trace!=NULL) fprintf(m_trace,"Discard an object\n");
    return;
  };
  UI_MessageBox("Unknown state in SmartDiscard",
                "Warning",
                MESSAGE_OK | MESSAGE_ICONWARNING);
  currentMode = ModeNotActive;
}
