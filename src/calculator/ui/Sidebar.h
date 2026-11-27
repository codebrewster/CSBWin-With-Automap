// The inputs every tab shares: the hero, the party, the monster, the rules.
#pragma once
#include <QWidget>
#include <vector>
#include "Session.h"

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace ui {

class IntField;

class Sidebar : public QWidget {
  Q_OBJECT
 public:
  explicit Sidebar(Session *session, QWidget *parent = nullptr);

 private:
  void buildHero(QVBoxLayout *column);
  void buildMonster(QVBoxLayout *column);
  void buildRules(QVBoxLayout *column);
  void refill();        // the choices that depend on the savegame and the tables
  void fillGroups();

  Session *s_;
  QComboBox *heroBox_ = nullptr, *weaponBox_ = nullptr, *armourBox_[6] = {};
  QComboBox *levelBox_ = nullptr, *groupBox_ = nullptr, *monsterBox_ = nullptr, *kindBox_ = nullptr;
  QComboBox *tablesBox_ = nullptr;
  QPushButton *restoreButton_ = nullptr;
  QLabel *heroNote_ = nullptr, *monsterNote_ = nullptr, *tablesNote_ = nullptr;
  std::vector<IntField *> masteryFields_;   // refreshed when the weapon changes them
  bool filling_ = false;
};

}  // namespace ui
