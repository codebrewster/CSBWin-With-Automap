// AutoMapPaint.cpp : see AutoMapPaint.h
#include "stdafx.h"

#include <algorithm>
#include <cmath>

#include "AutoMapPaint.h"
#include "AutoMapScene.h"

static SDL_Renderer *painter = NULL;
static const SDL_Color RIM = {0x0b, 0x0a, 0x08, 0xff};
static const SDL_Color PLATE_FILL = {0x0d, 0x0c, 0x0a, 0xff};

static void UseColour(SDL_Color colour)
{
  SDL_SetRenderDrawColor(painter, colour.r, colour.g, colour.b, colour.a);
}

void AutoMapPaint_Use(SDL_Renderer *renderer)
{
  painter = renderer;
}

void AutoMapPaint_Fill(const AutoMapRect &area, SDL_Color colour)
{
  if (painter == NULL || area.w <= 0 || area.h <= 0) return;
  SDL_Rect rect = {area.x, area.y, area.w, area.h};
  UseColour(colour);
  SDL_RenderFillRect(painter, &rect);
}

void AutoMapPaint_Frame(const AutoMapRect &area, SDL_Color colour, int thickness)
{
  int t = std::max(1, std::min(thickness, std::min(area.w, area.h) / 2));
  AutoMapRect top = {area.x, area.y, area.w, t};
  AutoMapRect bottom = {area.x, area.y + area.h - t, area.w, t};
  AutoMapRect left = {area.x, area.y, t, area.h};
  AutoMapRect right = {area.x + area.w - t, area.y, t, area.h};
  AutoMapPaint_Fill(top, colour);
  AutoMapPaint_Fill(bottom, colour);
  AutoMapPaint_Fill(left, colour);
  AutoMapPaint_Fill(right, colour);
}

void AutoMapPaint_Plate(const AutoMapRect &area, SDL_Color frame)
{
  AutoMapPaint_Fill(area, PLATE_FILL);
  AutoMapPaint_Frame(area, frame, 1);
}

void AutoMapPaint_Text(int x, int y, const std::string &text, SDL_Color colour, int pixel)
{
  if (painter == NULL || pixel <= 0) return;
  UseColour(colour);
  for (size_t i = 0; i < text.size(); i++)
  {
    for (int row = 0; row < 5; row++)
    {
      for (int col = 0; col < 3; col++)
      {
        if (!AutoMapScene_FontPixel(text[i], row, col)) continue;
        SDL_Rect dot = {x + ((int)i * 4 + col) * pixel, y + row * pixel, pixel, pixel};
        SDL_RenderFillRect(painter, &dot);
      }
    }
  }
}

int AutoMapPaint_TextWidth(const std::string &text, int pixel)
{
  return AutoMapScene_TextWidth(text.c_str()) * pixel;
}

static void StepBrush(float x0, float y0, float x1, float y1, int brush, int width, bool dashed)
{
  float steps = std::max(std::fabs(x1 - x0), std::fabs(y1 - y0));
  if (steps < 1.0f) return;
  int stride = std::max(1, width / 2);
  for (int i = 0; i <= (int)steps; i += stride)
  {
    if (dashed && ((i / (width * 4)) % 2) == 1) continue;
    int x = (int)std::lround(x0 + (x1 - x0) * i / steps);
    int y = (int)std::lround(y0 + (y1 - y0) * i / steps);
    SDL_Rect square = {x - brush / 2, y - brush / 2, brush, brush};
    SDL_RenderFillRect(painter, &square);
  }
}

void AutoMapPaint_Line(float x0, float y0, float x1, float y1, SDL_Color colour, int width, bool dashed,
                       bool rim)
{
  if (painter == NULL || width <= 0) return;
  if (rim)
  {
    UseColour(RIM);
    StepBrush(x0, y0, x1, y1, width + 2, width, dashed);
  }
  UseColour(colour);
  StepBrush(x0, y0, x1, y1, width, width, dashed);
}

void AutoMapPaint_Triangle(float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color colour)
{
  if (painter == NULL) return;
  // Sorted by y, filled one pixel row at a time: pixel exact with every SDL2
  float px[3] = {x0, x1, x2};
  float py[3] = {y0, y1, y2};
  for (int a = 0; a < 2; a++)
    for (int b = 0; b < 2 - a; b++)
      if (py[b] > py[b + 1])
      {
        std::swap(py[b], py[b + 1]);
        std::swap(px[b], px[b + 1]);
      }
  UseColour(colour);
  for (int y = (int)std::ceil(py[0]); y <= (int)std::floor(py[2]); y++)
  {
    float tLong = py[2] > py[0] ? (y - py[0]) / (py[2] - py[0]) : 0.0f;
    float onLong = px[0] + (px[2] - px[0]) * tLong;
    bool lower = y > py[1];
    float sy0 = lower ? py[1] : py[0], sy1 = lower ? py[2] : py[1];
    float sx0 = lower ? px[1] : px[0], sx1 = lower ? px[2] : px[1];
    float tShort = sy1 > sy0 ? (y - sy0) / (sy1 - sy0) : 0.0f;
    float onShort = sx0 + (sx1 - sx0) * tShort;
    int from = (int)std::lround(std::min(onLong, onShort));
    int to = (int)std::lround(std::max(onLong, onShort));
    SDL_RenderDrawLine(painter, from, y, to, y);
  }
}

