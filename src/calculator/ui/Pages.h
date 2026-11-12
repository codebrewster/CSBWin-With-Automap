// The five tabs.  Each listens to the session and recalculates, shortly
// after the last change and only while it is the one on show.
#pragma once
#include <QTimer>
#include <QWidget>
#include <functional>
#include <vector>

#include "../core/HeroAttack.h"
#include "../core/MonsterAttack.h"
#include "../core/Recovery.h"
#include "../core/Spells.h"
#include "Session.h"
#include "Widgets.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;
class QVBoxLayout;

namespace ui {

class Page : public QWidget {
  Q_OBJECT
 public:
  explicit Page(Session *session, QWidget *parent = nullptr);
 protected:
  virtual void recalculate() = 0;
  void showEvent(QShowEvent *event) override;
  void schedule();
  // The scrolling column the page's content goes into
  QVBoxLayout *column() const { return column_; }
  Session *s_;
 private:
  QTimer timer_;
  QVBoxLayout *column_;
  bool stale_ = true;
};

// A table cell that sorts by its number, not by its text
QTableWidgetItem *numberCell(double value, const QString &text);
QTableWidgetItem *textCell(const QString &text);
void styleTable(QTableWidget *table);
// Makes the table as tall as its rows, so it never scrolls inside the page
void fitTableHeight(QTableWidget *table);

class HeroAttackPage : public Page {
  Q_OBJECT
 public:
  explicit HeroAttackPage(Session *session, QWidget *parent = nullptr);
 protected:
  void recalculate() override;
 private:
  void showRow();
  QComboBox *scope_;
  QCheckBox *usable_, *drain_;
  QSpinBox *swings_;
  QTableWidget *table_;
  Tile *tHit_, *tDamage_, *tRange_, *tDps_, *tKill_, *tStamina_;
  Histogram *chart_;
  QLabel *note_;
  std::vector<calc::SwingResult> rows_;
  QString picked_;   // "weapon/action" of the row to keep selected
};

class MonsterAttackPage : public Page {
  Q_OBJECT
 public:
  explicit MonsterAttackPage(Session *session, QWidget *parent = nullptr);
 protected:
  void recalculate() override;
 private:
  QDoubleSpinBox *interval_;
  QSpinBox *distance_;
  Tile *tHit_, *tAvg_, *tRange_, *tDps_, *tShare_, *tKill_, *tWound_, *tPoison_, *tArmour_;
  Histogram *chart_;
  QTableWidget *parts_, *missiles_;
  QLabel *note_, *missileNote_, *missileHead_;
};

class SpellPage : public Page {
  Q_OBJECT
 public:
  explicit SpellPage(Session *session, QWidget *parent = nullptr);
 protected:
  void recalculate() override;
 private:
  void showCell();
  QComboBox *metric_;
  QSpinBox *distance_;
  QCheckBox *backRow_;
  QTableWidget *grid_;
  Tile *tAvg_, *tRange_, *tKill_, *tMana_, *tCast_, *tOthers_;
  Histogram *chart_;
  QLabel *note_;
  struct Cell {
    calc::Cast cast;
    calc::MissileState at;
    calc::MonsterHit hit;
  };
  std::vector<std::vector<Cell>> cells_;
};

class RecoveryPage : public Page {
  Q_OBJECT
 public:
  explicit RecoveryPage(Session *session, QWidget *parent = nullptr);
 protected:
  void recalculate() override;
 private:
  QSpinBox *clock_;
  QComboBox *mode_;
  QTableWidget *table_;
  LineChart *chart_;
  QLabel *note_, *foodNote_;
  bool loadingClock_ = false;
};

class WhatIfPage : public Page {
  Q_OBJECT
 public:
  explicit WhatIfPage(Session *session, QWidget *parent = nullptr);
 protected:
  void recalculate() override;
 private:
  void variablePicked();
  QComboBox *variable_, *result_;
  QSpinBox *from_, *to_, *power_;
  LineChart *chart_;
  QLabel *note_;
};

}  // namespace ui
