#include "TestSupport.h"
#include "../AutoMapExportChoice.h"

#include <set>
#include <string>
#include <vector>

static std::vector<bool> FirstThreeVisited(void)
{
  std::vector<bool> visited(6, false);
  visited[0] = visited[1] = visited[2] = true;
  return visited;
}

static void LevelsFollowTheChoice(void)
{
  std::vector<bool> visited = FirstThreeVisited();
  std::set<int> picked;
  picked.insert(4);
  picked.insert(1);
  CHECK(AutoMapExportChoice_Levels(AutoMapExportThisLevel, 6, 2, visited, picked) == std::vector<int>(1, 2));
  CHECK(AutoMapExportChoice_Levels(AutoMapExportVisited, 6, 2, visited, picked).size() == 3);
  CHECK(AutoMapExportChoice_Levels(AutoMapExportAll, 6, 2, visited, picked).size() == 6);
  std::vector<int> pick = AutoMapExportChoice_Levels(AutoMapExportPick, 6, 2, visited, picked);
  CHECK(pick.size() == 2 && pick[0] == 1 && pick[1] == 4);
  // A pick of a level the dungeon does not have is left out
  picked.insert(9);
  CHECK(AutoMapExportChoice_Levels(AutoMapExportPick, 6, 2, visited, picked).size() == 2);
}

static void PickButtons(void)
{
  CHECK(AutoMapExportChoice_PickAll(4).size() == 4);
  std::set<int> picked;
  picked.insert(0);
  picked.insert(3);
  std::set<int> inverted = AutoMapExportChoice_PickInverted(picked, 5);
  CHECK(inverted.size() == 3 && inverted.count(1) && inverted.count(2) && inverted.count(4));
  CHECK(AutoMapExportChoice_PickInverted(inverted, 5) == picked);
  std::set<int> visited = AutoMapExportChoice_PickVisited(FirstThreeVisited());
  CHECK(visited.size() == 3 && visited.count(0) && visited.count(2) && !visited.count(3));
}

static void PageNames(void)
{
  CHECK(AutoMapExportChoice_PageName("./csb-BeforeWin") == "csb-BeforeWin-map.html");
  CHECK(AutoMapExportChoice_PageName("/home/a/Dungeon Master/csbgame.dat") == "csbgame-map.html");
  CHECK(AutoMapExportChoice_PageName("C:\\Games\\CSB\\csbgame.BAK") == "csbgame-map.html");
  CHECK(AutoMapExportChoice_PageName("") == "csb-map.html");
  CHECK(AutoMapExportChoice_WithHtmlExtension("map") == "map.html");
  CHECK(AutoMapExportChoice_WithHtmlExtension("map.") == "map.html");
  CHECK(AutoMapExportChoice_WithHtmlExtension("map.htm") == "map.htm");
  CHECK(AutoMapExportChoice_WithHtmlExtension(".hidden") == ".hidden.html");
}

static void Paths(void)
{
  std::string folder, name;
  AutoMapExportChoice_SplitPath("/home/a/x.html", &folder, &name);
  CHECK(folder == "/home/a" && name == "x.html");
  AutoMapExportChoice_SplitPath("/x.html", &folder, &name);
  CHECK(folder == "/" && name == "x.html");
  AutoMapExportChoice_SplitPath("x.html", &folder, &name);
  CHECK(folder.empty() && name == "x.html");
  AutoMapExportChoice_SplitPath("C:\\Games\\x.html", &folder, &name);
  CHECK(folder == "C:\\Games" && name == "x.html");
  CHECK(AutoMapExportChoice_Join("/", "x.html") == "/x.html");
  CHECK(AutoMapExportChoice_Join("", "x.html") == "x.html");
}

static void FileUrls(void)
{
  CHECK(AutoMapExportChoice_FileUrl("/home/markus/Games/Dungeon Master/m.html") ==
        "file:///home/markus/Games/Dungeon%20Master/m.html");
  CHECK(AutoMapExportChoice_FileUrl("C:\\Games\\CSB\\m#1.html") == "file:///C:/Games/CSB/m%231.html");
  CHECK(AutoMapExportChoice_FileUrl("/a/\xc3\xa4.html") == "file:///a/%C3%A4.html");
}

static void SizeEstimate(void)
{
  // The measured dungeon: 14 levels and every view come out near 1.07 MB
  long all = AutoMapExportChoice_EstimatedBytes(14, 2, 2);
  CHECK(all > 1000L * 1024 && all < 1200L * 1024);
  CHECK(AutoMapExportChoice_EstimatedBytes(14, 0, 0) < all);
  CHECK(AutoMapExportChoice_EstimatedBytes(0, 2, 2) == 0);
  CHECK(AutoMapExportChoice_SizeText(1024) == "1 KB");
  CHECK(AutoMapExportChoice_SizeText(358400) == "350 KB");
  CHECK(AutoMapExportChoice_SizeText(1572864) == "1.5 MB");
}

static void Typing(void)
{
  CHECK(AutoMapExportChoice_Typed("map", "-1.html") == "map-1.html");
  CHECK(AutoMapExportChoice_Typed("map", "a/b\\c:d*e?f\"g<h>i|j\tk") == "mapabcdefghijk");
  CHECK(AutoMapExportChoice_Typed("", "\xc3\xa4") == "\xc3\xa4");
  CHECK(AutoMapExportChoice_Erased("ma\xc3\xa4") == "ma");
  CHECK(AutoMapExportChoice_Erased("map") == "ma");
  CHECK(AutoMapExportChoice_Erased("") == "");
}

void TestAutoMapExportChoice(void)
{
  LevelsFollowTheChoice();
  PickButtons();
  PageNames();
  Paths();
  FileUrls();
  SizeEstimate();
  Typing();
}