void AutoMapPaint_Head(float tipX, float tipY, float angle, float size, SDL_Color colour, AutoMapHead kind)
{
  float ax = std::cos(angle), ay = std::sin(angle);
  float leftX = tipX - ax * size - ay * size * 0.6f, leftY = tipY - ay * size + ax * size * 0.6f;
  float rightX = tipX - ax * size + ay * size * 0.6f, rightY = tipY - ay * size - ax * size * 0.6f;
  // The rim: the same head a pixel and a half larger all round
  AutoMapPaint_Triangle(tipX + ax * 2, tipY + ay * 2, leftX - ax * 1.5f - ay * 1.5f, leftY - ay * 1.5f + ax * 1.5f,
                        rightX - ax * 1.5f + ay * 1.5f, rightY - ay * 1.5f - ax * 1.5f, RIM);
  if (kind == AutoMapHeadFilled)
  {
    AutoMapPaint_Triangle(tipX, tipY, leftX, leftY, rightX, rightY, colour);
    return;
  }
  int width = 2;
  UseColour(colour);
  StepBrush(leftX, leftY, tipX, tipY, width, width, false);
  StepBrush(tipX, tipY, rightX, rightY, width, width, false);
  if (kind == AutoMapHeadHollow) StepBrush(rightX, rightY, leftX, leftY, width, width, false);
}

void AutoMapPaint_PartyArrow(float centreX, float centreY, float radius, int facing, SDL_Color fill,
                             bool outline)
{
  static const int DX[4] = {0, 1, 0, -1};
  static const int DY[4] = {-1, 0, 1, 0};
  int f = facing & 3;
  float dx = (float)DX[f], dy = (float)DY[f], qx = -dy, qy = dx;
  float tipX = centreX + dx * radius, tipY = centreY + dy * radius;
  float leftX = centreX - dx * 0.8f * radius + qx * 0.8f * radius, leftY = centreY - dy * 0.8f * radius + qy * 0.8f * radius;
  float notchX = centreX - dx * 0.3f * radius, notchY = centreY - dy * 0.3f * radius;
  float rightX = centreX - dx * 0.8f * radius - qx * 0.8f * radius, rightY = centreY - dy * 0.8f * radius - qy * 0.8f * radius;
  AutoMapPaint_Triangle(tipX, tipY, leftX, leftY, notchX, notchY, fill);
  AutoMapPaint_Triangle(tipX, tipY, notchX, notchY, rightX, rightY, fill);
  if (!outline) return;
  SDL_Color white = {0xff, 0xff, 0xff, 0xff};
  UseColour(white);
  SDL_RenderDrawLine(painter, (int)tipX, (int)tipY, (int)leftX, (int)leftY);
  SDL_RenderDrawLine(painter, (int)leftX, (int)leftY, (int)notchX, (int)notchY);
  SDL_RenderDrawLine(painter, (int)notchX, (int)notchY, (int)rightX, (int)rightY);
  SDL_RenderDrawLine(painter, (int)rightX, (int)rightY, (int)tipX, (int)tipY);
}

// Three slanted strokes of four dots each, kept inside the box with a margin on every side
void AutoMapPaint_Claw(const AutoMapRect &box, SDL_Color colour)
{
  int side = std::min(box.w, box.h);
  int margin = std::max(1, side * 15 / 100);
  int inner = side - 2 * margin;
  int p = std::max(1, inner * 2 / 13);
  int down = p + p / 2;
  int across = p / 2;
  int height = 3 * down + 2 * p;
  int gap = std::max(0, (inner - 3 * across - p) / 2);
  int width = 2 * gap + 3 * across + p;
  int left = box.x + (box.w - width) / 2;
  int top = box.y + (box.h - height) / 2;
  for (int mark = 0; mark < 3; mark++)
  {
    for (int step = 0; step < 4; step++)
    {
      AutoMapRect dot = {left + mark * gap + step * across, top + step * down, p, p * 2};
      AutoMapPaint_Fill(dot, colour);
    }
  }
}

void AutoMapPaint_Dither(const AutoMapRect &area, SDL_Color colour, int alpha, int square)
{
  if (square <= 0) return;
  SDL_Color faint = colour;
  faint.a = (Uint8)std::max(0, std::min(255, alpha));
  for (int row = 0; row * square < area.h; row++)
  {
    for (int col = 0; col * square < area.w; col++)
    {
      if ((row + col) % 2 != 0) continue;
      AutoMapRect cell = {area.x + col * square, area.y + row * square, std::min(square, area.w - col * square),
                          std::min(square, area.h - row * square)};
      AutoMapPaint_Fill(cell, faint);
    }
  }
}
