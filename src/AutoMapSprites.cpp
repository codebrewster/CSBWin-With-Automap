// AutoMapSprites.cpp : decodes pictures of the graphics file into textures for the map window.
#include "stdafx.h"

// Before CSB.h, which leaves #pragma pack(1) switched on
#include <cstdint>
#include <vector>
#include "AutoMapSprites.h"

struct PlanarPicture
{
  int width;
  int height;
  std::vector<uint8_t> rows;
};

#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

static const int PALETTE_SIZE = 16;
static const int PIXELS_PER_GROUP = 16;
static const int BIT_PLANES = 4;
static const int BYTES_PER_GROUP = 8;
static const int LARGEST_WIDTH = 320;
static const int LARGEST_HEIGHT = 200;
static const int HEADER_BYTES = 4;
// The sizes of the work areas LZWExpand expects (Graphics.cpp)
static const int LZW_SCRATCH_BYTES = 16384;
static const int LZW_STACK_BYTES = 5004;
// ExpandGraphic writes whole groups and may run a little past the last row
static const int EXPAND_SLACK_BYTES = 64;

static int BytesPerRow(int width)
{
  return ((width + PIXELS_PER_GROUP - 1) & ~(PIXELS_PER_GROUP - 1)) / 2;
}

static int HeaderWord(const std::vector<ui8> &storedPicture, int offset)
{
  i16 word = wordGear(&storedPicture[offset]);
  return bigEndianGraphics ? LE16(word) : (ui16)word;
}

// A handle of our own keeps the engine's read position and its one-cluster cache untouched
static bool ReadStoredPicture(int picture, std::vector<ui8> *storedPicture)
{
  i32 sizeInFile = d.GraphicCompressedSizes[picture];
  i32 sizeUnpacked = d.GraphicDecompressedSizes[picture];
  if (sizeUnpacked < HEADER_BYTES) return false;
  i16 file = OPEN(graphicName, "rb");
  if (file < 0) return false;
  storedPicture->assign(sizeUnpacked + EXPAND_SLACK_BYTES, 0);
  LSEEK(LocateNthGraphic(picture), file, SEEK_SET);
  i32 sizeRead;
  if (sizeInFile == sizeUnpacked)
  {
    sizeRead = READ(file, sizeInFile, storedPicture->data());
  }
  else
  {
    std::vector<ui8> scratch(LZW_SCRATCH_BYTES);
    std::vector<ui8> stack(LZW_STACK_BYTES);
    sizeRead = LZWExpand(file, sizeInFile, storedPicture->data(), scratch.data(), stack.data());
  }
  CLOSE(file);
  return sizeRead == sizeUnpacked;
}

static bool DecodePicture(int picture, PlanarPicture *decoded)
{
  if (picture < 0 || picture >= d.NumGraphic) return false;
  if (d.GraphicCompressedSizes == NULL || d.GraphicDecompressedSizes == NULL) return false;
  std::vector<ui8> storedPicture;
  if (!ReadStoredPicture(picture, &storedPicture)) return false;
  decoded->width = HeaderWord(storedPicture, 0);
  decoded->height = HeaderWord(storedPicture, 2);
  bool hasSensibleSize = decoded->width > 0 && decoded->width <= LARGEST_WIDTH
                      && decoded->height > 0 && decoded->height <= LARGEST_HEIGHT;
  if (!hasSensibleSize) return false;
  i32 rowBytes = BytesPerRow(decoded->width) * decoded->height;
  decoded->rows.assign(rowBytes + EXPAND_SLACK_BYTES, 0);
  ExpandGraphic((i8 *)storedPicture.data(), decoded->rows.data(), 0, 0);
  return true;
}

// Atari ST layout: every 16 pixels are four big-endian words, one per bit of the colour number
static int ColourNumberAt(const PlanarPicture &picture, int x, int y)
{
  const ui8 *group = &picture.rows[(size_t)y * BytesPerRow(picture.width)
                                   + (x / PIXELS_PER_GROUP) * BYTES_PER_GROUP];
  int bitInWord = PIXELS_PER_GROUP - 1 - x % PIXELS_PER_GROUP;
  int colourNumber = 0;
  for (int plane = 0; plane < BIT_PLANES; plane++)
  {
    int word = (group[2 * plane] << 8) | group[2 * plane + 1];
    colourNumber |= ((word >> bitInWord) & 1) << plane;
  }
  return colourNumber;
}

