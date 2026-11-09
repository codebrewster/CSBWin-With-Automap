#pragma once
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <memory>
#include <vector>
#include "DamageExact.h"
#include "DamageMonteCarlo.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;

struct RunOutput {
  std::vector<ComboResult> exact, mc;
  double secExact = 0, secMc = 0;
  bool cancelled = false;
  int mode = 0;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow();
  // test hook: run once with the given mode/trials, save a screenshot, quit
  void autoRun(int mode, double trialsM, const QString &png);

 private slots:
  void onRun();
  void onCancel();
  void onFinished();
  void onTick();
  void onGraph();
  void onLoadSave();
  void onChampion(int index);
  void onShowInventory();
  void onMonster();
  void onModeChanged();
  void updateInfo();
  void selectAll(bool on);

 private:
  QSpinBox *spin(int lo, int hi, int v);
  void buildUi();
  Character character() const;
  Monster monster() const;
  Options options() const;
  std::vector<std::pair<int, int>> selectedCombos() const;
  void showTable(const std::vector<ComboResult> &rows, const QString &tag);
  void showCompare(const RunOutput &out);
  void setBusy(bool busy);

  // character
  QSpinBox *sStr, *sDex, *sLuck, *sLuckMin, *sLuckMax, *sLoad, *sSta, *sMaxSta;
  QCheckBox *cBoots;
  QCheckBox *cWound[6];
  QMap<int, QSpinBox *> sMastery;
  QComboBox *cChampion;
  QLabel *lParty;
  // monster
  QComboBox *cMonster;
  QLineEdit *eMonName;
  QSpinBox *sArmor, *sMonDex, *sLevel, *sMonExp;
  QCheckBox *cNonMat;
  QLabel *lHp;
  // run settings
  QComboBox *cMode;
  QSpinBox *sThreads, *sFight;
  QSpinBox *sTrialsK;
  QSpinBox *sSeed;
  QCheckBox *cDrain, *cDrift, *cBias, *cAttacking;
  QPushButton *bRun, *bCancel, *bGraph;
  QProgressBar *progress;
  // weapons
  QTreeWidget *wTree;
  // results
  QStackedWidget *stack;
  QTreeWidget *table, *compare;
  QLabel *lStatus, *lWeight;

  QJsonArray champions_;
  QJsonObject slotNames_;
  int partyMult_ = -1;
  QString lastDir_;

  QFutureWatcher<RunOutput> watcher_;
  McProgress mcProgress_;
  QTimer timer_;
  uint64_t totalWork_ = 0;
  Character runChar_;
  Monster runMon_;
  Options runOpt_;
  int64_t runTrials_ = 0;
  qint64 startMs_ = 0;
  QString grabPath_;
};
