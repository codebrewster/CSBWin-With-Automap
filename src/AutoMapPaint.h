#ifndef AUTOMAPPAINT_H
#define AUTOMAPPAINT_H

// AutoMapPaint : the map's own marks in one pixel style: lines with a dark rim, arrowheads,
// plates, the party arrow, claw marks, checkerboards and the 3x5 text. Everything lands on whole
// pixels. It includes AutoMapLayout.h, which carries structures: include it before CSB.h.
#include <SDL.h>
#include <string>
#include "AutoMapLayout.h"

// The renderer the functions below draw with
void AutoMapPaint_Use(SDL_Renderer *renderer);

void AutoMapPaint_Fill(const AutoMapRect &area, SDL_Color colour);
// A frame of thickness pixels inside area
void AutoMapPaint_Frame(const AutoMapRect &area, SDL_Color colour, int thickness);
// A plate: dark fill (#0d0c0a) and a one-pixel frame
void AutoMapPaint_Plate(const AutoMapRect &area, SDL_Color frame);

// The 3x5 font, pixel screen pixels per font pixel, from (x, y) at the top left
void AutoMapPaint_Text(int x, int y, const std::string &text, SDL_Color colour, int pixel);
int AutoMapPaint_TextWidth(const std::string &text, int pixel);

// A line stepped with a square brush: first a pass width + 2 wide in the rim colour (#0b0a08),
// then width wide in colour. Dashed lines leave gaps of 4 * width.
// rim false: the colour pass only, for shapes made of many lines (arcs) whose rims are drawn first
void AutoMapPaint_Line(float x0, float y0, float x1, float y1, SDL_Color colour, int width, bool dashed,
                       bool rim = true);

enum AutoMapHead
{
  AutoMapHeadFilled,
  AutoMapHeadHollow,
  AutoMapHeadOpen,
  AutoMapHeadRhomb // a toggle: a filled diamond, half as wide as long
};

// An arrowhead with its tip at (tipX, tipY), pointing along angle (radians, screen axes: 0 east,
// pi/2 south), size pixels long, on a rim
void AutoMapPaint_Head(float tipX, float tipY, float angle, float size, SDL_Color colour, AutoMapHead kind);

void AutoMapPaint_Triangle(float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color colour);

// The party's arrow around (centreX, centreY), radius r, facing 0 north .. 3 west
void AutoMapPaint_PartyArrow(float centreX, float centreY, float radius, int facing, SDL_Color fill,
                             bool outline);
// Three claw marks: a monster
void AutoMapPaint_Claw(const AutoMapRect &box, SDL_Color colour);
// A checkerboard of square-pixel squares over area at alpha, cut at its edges
void AutoMapPaint_Dither(const AutoMapRect &area, SDL_Color colour, int alpha, int square);

#endif