// The brightest of the engine's six palettes. Three bits per channel: 0x0RGB.
// Entries 9 and 10 are repainted for every level from its monster types, so they come
// from the colour schemes of the level the map is showing instead.
static int s_scheme9 = 8;
static int s_scheme10 = 12;

// specialColors holds 13 schemes; a damaged dungeon could name one beyond that
static int SchemeOrDefault(int scheme, int fallback)
{
  return scheme < 0 || scheme > 12 ? fallback : scheme;
}

static int PackedColour(int colourNumber, const AutoMapSpriteRequest &request)
{
  if (!request.portraitPalette)
  {
    int scheme9 = request.scheme9PlusOne > 0 ? SchemeOrDefault(request.scheme9PlusOne - 1, 8) : s_scheme9;
    int scheme10 = request.scheme10PlusOne > 0 ? SchemeOrDefault(request.scheme10PlusOne - 1, 12) : s_scheme10;
    if (colourNumber == 9) return LE16(d.specialColors[scheme9][0]);
    if (colourNumber == 10) return LE16(d.specialColors[scheme10][0]);
    return d.Palette552[0].color[colourNumber];
  }
  // The portrait palette is byte-swapped at load like the rest of Palette552,
  // so unlike specialColors it needs no LE16 here.
  if (colourNumber == 9) return d.Palette11978.color[9];
  if (colourNumber == 10) return d.Palette11978.color[10];
  return d.Palette552[0].color[colourNumber];
}

static SDL_Color ColourOfTheGame(int colourNumber, const AutoMapSpriteRequest &request)
{
  int packed = PackedColour(colourNumber, request);
  SDL_Color colour;
  colour.r = (Uint8)(((packed >> 8) & 7) * 255 / 7);
  colour.g = (Uint8)(((packed >> 4) & 7) * 255 / 7);
  colour.b = (Uint8)((packed & 7) * 255 / 7);
  colour.a = SDL_ALPHA_OPAQUE;
  return colour;
}

// The pictures are drawn scaled with linear filtering, which mixes the colour of a transparent
// pixel into its opaque neighbours. That colour is the palette entry left out (often 10, which
// every level repaints for its monsters), so edges would take a tint of the level: purple on one,
// blue on the next. A transparent pixel takes the mean colour of its opaque neighbours instead,
// black when it has none.
static void BleedIntoTransparentPixels(std::vector<Uint8> *pixels, int width, int height)
{
  std::vector<Uint8> &p = *pixels;
  std::vector<Uint8> source(p);
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      size_t at = ((size_t)y * width + x) * 4;
      if (source[at + 3] != SDL_ALPHA_TRANSPARENT) continue;
      int r = 0, g = 0, b = 0, opaque = 0;
      for (int dy = -1; dy <= 1; dy++)
      {
        for (int dx = -1; dx <= 1; dx++)
        {
          int nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
          size_t n = ((size_t)ny * width + nx) * 4;
          if (source[n + 3] == SDL_ALPHA_TRANSPARENT) continue;
          r += source[n];
          g += source[n + 1];
          b += source[n + 2];
          opaque++;
        }
      }
      p[at] = (Uint8)(opaque ? r / opaque : 0);
      p[at + 1] = (Uint8)(opaque ? g / opaque : 0);
      p[at + 2] = (Uint8)(opaque ? b / opaque : 0);
    }
  }
}

static std::vector<Uint8> ToRgba(const PlanarPicture &picture,
                             const AutoMapSpriteRequest &request)
{
  std::vector<Uint8> pixels;
  pixels.reserve((size_t)picture.width * picture.height * 4);
  for (int y = 0; y < picture.height; y++)
  {
    for (int x = 0; x < picture.width; x++)
    {
      int colourNumber = ColourNumberAt(picture, x, y);
      SDL_Color colour = ColourOfTheGame(colourNumber, request);
      if (colourNumber == request.transparentColour) colour.a = SDL_ALPHA_TRANSPARENT;
      pixels.push_back(colour.r);
      pixels.push_back(colour.g);
      pixels.push_back(colour.b);
      pixels.push_back(colour.a);
    }
  }
  BleedIntoTransparentPixels(&pixels, picture.width, picture.height);
  return pixels;
}

