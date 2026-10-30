#ifndef HEROSTATSDUMP_H
#define HEROSTATSDUMP_H

#include <string>
#include "HeroStats.h"

// The same layout whatever was included before: CSB.h leaves #pragma pack(1) switched on
#pragma pack(push, 8)

// Every non-zero number of the whole run as text, one line each: one block per champion in the
// order they were met, then PARTY. Champions are named; monster types, weapons, options and
// causes are the numbers the store keeps.
std::string HeroStatsDump_Text(const HeroStats &stats);

#pragma pack(pop)

#endif
