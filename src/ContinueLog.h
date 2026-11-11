#ifndef CONTINUELOG_H
#define CONTINUELOG_H

// File > Continue recording replays a recording to its end and records on into it. A replay
// that reaches an inventory "Save and Quit" would quit there too; before the replay, that last
// Save and Quit becomes what CSBwin's 2223 line does: the clicks of the disk menu are made
// comment lines, and a 2223 line takes the place of the save. Any other ending is left as it is.
// Engine-free; tested in automap_tests/ContinueLogTest.cpp.

#pragma pack(push)
#pragma pack()

#include <string>

std::string ContinueLog_Prepare(const std::string &log, bool *changed);

#pragma pack(pop)

#endif