static SDL_Texture *CreateTexture(SDL_Renderer *renderer, const PlanarPicture &picture,
                                   const AutoMapSpriteRequest &request)
{
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STATIC, picture.width, picture.height);
  if (texture == NULL) return NULL;
  std::vector<Uint8> pixels = ToRgba(picture, request);
  SDL_UpdateTexture(texture, NULL, pixels.data(), picture.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
  // The pictures are drawn far smaller than they are stored
  SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif
  return texture;
}

static long long KeyOf(const AutoMapSpriteRequest &request)
{
  long long key = (long long)request.picture * (PALETTE_SIZE + 1) + (request.transparentColour + 1);
  key = key * 2 + (request.hasTile ? 1 : 0);
  if (request.hasTile)
  {
    key = (((key * 512 + request.tileX) * 512 + request.tileY) * 32 + request.tileW) * 32
        + request.tileH;
  }
  key = key * 2 + (request.portraitPalette ? 1 : 0);
  key = (key * 14 + request.scheme9PlusOne) * 14 + request.scheme10PlusOne;
  return key;
}

AutoMapSprites::AutoMapSprites(void) : m_renderer(NULL)
{
}

void AutoMapSprites::UseColourSchemes(int scheme9, int scheme10)
{
  s_scheme9 = SchemeOrDefault(scheme9, 8);
  s_scheme10 = SchemeOrDefault(scheme10, 12);
}

void AutoMapSprites::UseRenderer(SDL_Renderer *renderer)
{
  ForgetAll();
  m_renderer = renderer;
}

AutoMapSprite AutoMapSprites::Create(const AutoMapSpriteRequest &request)
{
  AutoMapSprite sprite = {NULL, 0, 0};
  PlanarPicture picture;
  if (m_renderer != NULL && DecodePicture(request.picture, &picture))
  {
    sprite.texture = CreateTexture(m_renderer, picture, request);
    sprite.width = picture.width;
    sprite.height = picture.height;
  }
  if (sprite.texture == NULL)
  {
    printf("Automap: picture %d cannot be shown, drawing a plain marker instead\n", request.picture);
  }
  return sprite;
}

// A picture that failed is remembered as well, so it is reported once and not decoded again
const AutoMapSprite *AutoMapSprites::Find(const AutoMapSpriteRequest &request)
{
  long long key = KeyOf(request);
  std::map<long long, AutoMapSprite>::iterator found = m_spritesByRequest.find(key);
  if (found == m_spritesByRequest.end())
  {
    found = m_spritesByRequest.insert(std::make_pair(key, Create(request))).first;
  }
  return found->second.texture == NULL ? NULL : &found->second;
}

void AutoMapSprites::ForgetAll(void)
{
  std::map<long long, AutoMapSprite>::iterator sprite;
  for (sprite = m_spritesByRequest.begin(); sprite != m_spritesByRequest.end(); ++sprite)
  {
    if (sprite->second.texture != NULL) SDL_DestroyTexture(sprite->second.texture);
  }
  m_spritesByRequest.clear();
}

bool AutoMapSprites_Pixels(const AutoMapSpriteRequest &request, std::vector<Uint8> *rgba, int *outW, int *outH)
{
  PlanarPicture picture;
  if (!DecodePicture(request.picture, &picture)) return false;
  std::vector<Uint8> pixels = ToRgba(picture, request);
  int x0 = 0, y0 = 0, w = picture.width, h = picture.height;
  if (request.hasTile)
  {
    x0 = request.tileX; y0 = request.tileY; w = request.tileW; h = request.tileH;
    if (x0 + w > picture.width || y0 + h > picture.height) return false;
  }
  if (w <= 0 || h <= 0) return false;
  rgba->assign((size_t)w * h * 4, 0);
  for (int y = 0; y < h; y++)
    memcpy(&(*rgba)[(size_t)y * w * 4], &pixels[((size_t)(y + y0) * picture.width + x0) * 4], (size_t)w * 4);
  *outW = w;
  *outH = h;
  return true;
}

// One picture as a 32-bit BMP with alpha, for the export (AutoMapExport.cpp)
bool AutoMapSprites_SaveBmp(const AutoMapSpriteRequest &request, const char *path)
{
  std::vector<Uint8> cut;
  int w = 0, h = 0;
  if (!AutoMapSprites_Pixels(request, &cut, &w, &h)) return false;
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(cut.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
  if (surface == NULL) return false;
  bool ok = SDL_SaveBMP(surface, path) == 0;
  SDL_FreeSurface(surface);
  return ok;
}
