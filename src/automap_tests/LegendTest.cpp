#include "TestSupport.h"
#include "../AutoMapHtml.h"
#include "../AutoMapLegend.h"
#include "../AutoMapScene.h"

#include <set>
#include <string>
#include <vector>

// The rows of the exported page's LEGEND (docs/automap-mockup/page/ui.js), headings and samples
// alike, in order: every line in it that starts with ['LABEL', is one
static std::vector<std::string> PageLegendLabels(void)
{
  std::string page = AutoMapHtml_Page("t", "s", "");
  std::vector<std::string> labels;
  size_t start = page.find("const LEGEND = [");
  if (start == std::string::npos) return labels;
  size_t end = page.find("\n];", start);
  size_t at = page.find('\n', start);
  while (at != std::string::npos && at < end)
  {
    size_t next = page.find('\n', at + 1);
    std::string line = page.substr(at + 1, (next == std::string::npos ? page.size() : next) - at - 1);
    size_t open = line.find("['");
    if (open != std::string::npos && line.find_first_not_of(' ') == open)
    {
      size_t close = line.find('\'', open + 2);
      labels.push_back(line.substr(open + 2, close - open - 2));
    }
    at = next;
  }
  return labels;
}

static void TheGameAndThePageListTheSameRows(void)
{
  std::vector<std::string> page = PageLegendLabels();
  const std::vector<AutoMapLegendEntry> &game = AutoMapLegend_Entries();
  CHECK(page.size() == game.size());
  for (size_t i = 0; i < page.size() && i < game.size(); i++)
  {
    if (page[i] != game[i].label) std::printf("  row %d: page '%s', game '%s'\n", (int)i, page[i].c_str(), game[i].label);
    CHECK(page[i] == game[i].label);
  }
}

static void EverySampleIsListedOnce(void)
{
  const std::vector<AutoMapLegendEntry> &rows = AutoMapLegend_Entries();
  std::set<int> samples;
  int headings = 0;
  for (size_t i = 0; i < rows.size(); i++)
  {
    if (rows[i].sample == AutoMapLegendHeading) headings++;
    else CHECK(samples.insert(rows[i].sample).second);
  }
  CHECK(rows[0].sample == AutoMapLegendHeading);
  CHECK(headings == 6);
  CHECK((int)samples.size() == AutoMapLegendChosenHit);
}

static void TheLayoutFillsColumnsTopToBottom(void)
{
  const std::vector<AutoMapLegendEntry> &rows = AutoMapLegend_Entries();
  // The page's 3x5 font: 4 pixels a letter but the last
  CHECK(AutoMapScene_TextWidth("ROCK") == 15);
  AutoMapLegendLayout wide = AutoMapLegend_Layout(2000, 2);
  CHECK(wide.columns == AUTOMAP_LEGEND_MOST_COLUMNS);
  CHECK(wide.places.size() == rows.size());
  // CELLS has 31 rows: 11 to a column, the twelfth starts the second
  CHECK(wide.places[0].y == 0 && rows[wide.places[0].entry].sample == AutoMapLegendHeading);
  CHECK(wide.places[1].column == 0 && wide.places[1].y == AUTOMAP_LEGEND_HEADING);
  CHECK(wide.places[12].column == 1 && wide.places[12].y == AUTOMAP_LEGEND_HEADING);
  CHECK(wide.places[11].y == AUTOMAP_LEGEND_HEADING + 10 * AUTOMAP_LEGEND_ROW);
  CHECK(wide.places[32].entry == 32 && rows[32].sample == AutoMapLegendHeading);
  CHECK(wide.places[32].y == AUTOMAP_LEGEND_HEADING + 11 * AUTOMAP_LEGEND_ROW + 6);
  AutoMapLegendLayout phone = AutoMapLegend_Layout(300, 2);
  CHECK(phone.columns == 1);
  CHECK(phone.height > wide.height);
  AutoMapLegendLayout two = AutoMapLegend_Layout(2 * wide.columnWidth + 20, 2);
  CHECK(two.columns == 2);
}

void TestAutoMapLegend(void)
{
  TheGameAndThePageListTheSameRows();
  EverySampleIsListedOnce();
  TheLayoutFillsColumnsTopToBottom();
}
