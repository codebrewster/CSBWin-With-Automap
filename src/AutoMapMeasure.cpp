// AutoMapMeasure.cpp : measures the wall decorations and floor items the 3D view draws, frame
// by frame and in a debug sweep over a level.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "AutoMapDrawTally.h"
#include "AutoMapLevelReader.h"
#include "AutoMapScene.h"
#include "AutoMapSight.h"
#include "AutoMapMeasure.h"

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

i32 GetPixel(pnt, i16, i16, i16); // Chaos.cpp
void DrawViewport(i32 facing, i32 x, i32 y);

// d.pViewportBMP: 224 x 136 pixels, 112 bytes per line
static const int VIEWPORT_BYTE_WIDTH = 112;
static const int VIEWPORT_WIDTH = 2 * VIEWPORT_BYTE_WIDTH;
static const int VIEWPORT_HEIGHT = 136;
static const int VIEWPORT_BYTES = VIEWPORT_BYTE_WIDTH * VIEWPORT_HEIGHT;

struct DecorationDraw
{
  int drawLocation;
  int wallX;
  int wallY;
  int side;
  int drawn;
};

static bool isRunning = false;
static SUMMARIZEROOMDATA framePositions[AUTOMAP_SIGHT_CELL_COUNT];
static int frameFacing = 0;
static ui8 viewportBefore[VIEWPORT_BYTES];
static bool pendingBefore = false;
static int pendingX1 = 0;
static int pendingX2 = -1;
static int pendingY1 = 0;
static int pendingY2 = -1;
static AutoMapDrawTally tally;
// The blit that wrote each viewport pixel last in this frame, 0 for none
static int writeStamps[VIEWPORT_WIDTH * VIEWPORT_HEIGHT];
static int blitCount = 0;
// The finished frame without its floor items, the background every item of a pile is seen
// against; made on the first item that asks for it in a frame
static std::vector<int> frameWithoutItems;
static bool hasFrameWithoutItems = false;
// The sweep's decoration draws, in the order drawn: where each one was
static std::vector<DecorationDraw> decorationsOfFrame;
static std::vector<AutoMapDrawOnScreen> drawsOfFrame;

bool AutoMapMeasure_IsRunning(void)
{
  return isRunning;
}

void AutoMapMeasure_OnViewportSummarised(const SUMMARIZEROOMDATA *cellsInView)
{
  for (int i = 0; i < AUTOMAP_SIGHT_CELL_COUNT; i++) framePositions[i] = cellsInView[i];
  pendingBefore = false;
  decorationsOfFrame.clear();
  tally.StartFrame();
  std::memset(writeStamps, 0, sizeof(writeStamps));
  blitCount = 0;
  hasFrameWithoutItems = false;
}

const std::vector<AutoMapDrawOnScreen> &AutoMapMeasure_DrawsOfFrame(void)
{
  return drawsOfFrame;
}

static int Clamp(int value, int lowest, int highest)
{
  if (value < lowest) return lowest;
  if (value > highest) return highest;
  return value;
}

void AutoMapMeasure_BeforeBlit(int x1, int x2, int y1, int y2)
{
  pendingX1 = Clamp(x1, 0, VIEWPORT_WIDTH - 1);
  pendingX2 = Clamp(x2, 0, VIEWPORT_WIDTH - 1);
  pendingY1 = Clamp(y1, 0, VIEWPORT_HEIGHT - 1);
  pendingY2 = Clamp(y2, 0, VIEWPORT_HEIGHT - 1);
  // A rectangle clamped to nothing would wrap the copy size below
  if (pendingX2 < pendingX1 || pendingY2 < pendingY1) return;
  // Only the rectangle: a blit changes nothing outside it. The viewport is in bitplanes, 16
  // pixels in four words, so whole groups of 16 are copied: a pixel's colour is in all four.
  int firstByte = (pendingX1 / 16) * 8;
  int endByte = (pendingX2 / 16 + 1) * 8;
  for (int y = pendingY1; y <= pendingY2; y++)
  {
    std::memcpy(viewportBefore + y * VIEWPORT_BYTE_WIDTH + firstByte,
                (const ui8 *)d.pViewportBMP + y * VIEWPORT_BYTE_WIDTH + firstByte,
                (size_t)(endByte - firstByte));
  }
  pendingBefore = true;
}

