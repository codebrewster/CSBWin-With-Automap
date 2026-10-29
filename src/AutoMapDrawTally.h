#ifndef AUTOMAPDRAWTALLY_H
#define AUTOMAPDRAWTALLY_H

#include <vector>
#include "AutoMapKnowledge.h"
#include "AutoMapSight.h"

// What the 3D view drew in a frame and how much of it is still on screen once nearer cells
// were drawn over it. Wall decorations, floor items and monsters are measured the same way, so
// the recorder can tell which of them the party noticed. Shut doors too.

enum AutoMapDrawKind { AutoMapDrawDecoration, AutoMapDrawItem, AutoMapDrawMonster, AutoMapDrawAlcoveItem,
                       AutoMapDrawDoor, AutoMapDrawFloorDecoration };

// What was drawn: drawLocation for a decoration; record, descIndex and the cell (level
// coordinates) for an item, the wall cell for an item in an alcove; record (the group) for a
// monster; the cell for a door or a floor decoration. A decoration's descIndex is
// AUTOMAP_SCRIBBLES_DRAWN when it is wall text drawn as scribbles. Unused fields are -1 /
// AUTOMAP_NO_RECORD.
const int AUTOMAP_SCRIBBLES_DRAWN = 1;

struct AutoMapDrawKey
{
  AutoMapDrawKind kind;
  int drawLocation;
  AutoMapRecordId record;
  int descIndex;
  int x;
  int y;
};
bool operator==(const AutoMapDrawKey &left, const AutoMapDrawKey &right);

// stamp: the blit that wrote the pixel last when the draw was done, so that a later blit in the
// same colour still tells that it drew over it
struct AutoMapDrawnPixel { int x; int y; int behind; int drawn; int stamp; };

struct AutoMapDrawOnScreen
{
  AutoMapDrawKey key;
  // The kept pixels as pairs of colour behind and colour drawn, in behind * 16 + drawn order
  std::vector<AutoMapPixelPair> kept;
};

// The colour index on screen at (x, y) of the finished frame; or, as a write stamp, the blit
// that wrote (x, y) last
typedef int (*AutoMapScreenPixel)(int x, int y);

class AutoMapDrawTally
{
public:
  void StartFrame(void);
  // Draws with an equal key are merged: a group's members add up
  void AddDraw(const AutoMapDrawKey &key, const std::vector<AutoMapDrawnPixel> &changed);
  // Keeps the pixels whose screen colour is still the drawn one and, given writeStamp, that no
  // later blit wrote; in the order first added
  std::vector<AutoMapDrawOnScreen> FinishFrame(AutoMapScreenPixel screenPixel,
                                               AutoMapScreenPixel writeStamp = NULL) const;
  // The finished frame, width * height colours row by row, as it would be without the floor items:
  // where the last writer of a pixel is an item's blit, the colour behind the lowest item there
  std::vector<int> FrameWithoutItems(int width, int height, AutoMapScreenPixel screenPixel,
                                     AutoMapScreenPixel writeStamp) const;
  // Every pixel the draws with this key changed, kept or drawn over; NULL for no such draw
  const std::vector<AutoMapDrawnPixel> *PixelsOf(const AutoMapDrawKey &key) const;

private:
  struct Draw
  {
    AutoMapDrawKey key;
    std::vector<AutoMapDrawnPixel> pixels;
  };
  std::vector<Draw> draws;
};

#endif
