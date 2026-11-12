// Reads files off an AmigaDOS disk image (ADF, OFS or FFS), enough to pull
// DMGAME.DAT off a save disk.  Files are found by scanning for file-header
// blocks, which is all the flat save disks need.
#pragma once
#include <string>
#include <vector>
#include "Bytes.h"

namespace calc {

struct AdfFile {
  std::string name;
  size_t size = 0;
  int headerBlock = 0;
};

bool looksLikeAdf(const Bytes &image);
std::vector<AdfFile> adfFiles(const Bytes &image);          // throws FormatError
Bytes adfRead(const Bytes &image, const AdfFile &file);     // throws FormatError
// DMGAME.DAT first, then any other .DAT, then a .BAK; -1 if none
int adfPickSavegame(const std::vector<AdfFile> &files);

}  // namespace calc