void AutoMapMeasure_AfterBlit(const AutoMapDrawKey &key, int x1, int x2, int y1, int y2)
{
  if (!pendingBefore) return;
  pendingBefore = false;
  if (Clamp(x1, 0, VIEWPORT_WIDTH - 1) != pendingX1 || Clamp(x2, 0, VIEWPORT_WIDTH - 1) != pendingX2 ||
      Clamp(y1, 0, VIEWPORT_HEIGHT - 1) != pendingY1 || Clamp(y2, 0, VIEWPORT_HEIGHT - 1) != pendingY2)
    return;
  if (key.kind == AutoMapDrawDecoration)
  {
    int cellNumber = 0;
    int summaryIndex = 0;
    if (!AutoMapSight_DecorationPlacement(key.drawLocation, &cellNumber, &summaryIndex)) return;
    DecorationDraw where;
    where.drawLocation = key.drawLocation;
    where.wallX = framePositions[cellNumber].x;
    where.wallY = framePositions[cellNumber].y;
    where.side = AutoMapSight_SideOfSummaryIndex(frameFacing, summaryIndex);
    where.drawn = 0;
    decorationsOfFrame.push_back(where);
  }
  std::vector<AutoMapDrawnPixel> changed;
  for (int y = pendingY1; y <= pendingY2; y++)
  {
    for (int x = pendingX1; x <= pendingX2; x++)
    {
      int behind = GetPixel((pnt)viewportBefore, sw(x), sw(y), VIEWPORT_BYTE_WIDTH);
      int drawn = GetPixel((pnt)d.pViewportBMP, sw(x), sw(y), VIEWPORT_BYTE_WIDTH);
      if (behind == drawn) continue;
      AutoMapDrawnPixel pixel = {x, y, behind, drawn, writeStamps[y * VIEWPORT_WIDTH + x]};
      changed.push_back(pixel);
    }
  }
  if (key.kind == AutoMapDrawDecoration) decorationsOfFrame.back().drawn = (int)changed.size();
  tally.AddDraw(key, changed);
}

void AutoMapMeasure_OnViewportBlit(const unsigned char *src, int x1, int x2, int y1, int y2,
                                   int srcOffsetX, int srcOffsetY, int srcByteWidth,
                                   int transparentColor)
{
  blitCount++;
  for (int y = Clamp(y1, 0, VIEWPORT_HEIGHT); y <= y2 && y < VIEWPORT_HEIGHT; y++)
  {
    for (int x = Clamp(x1, 0, VIEWPORT_WIDTH); x <= x2 && x < VIEWPORT_WIDTH; x++)
    {
      if (transparentColor >= 0)
      {
        int source = GetPixel((pnt)src, sw(srcOffsetX + x - x1), sw(srcOffsetY + y - y1), sw(srcByteWidth));
        if (source == transparentColor) continue;
      }
      writeStamps[y * VIEWPORT_WIDTH + x] = blitCount;
    }
  }
}

static int ViewportPixel(int x, int y)
{
  return GetPixel((pnt)d.pViewportBMP, sw(x), sw(y), VIEWPORT_BYTE_WIDTH);
}

static int WriteStamp(int x, int y)
{
  return writeStamps[y * VIEWPORT_WIDTH + x];
}

void AutoMapMeasure_OnViewportFinished(void)
{
  drawsOfFrame = tally.FinishFrame(ViewportPixel, WriteStamp);
  hasFrameWithoutItems = false;
}

// Gems on a club do not hide one another: the eye sees the pile against the floor
static int BackgroundPixel(int x, int y)
{
  if (!hasFrameWithoutItems)
  {
    frameWithoutItems = tally.FrameWithoutItems(VIEWPORT_WIDTH, VIEWPORT_HEIGHT, ViewportPixel, WriteStamp);
    hasFrameWithoutItems = true;
  }
  return frameWithoutItems[(size_t)(y * VIEWPORT_WIDTH + x)];
}

static bool IsKept(const AutoMapDrawnPixel &pixel)
{
  return ViewportPixel(pixel.x, pixel.y) == pixel.drawn && WriteStamp(pixel.x, pixel.y) == pixel.stamp;
}

// The draw's kept pixels and the frame around them, out to the camouflage radius. A floor item
// is seen against the frame without the floor items, anything else against the frame with what
// its own pixels cover.
struct DrawOnScreen
{
  int originX;
  int originY;
  AutoMapDrawView view;
};

