// Colours and the style sheet of the calculator, light and dark.
#pragma once
#include <QColor>
#include <QString>

class QApplication;

namespace ui {

struct Theme {
  bool dark = false;
  QColor window, surface, raised, border, text, muted, accent, accentSoft, good, warn, bad;
  QColor series[5];   // chart lines: hit points, stamina, mana, food, water
};

const Theme &theme();
// Follows the system's colour scheme until the user picks one
void applyTheme(QApplication &app, bool dark);
bool systemPrefersDark();

}  // namespace ui
