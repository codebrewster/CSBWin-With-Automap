#ifndef AUTOMAPVISIBILITYCASES_H
#define AUTOMAPVISIBILITYCASES_H

#include <string>
#include <vector>

// The case file of the visibility tests (csb --visibility-test): where the party stands,
// in which light, and what the map must count as seen there. Coordinates are global, as the map
// shows them: L<level> (<x>,<y>).
//
//   save csb-Visibility-test
//   at L0 (12,7) facing N light 4
//     decoration (12,5) S   seen        # a comment
//     item (12,6)           glimpsed    # the tile's items together: any of them
//     item (12,6) "Dagger"  seen        # one item, by the name the game shows
//     item (2,12) SE "Torch" not seen   # its corner, when the name alone is not enough
//     monster (13,5)        not seen
//     door (12,3)           seen
//     alcove (4,18) N       seen        # what the alcove in the wall's north face holds
//     tile (4,17)           seen        # the cell itself, floor or wall, as the fog shows it
//     floor (5,2)           not seen    # the cell's floor picture: a puddle, moss, a pad
//     portrait (10,3) S     seen        # a champion's face in its mirror, not just the frame
//
// A case without checks is a survey: the run prints what the frame drew instead.
// Another save line starts a section of its own, with the cases below it drawn in that save.

enum AutoMapVisibilityThing
{
  AutoMapVisibilityDecoration,
  AutoMapVisibilityItem,
  AutoMapVisibilityMonster,
  AutoMapVisibilityDoor,
  AutoMapVisibilityAlcove,
  AutoMapVisibilityTile,
  AutoMapVisibilityFloor,
  AutoMapVisibilityPortrait
};

enum AutoMapVisibilityVerdict
{
  AutoMapVisibilityNotSeen,
  AutoMapVisibilityGlimpsed,
  AutoMapVisibilitySeen
};

struct AutoMapVisibilityCheck
{
  int lineNumber;
  AutoMapVisibilityThing thing;
  int x;
  int y;
  // A decoration's or an alcove's wall face, an AutoMapDirection; -1 for the other things
  int side;
  // An item's corner of the tile, the engine's 0 north-west, 1 north-east, 2 south-east,
  // 3 south-west, and its name; -1 and empty to judge the tile's items together
  int corner;
  std::string itemName;
  AutoMapVisibilityVerdict expected;
};

struct AutoMapVisibilityCase
{
  int lineNumber;
  int level;
  int x;
  int y;
  int facing;
  // The palette index, as the map's debug light: 0 full light .. 5 darkest
  int light;
  std::vector<AutoMapVisibilityCheck> checks;
};

// A save line and the cases below it, up to the next save line
struct AutoMapVisibilitySection
{
  int lineNumber;
  std::string save;
  std::vector<AutoMapVisibilityCase> cases;
};

struct AutoMapVisibilityCases
{
  std::vector<AutoMapVisibilitySection> sections;
};

// False with *error naming the line when the text is not a case file
bool AutoMapVisibilityCases_Parse(const std::string &text, AutoMapVisibilityCases *cases,
                                  std::string *error);
// The words of the file: "decoration", "seen", "not seen", "N" ...
const char *AutoMapVisibilityCases_ThingName(AutoMapVisibilityThing thing);
const char *AutoMapVisibilityCases_VerdictName(AutoMapVisibilityVerdict verdict);
const char *AutoMapVisibilityCases_SideName(int side);
const char *AutoMapVisibilityCases_CornerName(int corner);
// A check line as the file writes it, without indentation: "decoration (12,5) S seen",
// "alcove (4,18) N not seen",
// "item (2,12) SE "Torch" not seen"
std::string AutoMapVisibilityCases_CheckLine(const AutoMapVisibilityCheck &check,
                                             AutoMapVisibilityVerdict verdict);

#endif