static DrawOnScreen DrawOnScreenOf(const std::vector<AutoMapDrawnPixel> &pixels, const short *palette,
                                   bool isItem)
{
  DrawOnScreen item;
  int x1 = VIEWPORT_WIDTH;
  int x2 = -1;
  int y1 = VIEWPORT_HEIGHT;
  int y2 = -1;
  for (size_t p = 0; p < pixels.size(); p++)
  {
    if (!IsKept(pixels[p])) continue;
    if (pixels[p].x < x1) x1 = pixels[p].x;
    if (pixels[p].x > x2) x2 = pixels[p].x;
    if (pixels[p].y < y1) y1 = pixels[p].y;
    if (pixels[p].y > y2) y2 = pixels[p].y;
  }
  item.originX = Clamp(x1 - AUTOMAP_CAMOUFLAGE_RADIUS, 0, VIEWPORT_WIDTH - 1);
  item.originY = Clamp(y1 - AUTOMAP_CAMOUFLAGE_RADIUS, 0, VIEWPORT_HEIGHT - 1);
  AutoMapDrawView &view = item.view;
  view.width = x2 < 0 ? 0 : Clamp(x2 + AUTOMAP_CAMOUFLAGE_RADIUS, 0, VIEWPORT_WIDTH - 1) - item.originX + 1;
  view.height = y2 < 0 ? 0 : Clamp(y2 + AUTOMAP_CAMOUFLAGE_RADIUS, 0, VIEWPORT_HEIGHT - 1) - item.originY + 1;
  for (int y = 0; y < view.height; y++)
  {
    for (int x = 0; x < view.width; x++)
    {
      view.screen.push_back(palette[ViewportPixel(item.originX + x, item.originY + y) & 15]);
      int background = isItem ? BackgroundPixel(item.originX + x, item.originY + y)
                              : ViewportPixel(item.originX + x, item.originY + y);
      view.background.push_back(palette[background & 15]);
      view.isDrawn.push_back(false);
    }
  }
  for (size_t p = 0; p < pixels.size(); p++)
  {
    const AutoMapDrawnPixel &pixel = pixels[p];
    if (!IsKept(pixel)) continue;
    size_t at = (size_t)((pixel.y - item.originY) * view.width + pixel.x - item.originX);
    view.isDrawn[at] = true;
    if (!isItem) view.background[at] = palette[pixel.behind & 15];
  }
  return item;
}

AutoMapDrawView AutoMapMeasure_ViewOf(const AutoMapDrawKey &key, const short *palette)
{
  const std::vector<AutoMapDrawnPixel> *pixels = tally.PixelsOf(key);
  if (pixels == NULL) return DrawOnScreenOf(std::vector<AutoMapDrawnPixel>(), palette, false).view;
  return DrawOnScreenOf(*pixels, palette, key.kind == AutoMapDrawItem).view;
}

// The picture's colours: the 16 of the palette shown, then the marks
static const int MARK_DRAWN_OVER = 16;
static const int MARK_COUNTS = 17;
static const int MARK_DOES_NOT_COUNT = 18;

static unsigned long Crc32(const std::vector<unsigned char> &bytes, size_t from)
{
  unsigned long crc = 0xffffffffUL;
  for (size_t i = from; i < bytes.size(); i++)
  {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320UL & (0UL - (crc & 1)));
  }
  return crc ^ 0xffffffffUL;
}

static void AppendBigEndian(std::vector<unsigned char> *bytes, unsigned long value)
{
  for (int shift = 24; shift >= 0; shift -= 8) bytes->push_back((unsigned char)((value >> shift) & 0xff));
}

static void WriteChunk(FILE *out, const char *type, const std::vector<unsigned char> &data)
{
  std::vector<unsigned char> chunk;
  AppendBigEndian(&chunk, (unsigned long)data.size());
  chunk.insert(chunk.end(), type, type + 4);
  chunk.insert(chunk.end(), data.begin(), data.end());
  AppendBigEndian(&chunk, Crc32(chunk, 4));
  std::fwrite(&chunk[0], 1, chunk.size(), out);
}

