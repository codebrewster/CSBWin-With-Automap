#ifndef WINDOW_POSITION_H
#define WINDOW_POSITION_H

#include <SDL.h>
#include <string.h>

// Where a window sits, in the form SDL_CreateWindow takes it back.
// Under X11 the window manager places the frame at the position a window is
// created with, but SDL_GetWindowPosition reports the inside of the window,
// below the title bar; saving that as it is moves the window down by one
// title bar on every start. Elsewhere SDL places and reports the inside alike.
static inline void GetWindowPlacePosition(SDL_Window *window, int *x, int *y)
{
  SDL_GetWindowPosition(window, x, y);
  const char *driver = SDL_GetCurrentVideoDriver();
  if (driver == NULL || strcmp(driver, "x11") != 0) return;
  int top = 0, left = 0;
  if (SDL_GetWindowBordersSize(window, &top, &left, NULL, NULL) != 0) return;
  *x -= left;
  *y -= top;
}

#endif
