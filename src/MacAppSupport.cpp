// MacAppSupport.cpp : keeps saved games and settings of a macOS .app bundle in Application Support.
#include "MacAppSupport.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SDL.h>
#include <copyfile.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static const char *SUPPORT_FOLDER_ORGANIZATION = "CSB";
static const char *BUNDLED_GAME_FOLDER = "game";

static void FailStartup(const std::string &message)
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CSB", message.c_str(), NULL);
  exit(1);
}

static std::string TakeSdlPath(char *sdlPath, const char *purpose)
{
  if (sdlPath == NULL)
  {
    FailStartup(std::string("Cannot find the ") + purpose + ": " + SDL_GetError());
  }
  std::string path = sdlPath;
  SDL_free(sdlPath);
  return path;
}

// A plain executable has no Info.plist and therefore no bundle name
static std::string BundleName(void)
{
  CFBundleRef bundle = CFBundleGetMainBundle();
  if (bundle == NULL) return "";
  CFTypeRef name = CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleNameKey);
  if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID()) return "";
  char utf8Name[NAME_MAX + 1];
  if (!CFStringGetCString((CFStringRef)name, utf8Name, sizeof(utf8Name), kCFStringEncodingUTF8)) return "";
  return utf8Name;
}

static bool IsDirectory(const std::string &path)
{
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

static bool Exists(const std::string &path)
{
  struct stat info;
  return lstat(path.c_str(), &info) == 0;
}

static void CopyMissingEntries(const std::string &sourceFolder, const std::string &targetFolder);

static void CopyMissingFolder(const std::string &source, const std::string &target)
{
  if (mkdir(target.c_str(), 0755) != 0 && errno != EEXIST)
  {
    FailStartup("Cannot create " + target + ": " + strerror(errno));
  }
  CopyMissingEntries(source, target);
}

static void CopyMissingFile(const std::string &source, const std::string &target)
{
  if (Exists(target)) return;
  // Data only: files inside a signed bundle are read-only, the copies must stay writable
  if (copyfile(source.c_str(), target.c_str(), NULL, COPYFILE_DATA) != 0)
  {
    FailStartup("Cannot copy " + source + " to " + target + ": " + strerror(errno));
  }
}

// Only adds what is missing, so saved games and edited settings survive an updated app
static void CopyMissingEntries(const std::string &sourceFolder, const std::string &targetFolder)
{
  DIR *directory = opendir(sourceFolder.c_str());
  if (directory == NULL)
  {
    FailStartup("Cannot read " + sourceFolder + ": " + strerror(errno));
  }
  while (struct dirent *entry = readdir(directory))
  {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    std::string source = sourceFolder + "/" + name;
    std::string target = targetFolder + "/" + name;
    if (IsDirectory(source))
    {
      CopyMissingFolder(source, target);
    }
    else
    {
      CopyMissingFile(source, target);
    }
  }
  closedir(directory);
}

void UseApplicationSupportFolderWhenBundled(void)
{
  std::string gameName = BundleName();
  if (gameName.empty()) return;
  std::string bundledGameFolder = TakeSdlPath(SDL_GetBasePath(), "application bundle") + BUNDLED_GAME_FOLDER;
  if (!IsDirectory(bundledGameFolder)) return;
  std::string supportFolder = TakeSdlPath(SDL_GetPrefPath(SUPPORT_FOLDER_ORGANIZATION, gameName.c_str()),
                                          "Application Support folder");
  CopyMissingEntries(bundledGameFolder, supportFolder);
  if (chdir(supportFolder.c_str()) != 0)
  {
    FailStartup("Cannot use " + supportFolder + ": " + strerror(errno));
  }
}
