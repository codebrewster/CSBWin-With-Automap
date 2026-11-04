// AutoMapExportChoice.cpp : see AutoMapExportChoice.h
#include "AutoMapExportChoice.h"

#include <cctype>
#include <cstdio>
#include <cstring>

std::vector<int> AutoMapExportChoice_Levels(AutoMapExportLevels which, int levelCount, int shownLevel,
                                            const std::vector<bool> &visited, const std::set<int> &picked)
{
  std::vector<int> levels;
  for (int level = 0; level < levelCount; level++)
  {
    bool taken = false;
    switch (which)
    {
    case AutoMapExportThisLevel: taken = level == shownLevel; break;
    case AutoMapExportVisited: taken = level < (int)visited.size() && visited[level]; break;
    case AutoMapExportAll: taken = true; break;
    case AutoMapExportPick: taken = picked.count(level) != 0; break;
    }
    if (taken) levels.push_back(level);
  }
  return levels;
}

std::set<int> AutoMapExportChoice_PickAll(int levelCount)
{
  std::set<int> all;
  for (int level = 0; level < levelCount; level++) all.insert(level);
  return all;
}

std::set<int> AutoMapExportChoice_PickInverted(const std::set<int> &picked, int levelCount)
{
  std::set<int> inverted;
  for (int level = 0; level < levelCount; level++)
    if (!picked.count(level)) inverted.insert(level);
  return inverted;
}

std::set<int> AutoMapExportChoice_PickVisited(const std::vector<bool> &visited)
{
  std::set<int> levels;
  for (size_t level = 0; level < visited.size(); level++)
    if (visited[level]) levels.insert((int)level);
  return levels;
}

void AutoMapExportChoice_SplitPath(const std::string &path, std::string *folder, std::string *name)
{
  size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos)
  {
    folder->clear();
    *name = path;
    return;
  }
  // The root keeps its slash: "/x" is x in "/"
  *folder = path.substr(0, slash == 0 ? 1 : slash);
  *name = path.substr(slash + 1);
}

std::string AutoMapExportChoice_Join(const std::string &folder, const std::string &name)
{
#ifdef _WIN32
  const char separator = '\\';
#else
  const char separator = '/';
#endif
  if (folder.empty()) return name;
  char last = folder[folder.size() - 1];
  if (last == '/' || last == '\\') return folder + name;
  return folder + separator + name;
}

std::string AutoMapExportChoice_PageName(const std::string &savePath)
{
  std::string folder, name;
  AutoMapExportChoice_SplitPath(savePath, &folder, &name);
  if (name.empty()) return "csb-map.html";
  size_t dot = name.rfind('.');
  if (dot != std::string::npos && dot > 0)
  {
    std::string extension = name.substr(dot);
    for (size_t i = 0; i < extension.size(); i++) extension[i] = (char)tolower((unsigned char)extension[i]);
    if (extension == ".dat" || extension == ".bak") name = name.substr(0, dot);
  }
  return name + "-map.html";
}

std::string AutoMapExportChoice_WithHtmlExtension(const std::string &name)
{
  size_t dot = name.rfind('.');
  if (dot != std::string::npos && dot > 0 && dot + 1 < name.size()) return name;
  if (dot != std::string::npos && dot + 1 == name.size()) return name + "html";
  return name + ".html";
}

std::string AutoMapExportChoice_FileUrl(const std::string &absolutePath)
{
  std::string path = absolutePath;
  for (size_t i = 0; i < path.size(); i++)
    if (path[i] == '\\') path[i] = '/';
  // C:/x needs the slash of an empty host in front of it
  if (path.size() >= 2 && path[1] == ':') path = "/" + path;
  std::string url = "file://";
  for (size_t i = 0; i < path.size(); i++)
  {
    unsigned char c = (unsigned char)path[i];
    bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr("-._~/:", c) != NULL;
    if (plain && c != 0) url += (char)c;
    else
    {
      char escaped[4];
      sprintf(escaped, "%%%02X", c);
      url += escaped;
    }
  }
  return url;
}

static const long PICTURE_BYTES = 350L * 1024;
static const long BYTES_PER_VIEW = 5800;

long AutoMapExportChoice_EstimatedBytes(int levelCount, int maxVision, int maxSpoilers)
{
  if (levelCount <= 0) return 0;
  long views = (long)(maxVision + 1) * (maxSpoilers + 1);
  return PICTURE_BYTES + (long)levelCount * views * BYTES_PER_VIEW;
}

std::string AutoMapExportChoice_SizeText(long bytes)
{
  char text[32];
  if (bytes < 1000L * 1024) sprintf(text, "%ld KB", (bytes + 1023) / 1024);
  else sprintf(text, "%.1f MB", bytes / (1024.0 * 1024.0));
  return text;
}

std::string AutoMapExportChoice_Typed(const std::string &name, const char *utf8)
{
  std::string typed = name;
  for (const char *p = utf8; p != NULL && *p != '\0'; p++)
  {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c == 127 || strchr("/\\:*?\"<>|", c) != NULL) continue;
    typed += (char)c;
  }
  return typed;
}

std::string AutoMapExportChoice_Erased(const std::string &name)
{
  if (name.empty()) return name;
  size_t end = name.size() - 1;
  // Continuation bytes of UTF-8 are 10xxxxxx: step back to the first byte of the character
  while (end > 0 && ((unsigned char)name[end] & 0xc0) == 0x80) end--;
  return name.substr(0, end);
}
