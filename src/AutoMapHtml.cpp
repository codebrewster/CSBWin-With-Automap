// AutoMapHtml.cpp : see AutoMapHtml.h
#include "AutoMapHtml.h"

#include <cstring>

#include "miniz/miniz.h"

#include "AutoMapExportPage.inc"

std::string AutoMapHtml_Gzip(const std::string &bytes)
{
  // A raw deflate stream (no zlib header): negative window bits
  mz_uint flags = tdefl_create_comp_flags_from_zip_params(MZ_BEST_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY);
  size_t deflatedSize = 0;
  void *deflated = tdefl_compress_mem_to_heap(bytes.data(), bytes.size(), &deflatedSize, (int)flags);
  if (deflated == NULL && !bytes.empty()) return std::string();
  // Header: magic, deflate, no flags, no time, no extra flags, unknown system
  static const unsigned char HEADER[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff};
  std::string out((const char *)HEADER, sizeof(HEADER));
  if (deflated != NULL) out.append((const char *)deflated, deflatedSize);
  else out.append("\x03\x00", 2); // an empty final block
  mz_free(deflated);
  mz_ulong crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char *)bytes.data(), bytes.size());
  uint32_t size = (uint32_t)bytes.size();
  for (int i = 0; i < 4; i++) out.push_back((char)((crc >> (8 * i)) & 0xff));
  for (int i = 0; i < 4; i++) out.push_back((char)((size >> (8 * i)) & 0xff));
  return out;
}

std::string AutoMapHtml_Base64(const std::string &bytes)
{
  static const char DIGITS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3)
  {
    uint32_t v = ((unsigned char)bytes[i] << 16) | ((unsigned char)bytes[i + 1] << 8) | (unsigned char)bytes[i + 2];
    out.push_back(DIGITS[(v >> 18) & 63]);
    out.push_back(DIGITS[(v >> 12) & 63]);
    out.push_back(DIGITS[(v >> 6) & 63]);
    out.push_back(DIGITS[v & 63]);
  }
  if (i < bytes.size())
  {
    bool two = i + 1 < bytes.size();
    uint32_t v = ((unsigned char)bytes[i] << 16) | (two ? (unsigned char)bytes[i + 1] << 8 : 0);
    out.push_back(DIGITS[(v >> 18) & 63]);
    out.push_back(DIGITS[(v >> 12) & 63]);
    out.push_back(two ? DIGITS[(v >> 6) & 63] : '=');
    out.push_back('=');
  }
  return out;
}

std::string AutoMapHtml_Png(const unsigned char *rgba, int w, int h)
{
  if (w <= 0 || h <= 0) return std::string();
  size_t size = 0;
  void *png = tdefl_write_image_to_png_file_in_memory_ex(rgba, w, h, 4, &size, MZ_BEST_COMPRESSION, MZ_FALSE);
  if (png == NULL) return std::string();
  std::string out((const char *)png, size);
  mz_free(png);
  return out;
}

std::string AutoMapHtml_Escape(const std::string &text)
{
  std::string out;
  for (size_t i = 0; i < text.size(); i++)
  {
    switch (text[i])
    {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    default: out += text[i]; break;
    }
  }
  return out;
}

std::vector<std::pair<int, int> > AutoMapHtml_Views(int maxVision, int maxSpoilers)
{
  std::vector<std::pair<int, int> > views;
  if (maxVision < 0) maxVision = 0;
  if (maxVision > 2) maxVision = 2;
  if (maxSpoilers < 0) maxSpoilers = 0;
  if (maxSpoilers > 2) maxSpoilers = 2;
  for (int vision = 0; vision <= maxVision; vision++)
    for (int scene = 2 - maxSpoilers; scene <= 2; scene++) views.push_back(std::make_pair(vision, scene));
  return views;
}

static void ReplaceAll(std::string *text, const std::string &what, const std::string &with)
{
  for (size_t at = text->find(what); at != std::string::npos; at = text->find(what, at + with.size()))
    text->replace(at, what.size(), with);
}

std::string AutoMapHtml_Page(const std::string &title, const std::string &subtitle, const std::string &dataScript)
{
  std::string head = EXPORT_PAGE_HEAD;
  ReplaceAll(&head, "@SUBTITLE@", AutoMapHtml_Escape(subtitle));
  ReplaceAll(&head, "@TITLE@", AutoMapHtml_Escape(title));
  // "</script" inside the data would end its script element; JSON text never needs it raw
  std::string data = dataScript;
  ReplaceAll(&data, "</", "<\\/");
  return head + "<script id=\"data\">\n" + data + "\n</script>\n<script>\n" + EXPORT_PAGE_CODE + "\n</script>\n</body></html>\n";
}