// An indexed PNG. The build has no zlib, so the image data goes into stored deflate blocks:
// one byte a pixel, uncompressed.
static bool WritePng(const char *path, int width, int height, const std::vector<unsigned char> &pixels,
                     const short *palette)
{
  std::vector<unsigned char> raw;
  for (int y = 0; y < height; y++)
  {
    raw.push_back(0);   // no filter
    raw.insert(raw.end(), pixels.begin() + (size_t)y * width, pixels.begin() + (size_t)(y + 1) * width);
  }
  std::vector<unsigned char> zlib;
  zlib.push_back(0x78);
  zlib.push_back(0x01);
  for (size_t at = 0; at < raw.size(); at += 65535)
  {
    size_t length = std::min(raw.size() - at, (size_t)65535);
    zlib.push_back(at + length == raw.size() ? 1 : 0);
    zlib.push_back((unsigned char)(length & 0xff));
    zlib.push_back((unsigned char)(length >> 8));
    zlib.push_back((unsigned char)(~length & 0xff));
    zlib.push_back((unsigned char)((~length >> 8) & 0xff));
    zlib.insert(zlib.end(), raw.begin() + at, raw.begin() + at + length);
  }
  unsigned long low = 1;
  unsigned long high = 0;
  for (size_t i = 0; i < raw.size(); i++)
  {
    low = (low + raw[i]) % 65521;
    high = (high + low) % 65521;
  }
  AppendBigEndian(&zlib, (high << 16) | low);
  std::vector<unsigned char> header;
  AppendBigEndian(&header, (unsigned long)width);
  AppendBigEndian(&header, (unsigned long)height);
  const unsigned char rest[5] = {8, 3, 0, 0, 0};   // 8 bits, indexed
  header.insert(header.end(), rest, rest + 5);
  std::vector<unsigned char> colours;
  for (int i = 0; i < 16; i++)
  {
    // Three bits per channel, as the palette holds them
    colours.push_back((unsigned char)(((palette[i] >> 8) & 7) * 255 / 7));
    colours.push_back((unsigned char)(((palette[i] >> 4) & 7) * 255 / 7));
    colours.push_back((unsigned char)((palette[i] & 7) * 255 / 7));
  }
  const unsigned char marks[9] = {255, 255, 0, 255, 0, 255, 0, 255, 255};
  colours.insert(colours.end(), marks, marks + 9);
  FILE *out = std::fopen(path, "wb");
  if (out == NULL) return false;
  const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::fwrite(signature, 1, 8, out);
  WriteChunk(out, "IHDR", header);
  WriteChunk(out, "PLTE", colours);
  WriteChunk(out, "IDAT", zlib);
  WriteChunk(out, "IEND", std::vector<unsigned char>());
  bool failed = std::ferror(out) != 0;
  if (std::fclose(out) != 0) failed = true;
  return !failed;
}

