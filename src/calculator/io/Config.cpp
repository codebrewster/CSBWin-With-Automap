#include "Config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace calc {

bool readRulesFromConfig(const std::string &path, Rules &rules)
{
  FILE *f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  bool nostalgia = false, vulnerable = rules.vulnerableOnAttack;
  int launcher = rules.shotDamageFactor;
  char line[512];
  while (std::fgets(line, sizeof line, f)) {
    char key[128];
    int value = 0;
    if (std::sscanf(line, " %127[A-Za-z_] = %d", key, &value) != 2) continue;
    if (!std::strcmp(key, "nostalgia_mode")) nostalgia = value != 0;
    else if (!std::strcmp(key, "monsters_vulnerable_on_attack")) vulnerable = value != 0;
    else if (!std::strcmp(key, "launcher_power")) launcher = value;
  }
  std::fclose(f);
  // Nostalgia Mode plays by the classic rules whatever else is set (GameRules.cpp)
  rules.vulnerableOnAttack = !nostalgia && vulnerable;
  rules.shotDamageFactor = nostalgia ? 1 : (launcher < 1 ? 1 : (launcher > 6 ? 6 : launcher));
  return true;
}

}  // namespace calc
