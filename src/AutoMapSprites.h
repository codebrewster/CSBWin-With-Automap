#ifndef AUTOMAPSPRITES_H
#define AUTOMAPSPRITES_H

#include <SDL.h>
#include <map>
#include <vector>

struct AutoMapSprite
{
  SDL_Texture *texture;
  int width;
  int height;
};

struct AutoMapSpriteRequest
{
  // Number of the picture in the graphics file
  int picture;
  // Palette entry that is left out when the picture is drawn; -1 draws opaque
  // (inventory tiles carry their own background)
  int transparentColour;
  // Inventory tile: a 16x16 sub-rectangle of the decoded picture, the way the
  // inventory draws objects. Off when drawing whole pictures.
  bool hasTile;
  int tileX;
  int tileY;
  int tileW;
  int tileH;
  // Portrait palette for entries 9 and 10: the inventory (portraits region)
  // keeps the dungeon's own 9 and 10, while the 3D view (viewport region)
  // repaints them per level. Icons must use the portrait palette to look the
  // way they do in the inventory; world sprites use the shown level's schemes.
  bool portraitPalette;
  // The colour schemes for entries 9 and 10 plus one, for a picture of another level than the
  // shown one (a search hit); 0 takes the shown level's. Plus one so that a request that leaves
  // them out gets the shown level's.
  int scheme9PlusOne;
  int scheme10PlusOne;
};

// Turns pictures of the game's graphics file into textures of the map window.
// The pictures are decoded from the file into buffers of their own: the engine keeps only the
// pictures of the current level in memory and discards them while it draws.
class AutoMapSprites
{
public:
  AutoMapSprites(void);
  void UseRenderer(SDL_Renderer *renderer);
  // The two palette entries the engine repaints for every level: sprites are decoded
  // with the schemes of the level the map is showing
  void UseColourSchemes(int scheme9, int scheme10);
  // NULL when the picture cannot be decoded
  const AutoMapSprite *Find(const AutoMapSpriteRequest &request);
  // The engine changes two palette entries with every level, so the textures of one level
  // have wrong colours on the next
  void ForgetAll(void);

private:
  AutoMapSprite Create(const AutoMapSpriteRequest &request);
  SDL_Renderer *m_renderer;
  std::map<long long, AutoMapSprite> m_spritesByRequest;
};

// Writes one picture, as the request would draw it, to a 32-bit BMP with alpha; false when it
// cannot be decoded or written
bool AutoMapSprites_SaveBmp(const AutoMapSpriteRequest &request, const char *path);
// The picture as the request would draw it, 4 bytes a pixel (red, green, blue, alpha), rows
// from the top; false when it cannot be decoded
bool AutoMapSprites_Pixels(const AutoMapSpriteRequest &request, std::vector<Uint8> *rgba, int *w, int *h);

// A picture the engine holds in memory in the graphics file's own layout (a champion's portrait),
// with the portrait palette and colour 1 left out as the background; false when it has no size
bool AutoMapSprites_PlanarPixels(const unsigned char *rows, int w, int h, std::vector<Uint8> *rgba);

#endif