bool AutoMapMeasure_WritePicture(const char *path, const short *palette, AutoMapDrawKind kind)
{
  int width = 3 * VIEWPORT_WIDTH;
  std::vector<unsigned char> pixels((size_t)(width * VIEWPORT_HEIGHT));
  for (int y = 0; y < VIEWPORT_HEIGHT; y++)
  {
    for (int x = 0; x < VIEWPORT_WIDTH; x++)
    {
      unsigned char shown = (unsigned char)(ViewportPixel(x, y) & 15);
      pixels[(size_t)(y * width + x)] = shown;
      pixels[(size_t)(y * width + x + VIEWPORT_WIDTH)] = shown;
      // A decoration's background is the wall; its own pixels are put back below
      int behind = kind == AutoMapDrawItem ? BackgroundPixel(x, y) : ViewportPixel(x, y);
      pixels[(size_t)(y * width + x + 2 * VIEWPORT_WIDTH)] = (unsigned char)(behind & 15);
    }
  }
  for (size_t i = 0; i < drawsOfFrame.size(); i++)
  {
    AutoMapDrawKind drawKind = drawsOfFrame[i].key.kind;
    // Items in alcoves and doors go into the items' picture, judged against what they cover
    bool isAlcoveItem = kind == AutoMapDrawItem
                        && (drawKind == AutoMapDrawAlcoveItem || drawKind == AutoMapDrawDoor);
    if (drawKind != kind && !isAlcoveItem) continue;
    const std::vector<AutoMapDrawnPixel> *drawn = tally.PixelsOf(drawsOfFrame[i].key);
    if (drawn == NULL) continue;
    for (size_t p = 0; p < drawn->size(); p++)
    {
      const AutoMapDrawnPixel &pixel = (*drawn)[p];
      size_t row = (size_t)(pixel.y * width);
      if (!IsKept(pixel))
      {
        pixels[row + pixel.x + VIEWPORT_WIDTH] = (unsigned char)MARK_DRAWN_OVER;
        continue;
      }
      if (drawKind != AutoMapDrawItem) pixels[row + pixel.x + 2 * VIEWPORT_WIDTH] = (unsigned char)(pixel.behind & 15);
    }
    DrawOnScreen draw = DrawOnScreenOf(*drawn, palette, drawKind == AutoMapDrawItem);
    std::vector<bool> counts;
    AutoMapSight_ShapeOfItem(draw.view, &counts);
    if (kind == AutoMapDrawDecoration)
    {
      // A decoration counts by its contrast as well as by its colours the wall does not show
      std::vector<bool> contrasts;
      AutoMapSight_ContrastingPixels(draw.view, &contrasts);
      for (size_t at = 0; at < counts.size(); at++) counts[at] = counts[at] || contrasts[at];
    }
    for (int y = 0; y < draw.view.height; y++)
    {
      for (int x = 0; x < draw.view.width; x++)
      {
        size_t at = (size_t)(y * draw.view.width + x);
        if (!draw.view.isDrawn[at]) continue;
        pixels[(size_t)((draw.originY + y) * width + draw.originX + x + VIEWPORT_WIDTH)] =
            (unsigned char)(counts[at] ? MARK_COUNTS : MARK_DOES_NOT_COUNT);
      }
    }
  }
  return WritePng(path, width, VIEWPORT_HEIGHT, pixels, palette);
}

static const AutoMapCellRecord *CellOf(const AutoMapLevelSnapshot &snapshot, int x, int y)
{
  int sx = x + snapshot.boundaryMarginX;
  int sy = y + snapshot.boundaryMarginY;
  if (sx < 0 || sy < 0 || sx >= snapshot.width || sy >= snapshot.height) return NULL;
  return &snapshot.cells[(size_t)(sy * snapshot.width + sx)];
}

static bool PartyCanStandOn(const AutoMapCellRecord &cell)
{
  return cell.kind != AutoMapCellWall && cell.kind != AutoMapCellFalseWall && !cell.isOutsideLevel;
}

// The map's own coordinates, as on its rulers: a cell's place in the snapshot (the level
// plus the rock margins the reader adds on the west and north) plus the level's offset
static int GlobalX(const AutoMapLevelSnapshot &snapshot, int x)
{
  return x + snapshot.boundaryMarginX + snapshot.offsetX;
}

static int GlobalY(const AutoMapLevelSnapshot &snapshot, int y)
{
  return y + snapshot.boundaryMarginY + snapshot.offsetY;
}

static const char *const SIDE_NAMES[4] = {"N", "E", "S", "W"};

// The pixels still on screen in the finished frame, as "behind>drawn:count" pairs
static std::string PairsText(const std::vector<AutoMapPixelPair> &kept, int *keptCount)
{
  *keptCount = 0;
  std::string pairs;
  for (size_t k = 0; k < kept.size(); k++)
  {
    char pair[32];
    std::snprintf(pair, sizeof(pair), "%s%d>%d:%d", pairs.empty() ? "" : " ", kept[k].behind,
                  kept[k].drawn, kept[k].count);
    pairs += pair;
    *keptCount += kept[k].count;
  }
  return pairs;
}

