#ifndef AUTOMAPHTML_H
#define AUTOMAPHTML_H

#include <string>
#include <utility>
#include <vector>

// The pieces of the web page the map window's EXPORT writes (AutoMapExport.cpp): one file that
// holds the page code of docs/automap-mockup, the map data gzip-compressed and the pictures as
// PNG. Pure functions over bytes, built on miniz; no engine, no SDL.

// A gzip stream (RFC 1952) of bytes, as the page's DecompressionStream('gzip') reads it
std::string AutoMapHtml_Gzip(const std::string &bytes);
std::string AutoMapHtml_Base64(const std::string &bytes);
// A PNG of w x h pixels of 4 bytes each, red green blue alpha, rows from the top; empty when
// miniz fails
std::string AutoMapHtml_Png(const unsigned char *rgba, int w, int h);
// &, <, > and " as entities
std::string AutoMapHtml_Escape(const std::string &text);

// The draw lists the page holds: every vision up to maxVision (0 fog, 1 explored live, 2 whole
// level) and every spoiler level up to maxSpoilers, numbered as in csb.cfg (0 seen, 1 learned,
// 2 all). Pairs of vision and the scene's spoiler number (0 All, 1 Learned, 2 Seen), as the
// draw lists' names L<level>_v<vision>_s<spoilers> have them.
std::vector<std::pair<int, int> > AutoMapHtml_Views(int maxVision, int maxSpoilers);

// The whole page: the head with title and subtitle (plain text, escaped here), then dataScript
// (the "window.MAPDATA=...;" statement), then the page code
std::string AutoMapHtml_Page(const std::string &title, const std::string &subtitle, const std::string &dataScript);

#endif
