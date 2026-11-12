// What the window is working on: the game's tables, an optional savegame,
// and the hero, party, monster and rules every tab calculates with.  The
// side panels change these values; the tabs listen and recalculate.
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "../core/Model.h"
#include "../io/SaveGame.h"

namespace ui {

enum TableSource { TablesFromFile, TablesDungeonMaster, TablesChaosStrikesBack };

class Session : public QObject {
  Q_OBJECT
 public:
  explicit Session(QObject *parent = nullptr);

  calc::GameData data;
  int tableSource = TablesDungeonMaster;
  QString graphicsPath;                  // the graphics.dat found beside the savegame

  std::unique_ptr<calc::SaveGame> save;
  QString savePath;
  int heroIndex = -1;                    // into save->heroes
  int levelIndex = -1;                   // into save->levels
  int groupIndex = -1;                   // into that level's groups

  calc::Hero hero, heroBase;             // heroBase = as loaded, for "changed" marks
  calc::Party party, partyBase;
  calc::Target target, targetBase;
  calc::Rules rules;
  unsigned gameTime = 0;
  bool hasBase = false;                  // the base values come from a savegame

  // Throws nothing: problems come back as text (empty = fine)
  QString openSave(const QString &path);
  void closeSave();
  void selectHero(int index);
  void selectTables(int source);
  void selectLevel(int index);           // -1 = no savegame level
  void selectGroup(int index);           // -1 = a fresh monster of the chosen type
  void selectMonsterType(int type);
  void resetMonsterHp();
  QStringList warnings() const;
  QString summary() const;               // one line for the top bar

  // A value changed: recalculate
  void touch() { emit changed(); }

 signals:
  void changed();    // values
  void reloaded();   // the lists behind the choices changed too (savegame, tables)

 private:
  void applyTables();
  void refit();      // after new tables: keep ids inside them
};

}  // namespace ui