// One line per decoration draw: the kept pixels and how many the decoration drew before nearer
// cells covered it
static int WriteFrame(FILE *out, const AutoMapLevelSnapshot &snapshot, int partyX, int partyY)
{
  int written = 0;
  // The tally merges equal keys and a view spot is drawn once per frame, so the decoration
  // draws of the tally and the placements above them are in the same order
  size_t decoration = 0;
  for (size_t i = 0; i < drawsOfFrame.size(); i++)
  {
    if (drawsOfFrame[i].key.kind != AutoMapDrawDecoration) continue;
    if (decoration >= decorationsOfFrame.size()) break;
    const DecorationDraw &draw = decorationsOfFrame[decoration++];
    const std::vector<AutoMapPixelPair> &kept = drawsOfFrame[i].kept;
    int keptCount = 0;
    std::string pairs = PairsText(kept, &keptCount);
    const AutoMapCellRecord *wall = CellOf(snapshot, draw.wallX, draw.wallY);
    const AutoMapWallSide *face = wall == NULL ? NULL : &wall->wallSides[draw.side];
    std::fprintf(out, "%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%s\n",
                 snapshot.level,
                 GlobalX(snapshot, draw.wallX), GlobalY(snapshot, draw.wallY),
                 draw.wallX, draw.wallY, SIDE_NAMES[draw.side],
                 face == NULL ? AUTOMAP_NO_PICTURE : face->picture,
                 face != NULL && face->functional ? 1 : 0,
                 face != NULL && face->hidden ? 1 : 0,
                 face != NULL && face->isAlcove ? 1 : 0,
                 face == NULL ? 0 : (int)face->actuators.size(),
                 GlobalX(snapshot, partyX), GlobalY(snapshot, partyY),
                 draw.drawLocation, SIDE_NAMES[frameFacing],
                 draw.drawn, keptCount, (int)kept.size(), pairs.c_str());
    written++;
  }
  return written;
}

// One line per item draw. The draw's own key says what was drawn and where, so unlike the
// decorations no placement has to be paired with it. Cells in the map's coordinates.
static int WriteThingsOfFrame(FILE *out, const AutoMapLevelSnapshot &snapshot, int partyX, int partyY)
{
  int written = 0;
  for (size_t i = 0; i < drawsOfFrame.size(); i++)
  {
    const AutoMapDrawKey &key = drawsOfFrame[i].key;
    if (key.kind != AutoMapDrawItem && key.kind != AutoMapDrawMonster) continue;
    int keptCount = 0;
    std::string pairs = PairsText(drawsOfFrame[i].kept, &keptCount);
    // A monster group has no description or cell of its own in the key: -1 for all three
    bool isItem = key.kind == AutoMapDrawItem;
    std::fprintf(out, "%d,%d,%d,%s,%s,%u,%d,%d,%d,%d,%s\n", snapshot.level,
                 GlobalX(snapshot, partyX), GlobalY(snapshot, partyY), SIDE_NAMES[frameFacing],
                 isItem ? "item" : "monster", (unsigned)key.record, key.descIndex,
                 isItem ? GlobalX(snapshot, key.x) : -1, isItem ? GlobalY(snapshot, key.y) : -1,
                 keptCount, pairs.c_str());
    written++;
  }
  return written;
}

int AutoMapMeasure_SweepPartyLevel(const char *wallsPath, const char *thingsPath, int *thingsWritten)
{
  *thingsWritten = 0;
  FILE *out = std::fopen(wallsPath, "w");
  if (out == NULL) return -1;
  FILE *things = std::fopen(thingsPath, "w");
  if (things == NULL)
  {
    std::fclose(out);
    return -1;
  }
  std::fprintf(out, "level,wallX,wallY,localX,localY,side,picture,functional,hidden,alcove,"
                    "actuators,partyX,partyY,drawLocation,facing,drawn,kept,pairCount,pairs\n");
  std::fprintf(things, "level,partyX,partyY,facing,kind,record,descIndex,cellX,cellY,kept,pairs\n");
  AutoMapLevelSnapshot snapshot = AutoMapLevelReader_ReadPartyLevel();
  int written = 0;
  isRunning = true;
  for (int y = 0; y < d.height; y++)
  {
    for (int x = 0; x < d.width; x++)
    {
      const AutoMapCellRecord *cell = CellOf(snapshot, x, y);
      if (cell == NULL || !PartyCanStandOn(*cell)) continue;
      for (int facing = 0; facing < 4; facing++)
      {
        frameFacing = facing;
        DrawViewport(facing, x, y);
        written += WriteFrame(out, snapshot, x, y);
        *thingsWritten += WriteThingsOfFrame(things, snapshot, x, y);
      }
    }
  }
  isRunning = false;
  DrawViewport(d.partyFacing, d.partyX, d.partyY);
  bool failed = std::ferror(out) != 0 || std::ferror(things) != 0;
  if (std::fclose(out) != 0) failed = true;
  if (std::fclose(things) != 0) failed = true;
  return failed ? -1 : written;
}
