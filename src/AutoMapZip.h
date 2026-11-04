#ifndef AUTOMAPZIP_H
#define AUTOMAPZIP_H

#include <string>

// The .map file is a zip archive holding one entry, the map data AutoMapFile writes. A file
// written before the map was zipped holds that data bare; Unpack tells the two apart by their
// first bytes, so both load. Pure functions over bytes in memory, built on miniz.

// The archive with bytes as its one entry, under entryName. Empty when miniz fails.
std::string AutoMapZip_Pack(const std::string &entryName, const std::string &bytes);

enum AutoMapZipUnpackResult
{
  AutoMapZipUnpacked,   // file was an archive: *bytes holds its first entry
  AutoMapZipWasBare,    // file is no archive: *bytes holds file as it is
  AutoMapZipIsBroken    // file starts like an archive but its first entry cannot be read
};

AutoMapZipUnpackResult AutoMapZip_Unpack(const std::string &file, std::string *bytes);

#endif
