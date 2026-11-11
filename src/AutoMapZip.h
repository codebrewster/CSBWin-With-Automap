#ifndef AUTOMAPZIP_H
#define AUTOMAPZIP_H

#include <string>
#include <vector>

// The .map file is a zip archive: its first entry is the map data AutoMapFile writes, a second
// one named "statistics" the hero statistics AutoMapStatsFile writes. A file written before the
// map was zipped holds the map data bare; Unpack tells the two apart by their first bytes, so
// both load. Pure functions over bytes in memory, built on miniz.

struct AutoMapZipEntry
{
  std::string name;
  std::string bytes;
  // Unpack only: false for an entry whose data is damaged; its bytes are then empty
  bool isReadable;
};

// The archive holding the entries in their order (isReadable is not looked at). Empty when miniz fails.
std::string AutoMapZip_Pack(const std::vector<AutoMapZipEntry> &entries);

enum AutoMapZipUnpackResult
{
  AutoMapZipUnpacked,   // file was an archive: *entries holds its entries in their order
  AutoMapZipWasBare,    // file is no archive: *entries holds one entry, with no name, the file as it is
  AutoMapZipIsBroken    // file starts like an archive but cannot be opened, or its first entry cannot be read
};

// A damaged entry after the first one leaves the others readable: it comes out with isReadable false
AutoMapZipUnpackResult AutoMapZip_Unpack(const std::string &file, std::vector<AutoMapZipEntry> *entries);

// The entry of that name, or NULL
const AutoMapZipEntry *AutoMapZip_Find(const std::vector<AutoMapZipEntry> &entries, const std::string &name);

#endif
