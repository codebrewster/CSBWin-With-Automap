#ifndef AUTOMAPMEASURE_H
#define AUTOMAPMEASURE_H

struct SUMMARIZEROOMDATA;

#include <vector>
#include "AutoMapDrawTally.h"
#include "AutoMapSight.h"

// What every frame of the 3D view shows of its wall decorations and floor items: the pixels each
// one drew and kept once the nearer cells were drawn over it, so the recorder can tell which
// were noticed.
// The same measurement serves a debug sweep over the party's level: the 3D view is drawn from every floor cell in every
// facing, and every wall decoration it draws is measured the way it ends up on screen. Each
// draw writes one line to a CSV file: the wall face, where the party stands, the view spot
// (drawLocation 0..12) and the pixels the decoration changed and kept after the nearer cells
// were drawn over it, as pairs of the colour behind and the colour drawn. Colours are the
// viewport's colour indices, before the light's palette, so one sweep serves every light.

// The floor item draws go to a second CSV, one line each: where the party stands, the item's
// record, object description and cell, and its kept pixels.
//
// Runs the sweep in the engine's context and redraws the party's own view afterwards.
// Returns the number of decoration draws written, -1 when a file could not be written;
// thingsWritten: the number of item draws.
int AutoMapMeasure_SweepPartyLevel(const char *wallsPath, const char *thingsPath, int *thingsWritten);
bool AutoMapMeasure_IsRunning(void);

// Every measured draw of the frame DrawViewport finished last, in the order first drawn
const std::vector<AutoMapDrawOnScreen> &AutoMapMeasure_DrawsOfFrame(void);

// Called by the hooks on every frame: the frame starts, something is drawn, the frame ends
void AutoMapMeasure_OnViewportSummarised(const SUMMARIZEROOMDATA *cellsInView);
// The rectangle about to be drawn into, viewport pixels inclusive, clamped to the viewport
void AutoMapMeasure_BeforeBlit(int x1, int x2, int y1, int y2);
// The same rectangle has just been drawn into; the pixels it changed go to the tally. Ignored
// without a matching Before.
void AutoMapMeasure_AfterBlit(const AutoMapDrawKey &key, int x1, int x2, int y1, int y2);
// A blit has drawn src into the viewport rectangle x1..x2, y1..y2 (inclusive, unclamped): every
// pixel whose source pixel is not transparentColor (-1: none is) was written, even in the colour
// already there. A draw measured before it does not keep those pixels.
void AutoMapMeasure_OnViewportBlit(const unsigned char *src, int x1, int x2, int y1, int y2,
                                   int srcOffsetX, int srcOffsetY, int srcByteWidth,
                                   int transparentColor);
// The frame is complete: DrawViewport clears the viewport right after this
void AutoMapMeasure_OnViewportFinished(void);

// A floor item or a decoration drawn in the frame DrawViewport finished last, as it shows on
// screen: a floor item against the frame without the floor items, a decoration against the wall
AutoMapDrawView AutoMapMeasure_ViewOf(const AutoMapDrawKey &key, const short *palette);

// Debug: the finished frame as an indexed PNG in the light's colours, three times side by side: as
// shown, with the pixels of the draws of kind marked (magenta: they count for being seen), and the background they are judged
// against: for items the frame without the floor items, for decorations the wall without them. Magenta: on screen and standing out; cyan: on
// screen but camouflaged; yellow: drawn over by a later blit. Returns false
// when the file could not be written.
bool AutoMapMeasure_WritePicture(const char *path, const short *palette, AutoMapDrawKind kind);

#endif
