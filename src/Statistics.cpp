#include "stdafx.h"

#include "UI.h"

#include <stdio.h>

//#include "Objects.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"
//#include "UI.h"


#define NUM_MONSTER_TYPE 27

const char *monsterNames[27] =
{
  "Scorpion",
  "SlimeDevil",
  "Giggler",
  "FlyingEye",
  "PainRat",
  "Ruster",
  "Screamer",
  "RockPile",
  "Rive",
  "StoneGolem",
  "Mummy",
  "BlackFlame",
  "Skeleton",
  "Couatl",
  "Vexirk",
  "Worm",
  "Trolin",
  "Wasp",
  "DethKnight",
  "Zytaz",
  "WaterElemental",
  "Oitu",
  "Demon",
  "LordChaos",
  "Dragon",
  "25",
  "GreyLord"
};


// The monster names for the Statistics window, as a player reads them
static const char *const monsterDisplayNames[NUM_MONSTER_TYPE] =
{
  "Scorpion", "Slime Devil", "Giggler", "Flying Eye", "Pain Rat", "Ruster", "Screamer",
  "Rock Pile", "Rive", "Stone Golem", "Mummy", "Black Flame", "Skeleton", "Couatl", "Vexirk",
  "Worm", "Trolin", "Wasp", "Deth Knight", "Zytaz", "Water Elemental", "Oitu", "Demon",
  "Lord Chaos", "Dragon", "Lord Order", "Grey Lord"
};

// The dungeon's own name of the graphic variant of a monster type, from the expool database.
// False when the dungeon names none for it.
static bool FindCustomMonsterName(int monsterType, int graphic, char *name)
{
  // See if name is in expool database.
  ui32 key, *pRecord;
  char *pText;
  int col, size;
  key = (EDT_Database<<24) | (EDBT_MonsterNames<<16) | monsterType;
  size = expool.Locate(key, &pRecord);
  size = 4*size;
  pText = (char *)pRecord;
  for (col = 0; graphic>=0; graphic--)
  {
    for (; col<size; col++)
    {
      // Search for a non-special character
      if (pText[col] == 0)
      {
        col = size;  // no find
        break;
      };
      if (pText[col] == '|')
      {
        col++;  // Start of next monster's name
        break;
      };
      if (graphic == 0)
      {
        // Evidently, we have found a name that is needed.
        pText += col;
        size -= col;
        for (col=0; col<size; col++)
        {
          // Count the characters in the name
          if ((pText[col]==0) || (pText[col]=='|')) break;
        };
        //return CString(pText,col);
        memcpy(name, pText, col);
        name[col] = 0;
        return true;
      };
    };
  };
  return false;
}

void GetMonsterName(int monsterType, int graphic, char *name)
{
  if (FindCustomMonsterName(monsterType, graphic, name)) return;
  sprintf(name, "%s(%d)", monsterNames[monsterType], graphic);
};

// The dungeon's name of the variant, else the type's name with no variant number: the variants
// a dungeon does not name look alike to the player
void GetMonsterDisplayName(int monsterType, int graphic, char *name)
{
  if (FindCustomMonsterName(monsterType, graphic, name)) return;
  if (monsterType >= 0 && monsterType < NUM_MONSTER_TYPE) strcpy(name, monsterDisplayNames[monsterType]);
  else sprintf(name, "Monster #%d", monsterType);
}


void Statistics(void)
{
  int i, j, why, total, len, maxLen;
  unsigned int *pRecord;
  unsigned int key;
  LISTING list;
  char line[80];
  unsigned int stats[NUM_MONSTER_TYPE][4][DMW_numWhy];
  list.Clear();
  for (i=0; i<NUM_MONSTER_TYPE; i++)
  {
    for (j=0; j<4; j++)
    {
      for (why=0; why<DMW_numWhy; why++)
      {
        stats[i][j][why] = 0;
      }
    };
  };
  for (i=0; i<NUM_MONSTER_TYPE; i++)
  {
    for (why=0; why<DMW_numWhy; why++)
    {
      key = (EDT_Statistics << 24) + (ESTAT_NumMonsterKilled << 16) + (why << 8) + i;
      len = expool.Locate(key, &pRecord);
      if (len >=4)
      {
        for (j=0; j<4; j++)
        {
          stats[i][j][why] = pRecord[j];
        };
      };
    };
  };
  maxLen = 0;
  for (i=0; i<NUM_MONSTER_TYPE; i++)
  {
    for (j=0; j<4; j++)
    {
      total = 0;
      for (why=0; why<DMW_numWhy; why++)
      {
        total += stats[i][j][why];
      };
      if (total != 0)
      {

        char mName[80];
        GetMonsterName(i, j, mName);
        len = (i32)strlen(mName);
        if (len > maxLen) maxLen = len;
      };
    }
  };
  memset(line,' ',maxLen);
  strcpy(line+maxLen,"   Total    ??? Fusion  Space Damage  Move1  Move2");
  list.AddLine(line);
  for (i=0; i<NUM_MONSTER_TYPE; i++)
  {
    for (j=0; j<4; j++)
    {
      total = 0;
      for (why=0; why<DMW_numWhy; why++)
      {
        total += stats[i][j][why];
      };
      if (total != 0)
      {
        char mName[80];
        memset(mName,' ',80);
        GetMonsterName(i,j,mName);
        len = (i32)strlen(mName);
        mName[len] = ' ';
        mName[maxLen] = 0;
        sprintf(line, "%s", mName);
        sprintf(line+strlen(line), "%7d",
                           total);
        for (why=0; why<DMW_numWhy; why++)
        {
          sprintf(line+strlen(line), "%7d",
                           stats[i][j][why]);
        };
        list.AddLine(line);
      };
    };
  };
  list.DisplayList("Statistics");
}
