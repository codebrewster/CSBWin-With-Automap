#include "TestSupport.h"
#include "../AutoMapHtml.h"
#include "../miniz/miniz.h"

#include <string>
#include <vector>

static uint32_t Le32(const std::string &s, size_t at)
{
  uint32_t v = 0;
  for (int i = 3; i >= 0; i--) v = (v << 8) | (unsigned char)s[at + i];
  return v;
}

// Undoes AutoMapHtml_Gzip with miniz's inflater, checking the header and the trailer
static bool Gunzip(const std::string &gz, std::string *out)
{
  if (gz.size() < 18 || (unsigned char)gz[0] != 0x1f || (unsigned char)gz[1] != 0x8b || gz[2] != 8 || gz[3] != 0) return false;
  size_t size = 0;
  void *raw = tinfl_decompress_mem_to_heap(gz.data() + 10, gz.size() - 18, &size, 0);
  if (raw == NULL && size != 0) return false;
  out->assign(raw ? (const char *)raw : "", size);
  mz_free(raw);
  uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, (const unsigned char *)out->data(), out->size());
  return Le32(gz, gz.size() - 8) == crc && Le32(gz, gz.size() - 4) == (uint32_t)out->size();
}

static void GzipRoundTrips(void)
{
  std::string json = "{\"cells\":[";
  for (int i = 0; i < 3000; i++) json += "{\"x\":" + std::to_string(i % 40) + ",\"k\":1},";
  json += "{}]}";
  std::string gz = AutoMapHtml_Gzip(json);
  CHECK(gz.size() < json.size() / 5);
  std::string back;
  CHECK(Gunzip(gz, &back));
  CHECK(back == json);
  std::string empty;
  CHECK(Gunzip(AutoMapHtml_Gzip(std::string()), &empty));
  CHECK(empty.empty());
}

static void Base64KnownValues(void)
{
  CHECK(AutoMapHtml_Base64("") == "");
  CHECK(AutoMapHtml_Base64("f") == "Zg==");
  CHECK(AutoMapHtml_Base64("fo") == "Zm8=");
  CHECK(AutoMapHtml_Base64("foo") == "Zm9v");
  CHECK(AutoMapHtml_Base64("foobar") == "Zm9vYmFy");
  CHECK(AutoMapHtml_Base64(std::string("\xff\x00\xfe", 3)) == "/wD+");
}

static void PngHasItsSignatureAndSize(void)
{
  std::vector<unsigned char> rgba(3 * 2 * 4, 0x80);
  std::string png = AutoMapHtml_Png(rgba.data(), 3, 2);
  CHECK(png.size() > 33);
  CHECK(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0);
  // The IHDR chunk: width and height, highest byte first
  CHECK(png.compare(12, 4, "IHDR") == 0);
  CHECK((unsigned char)png[19] == 3 && (unsigned char)png[23] == 2);
  CHECK(AutoMapHtml_Png(rgba.data(), 0, 2).empty());
}

static void ViewsStopAtTheChosenLimits(void)
{
  std::vector<std::pair<int, int> > all = AutoMapHtml_Views(2, 2);
  CHECK(all.size() == 9);
  // Spoilers up to seen keeps only the scene's Seen (2), fog only vision 0
  std::vector<std::pair<int, int> > least = AutoMapHtml_Views(0, 0);
  CHECK(least.size() == 1);
  CHECK(least[0].first == 0 && least[0].second == 2);
  // Up to learned: the scene's Learned (1) and Seen (2), never All (0)
  std::vector<std::pair<int, int> > learned = AutoMapHtml_Views(1, 1);
  CHECK(learned.size() == 4);
  bool anyAll = false;
  for (size_t i = 0; i < learned.size(); i++) anyAll = anyAll || learned[i].second == 0 || learned[i].first == 2;
  CHECK(!anyAll);
}

static void PageCarriesTitleDataAndCode(void)
{
  std::string page = AutoMapHtml_Page("Map of <csb> & co", "Exported", "window.MAPDATA={\"t\":\"</script>\"};");
  CHECK(page.compare(0, 15, "<!doctype html>") == 0);
  CHECK(page.find("<title>Map of &lt;csb&gt; &amp; co</title>") != std::string::npos);
  CHECK(page.find("@TITLE@") == std::string::npos);
  CHECK(page.find("@SUBTITLE@") == std::string::npos);
  // The data cannot close its own script element
  CHECK(page.find("\"<\\/script>\"") != std::string::npos);
  CHECK(page.find("<script id=\"data\">") < page.find("function viewOf"));
  CHECK(page.find("</body></html>") != std::string::npos);
}

void TestAutoMapHtml(void)
{
  GzipRoundTrips();
  Base64KnownValues();
  PngHasItsSignatureAndSize();
  ViewsStopAtTheChosenLimits();
  PageCarriesTitleDataAndCode();
}
