// AutoMapDrawTally.cpp : keeps the pixels each draw of the 3D view changed and counts the ones
// that survive to the finished frame.
#include "AutoMapDrawTally.h"

#include <algorithm>
#include <map>
#include <set>

bool operator==(const AutoMapDrawKey &left, const AutoMapDrawKey &right)
{
  return left.kind == right.kind && left.drawLocation == right.drawLocation &&
         left.record == right.record && left.descIndex == right.descIndex &&
         left.x == right.x && left.y == right.y;
}

void AutoMapDrawTally::StartFrame(void)
{
  draws.clear();
}

void AutoMapDrawTally::AddDraw(const AutoMapDrawKey &key, const std::vector<AutoMapDrawnPixel> &changed)
{
  for (size_t i = 0; i < draws.size(); i++)
  {
    if (!(draws[i].key == key)) continue;
    draws[i].pixels.insert(draws[i].pixels.end(), changed.begin(), changed.end());
    return;
  }
  Draw draw;
  draw.key = key;
  draw.pixels = changed;
  draws.push_back(draw);
}

static bool DrawnLater(const AutoMapDrawnPixel &left, const AutoMapDrawnPixel &right)
{
  return left.stamp > right.stamp;
}

std::vector<int> AutoMapDrawTally::FrameWithoutItems(int width, int height, AutoMapScreenPixel screenPixel,
                                                     AutoMapScreenPixel writeStamp) const
{
  std::vector<int> frame((size_t)(width * height));
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++) frame[(size_t)(y * width + x)] = screenPixel(x, y);
  }
  std::vector<AutoMapDrawnPixel> itemPixels;
  std::set<int> itemBlits;
  for (size_t i = 0; i < draws.size(); i++)
  {
    if (draws[i].key.kind != AutoMapDrawItem) continue;
    for (size_t p = 0; p < draws[i].pixels.size(); p++)
    {
      itemPixels.push_back(draws[i].pixels[p]);
      itemBlits.insert(draws[i].pixels[p].stamp);
    }
  }
  // The upper items first, so that the lowest one has the last word: what lies under the pile
  std::stable_sort(itemPixels.begin(), itemPixels.end(), DrawnLater);
  for (size_t p = 0; p < itemPixels.size(); p++)
  {
    const AutoMapDrawnPixel &pixel = itemPixels[p];
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= width || pixel.y >= height) continue;
    if (itemBlits.count(writeStamp(pixel.x, pixel.y)) == 0) continue;
    frame[(size_t)(pixel.y * width + pixel.x)] = pixel.behind;
  }
  return frame;
}

const std::vector<AutoMapDrawnPixel> *AutoMapDrawTally::PixelsOf(const AutoMapDrawKey &key) const
{
  for (size_t i = 0; i < draws.size(); i++)
  {
    if (draws[i].key == key) return &draws[i].pixels;
  }
  return NULL;
}

std::vector<AutoMapDrawOnScreen> AutoMapDrawTally::FinishFrame(AutoMapScreenPixel screenPixel,
                                                               AutoMapScreenPixel writeStamp) const
{
  std::vector<AutoMapDrawOnScreen> result;
  for (size_t i = 0; i < draws.size(); i++)
  {
    std::map<int, int> pairs;
    for (size_t p = 0; p < draws[i].pixels.size(); p++)
    {
      const AutoMapDrawnPixel &pixel = draws[i].pixels[p];
      if (screenPixel(pixel.x, pixel.y) != pixel.drawn) continue;
      if (writeStamp != NULL && writeStamp(pixel.x, pixel.y) != pixel.stamp) continue;
      pairs[pixel.behind * 16 + pixel.drawn]++;
    }
    AutoMapDrawOnScreen onScreen;
    onScreen.key = draws[i].key;
    for (std::map<int, int>::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
    {
      AutoMapPixelPair pair = {it->first / 16, it->first % 16, it->second};
      onScreen.kept.push_back(pair);
    }
    result.push_back(onScreen);
  }
  return result;
}
