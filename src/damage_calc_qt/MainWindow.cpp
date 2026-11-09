#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <set>

namespace {

enum Mode { ModeExact = 0, ModeMonteCarlo = 1, ModeCompare = 2 };

// tree item that sorts numerically on the value kept in Qt::UserRole
class NumItem : public QTreeWidgetItem {
 public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &o) const override
  {
    int c = treeWidget() ? treeWidget()->sortColumn() : 0;
    if (c == 0) return text(0) < o.text(0);
    return data(c, Qt::UserRole).toDouble() < o.data(c, Qt::UserRole).toDouble();
  }
};

QString fmt(double v, int prec = 2) { return QString::number(v, 'f', prec); }

// plain line plot (DPS vs strength)
class GraphWidget : public QWidget {
 public:
  struct Pt { int x; double a, b; };
  GraphWidget(std::vector<Pt> pts, QString title)
      : pts_(std::move(pts)), title_(std::move(title))
  {
    setMinimumSize(640, 420);
  }

 protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    const int ML = 70, MR = 20, MT = 40, MB = 50;
    int W = width(), H = height();
    int x0 = pts_.front().x, x1 = pts_.back().x;
    double ymax = 1;
    for (auto &q : pts_) ymax = std::max({ymax, q.a, q.b});
    ymax *= 1.05;
    auto px = [&](double x) { return ML + (x - x0) / std::max(x1 - x0, 1) * (W - ML - MR); };
    auto py = [&](double y) { return H - MB - y / ymax * (H - MT - MB); };
    p.setPen(QColor(160, 160, 160, 120));
    for (int i = 0; i <= 5; i++) {
      double yv = ymax * i / 5;
      p.drawLine(QPointF(px(x0), py(yv)), QPointF(px(x1), py(yv)));
      p.setPen(palette().text().color());
      p.drawText(QRectF(0, py(yv) - 10, ML - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                 fmt(yv, 1));
      p.setPen(QColor(160, 160, 160, 120));
    }
    int step = std::max(1, (x1 - x0) / 10);
    for (int x = x0; x <= x1; x += step) {
      p.setPen(QColor(160, 160, 160, 120));
      p.drawLine(QPointF(px(x), py(0)), QPointF(px(x), py(ymax)));
      p.setPen(palette().text().color());
      p.drawText(QRectF(px(x) - 20, py(0) + 4, 40, 18), Qt::AlignCenter, QString::number(x));
    }
    p.setPen(palette().text().color());
    p.drawText(QRectF(0, 8, W, 22), Qt::AlignCenter, title_);
    p.drawText(QRectF(0, H - 22, W, 20), Qt::AlignCenter, "strength");
    p.save();
    p.translate(16, H / 2.0);
    p.rotate(-90);
    p.drawText(QRectF(-40, -10, 80, 20), Qt::AlignCenter, "dps");
    p.restore();
    for (int k = 0; k < 2; k++) {
      QColor col = k == 0 ? QColor(40, 90, 220) : QColor(220, 50, 50);
      p.setPen(QPen(col, 2));
      QPolygonF poly;
      for (auto &q : pts_) poly << QPointF(px(q.x), py(k == 0 ? q.a : q.b));
      p.drawPolyline(poly);
      p.fillRect(QRectF(W - 190, 44 + 22 * k, 20, 12), col);
      p.setPen(palette().text().color());
      p.drawText(QPointF(W - 162, 55 + 22 * k), k == 0 ? "barefoot" : "elven boots");
    }
  }

 private:
  std::vector<Pt> pts_;
  QString title_;
};

}  // namespace

// --------------------------------------------------------------------- UI
QSpinBox *MainWindow::spin(int lo, int hi, int v)
{
  auto *s = new QSpinBox;
  s->setRange(lo, hi);
  s->setValue(v);
  s->setGroupSeparatorShown(false);
  return s;
}

MainWindow::MainWindow()
{
  setWindowTitle("Damage Calculator (Qt) - CSB");
  buildUi();
  connect(&watcher_, &QFutureWatcher<RunOutput>::finished, this, &MainWindow::onFinished);
  connect(&timer_, &QTimer::timeout, this, &MainWindow::onTick);
  timer_.setInterval(100);
  onMonster();
  onModeChanged();
  updateInfo();
  resize(1500, 800);
}

void MainWindow::buildUi()
{
  Character dc = defaultCharacter();
  auto *central = new QWidget;
  auto *root = new QVBoxLayout(central);

  // ---------------------------------------------------------- top bar
  auto *top = new QGridLayout;
  cMode = new QComboBox;
  cMode->addItems({"Exact", "Monte Carlo", "Compare exact vs Monte Carlo"});
  cMode->setCurrentIndex(ModeCompare);
  sTrialsK = spin(1, 2000000000, 20000);
  sTrialsK->setSuffix(" thousand");
  sTrialsK->setGroupSeparatorShown(true);
  sTrialsK->setToolTip("Trials = swings per weapon+attack (20000 thousand = 20 million)");
  sSeed = spin(0, 2000000000, 12345);
  sThreads = spin(1, 1024, std::max(1, QThread::idealThreadCount()));
  cDrain = new QCheckBox("Stamina drain");
  sFight = spin(0, 100000, 50);
  sFight->setToolTip(
      "Swings per fight. Stamina (with drain) and luck (with luck drift) are\n"
      "restored at the start of every fight. 0 = never: one long sequence per\n"
      "combo (the Monte Carlo can then use only one core per combo).");
  cDrift = new QCheckBox("Luck drifts");
  cDrift->setChecked(true);
  cDrift->setToolTip(
      "IsCharacterLucky() moves the character's Luck by -2 / +2 on every call\n"
      "(Character.cpp:2185), within Luck min..max. Off = luck stays fixed.");
  cBias = new QCheckBox("STRandom modulo bias");
  cBias->setChecked(true);
  cBias->setToolTip(
      "The game computes STRandom(n) as (r & 0xffff) % n, which is very slightly\n"
      "non-uniform unless n divides 65536. Off = ideal uniform rolls.");
  cAttacking = new QCheckBox("Monster is attacking");
  cAttacking->setToolTip(
      "monsters_vulnerable_on_attack: the monster's armour roll is halved while\n"
      "it is attacking (Attack.cpp:671).");
  bRun = new QPushButton("Run");
  bCancel = new QPushButton("Cancel");
  bCancel->setEnabled(false);
  bGraph = new QPushButton("Graph DPS vs Str");
  auto *bAll = new QPushButton("Select all");
  auto *bNone = new QPushButton("Clear");
  progress = new QProgressBar;
  progress->setRange(0, 1000);
  progress->setVisible(false);
  int c = 0;
  top->addWidget(new QLabel("Method:"), 0, c++);
  top->addWidget(cMode, 0, c++);
  top->addWidget(new QLabel("Trials:"), 0, c++);
  top->addWidget(sTrialsK, 0, c++);
  top->addWidget(new QLabel("Seed:"), 0, c++);
  top->addWidget(sSeed, 0, c++);
  top->addWidget(new QLabel("Threads:"), 0, c++);
  top->addWidget(sThreads, 0, c++);
  top->addWidget(bAll, 0, c++);
  top->addWidget(bNone, 0, c++);
  top->addWidget(bGraph, 0, c++);
  top->addWidget(bRun, 0, c++);
  top->addWidget(bCancel, 0, c++);
  top->setColumnStretch(c, 1);
  auto *top2 = new QHBoxLayout;
  top2->addWidget(cDrain);
  top2->addWidget(new QLabel("Swings/fight:"));
  top2->addWidget(sFight);
  top2->addWidget(cDrift);
  top2->addWidget(cBias);
  top2->addWidget(cAttacking);
  top2->addStretch(1);
  root->addLayout(top);
  root->addLayout(top2);
  root->addWidget(progress);

  // -------------------------------------------------------- character
  auto *charBox = new QWidget;
  auto *cl = new QVBoxLayout(charBox);
  auto *loadRow = new QHBoxLayout;
  auto *bLoad = new QPushButton("Load from save...");
  cChampion = new QComboBox;
  cChampion->setEnabled(false);
  cChampion->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  cChampion->setMinimumContentsLength(14);
  loadRow->addWidget(bLoad);
  loadRow->addWidget(cChampion, 1);
  cl->addLayout(loadRow);
  lParty = new QLabel;
  cl->addWidget(lParty);
  auto *bInv = new QPushButton("Show inventory...");
  cl->addWidget(bInv);
  auto *form = new QFormLayout;
  sStr = spin(0, 2000, dc.strength);
  sDex = spin(0, 2000, dc.dexterity);
  sLuck = spin(0, 2000, dc.luck);
  sLuckMin = spin(0, 255, dc.luckMin);
  sLuckMax = spin(0, 255, dc.luckMax);
  sLoad = spin(0, 5000, dc.load);
  sSta = spin(0, 5000, dc.stamina);
  sMaxSta = spin(1, 5000, dc.maxStamina);
  form->addRow("Strength", sStr);
  form->addRow("Dexterity", sDex);
  form->addRow("Luck", sLuck);
  form->addRow("Luck min", sLuckMin);
  form->addRow("Luck max", sLuckMax);
  form->addRow("Load", sLoad);
  form->addRow("Stamina", sSta);
  form->addRow("Max stamina", sMaxSta);
  cl->addLayout(form);
  auto *wounds = new QGroupBox("Wounds");
  auto *wg = new QGridLayout(wounds);
  const char *wn[6] = {"ready hand", "action hand", "head", "torso", "legs", "feet"};
  for (int i = 0; i < 6; i++) {
    cWound[i] = new QCheckBox(wn[i]);
    wg->addWidget(cWound[i], i / 2, i % 2);
  }
  wounds->setToolTip(
      "Any wound lowers MaxLoad (-1/8, legs -1/4); the action hand halves the\n"
      "throwing distance (Code11f52.cpp:4451, Character.cpp:2358).");
  cl->addWidget(wounds);
  cBoots = new QCheckBox("Elven Boots");
  cl->addWidget(cBoots);
  std::set<int> keys = {4, 5, 6, 7, 8, 9, 10, 11};
  for (auto &a : attacks()) keys.insert(a.skill);
  auto *mf = new QFormLayout;
  for (int k : keys) {
    sMastery[k] = spin(0, 30, dc.mastery[k]);
    mf->addRow(QString("skill %1").arg(k), sMastery[k]);
  }
  auto *mbox = new QGroupBox("Skill masteries");
  mbox->setLayout(mf);
  cl->addWidget(mbox);
  // monster
  auto *mon = new QGroupBox("Monster");
  auto *ml = new QFormLayout(mon);
  cMonster = new QComboBox;
  for (auto &m : monsters()) cMonster->addItem(QString::fromStdString(m.label));
  eMonName = new QLineEdit;
  sArmor = spin(0, 999, 0);
  sMonDex = spin(0, 999, 0);
  sLevel = spin(0, 9, 1);
  sMonExp = spin(0, 15, 0);
  cNonMat = new QCheckBox("Non-material");
  lHp = new QLabel;
  auto *bMult = new QPushButton("Use savegame level mult");
  ml->addRow(cMonster);
  ml->addRow("Name", eMonName);
  ml->addRow("Armor", sArmor);
  ml->addRow("Dex", sMonDex);
  ml->addRow("Level diff.", sLevel);
  ml->addRow("Experience", sMonExp);
  ml->addRow(cNonMat);
  ml->addRow(lHp);
  ml->addRow(bMult);
  cl->addWidget(mon);
  cl->addStretch(1);
  auto *charScroll = new QScrollArea;
  charScroll->setWidget(charBox);
  charScroll->setWidgetResizable(true);
  charScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  charScroll->setMinimumWidth(360);

  // --------------------------------------------------------- weapons
  wTree = new QTreeWidget;
  wTree->setHeaderLabel("Weapons & attack methods");
  wTree->setMinimumWidth(300);
  QFont bold = font();
  bold.setBold(true);
  for (size_t wi = 0; wi < weapons().size(); wi++) {
    const Weapon &w = weapons()[wi];
    QString wt = QString("%1  (str+%2, wt=%3%4)").arg(w.name).arg(w.ubyte2).arg(w.weight)
                     .arg(w.armorDiv ? QString(", armor/-%1").arg(w.armorDiv)
                                     : (w.vorpal ? " (vorpal)" : ""));
    auto *wi_ = new QTreeWidgetItem(wTree, QStringList(wt));
    wi_->setFont(0, bold);
    wi_->setFlags(wi_->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    wi_->setCheckState(0, Qt::Unchecked);
    wi_->setData(0, Qt::UserRole, int(wi));
    for (int ai : weaponAttacks()[wi]) {
      const Attack &a = attacks()[size_t(ai)];
      QString at = QString("%1  (P7=%2 P8=%3 skill=%4 t=%5 e=%6)%7").arg(a.name).arg(a.p7)
                       .arg(a.p8).arg(a.skill).arg(a.time).arg(a.energy)
                       .arg(a.disrupt ? " [disrupt]" : "");
      auto *it = new QTreeWidgetItem(wi_, QStringList(at));
      it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
      it->setCheckState(0, Qt::Unchecked);
      it->setData(0, Qt::UserRole, ai);
    }
    wi_->setExpanded(true);
  }
  for (auto &dcb : defaultCombos()) {
    auto *wi_ = wTree->topLevelItem(dcb.first);
    for (int k = 0; k < wi_->childCount(); k++)
      if (wi_->child(k)->data(0, Qt::UserRole).toInt() == dcb.second)
        wi_->child(k)->setCheckState(0, Qt::Checked);
  }

  // --------------------------------------------------------- results
  auto *res = new QWidget;
  auto *rl = new QVBoxLayout(res);
  lStatus = new QLabel("Tick weapons/attacks, then Run.");
  lStatus->setWordWrap(true);
  lWeight = new QLabel;
  lWeight->setWordWrap(true);
  rl->addWidget(lStatus);
  rl->addWidget(lWeight);
  stack = new QStackedWidget;
  table = new QTreeWidget;
  table->setRootIsDecorated(false);
  table->setSortingEnabled(true);
  table->setHeaderLabels({"combo", "hit%", "dmg/try", "dmg/hit", "min_hit", "max_hit",
                          "dps", "dmg/sta", "sta/try", "xp/try", "xps", "end_stam"});
  for (int i = 1; i < 12; i++)
    table->headerItem()->setTextAlignment(i, Qt::AlignRight);
  table->header()->setSectionResizeMode(QHeaderView::Interactive);
  table->setColumnWidth(0, 190);
  compare = new QTreeWidget;
  compare->setHeaderLabels({"combo / quantity", "exact", "Monte Carlo", "difference",
                            "std. error", "z", ""});
  compare->setColumnWidth(0, 230);
  stack->addWidget(table);
  stack->addWidget(compare);
  rl->addWidget(stack, 1);

  auto *split = new QSplitter;
  split->addWidget(charScroll);
  split->addWidget(wTree);
  split->addWidget(res);
  split->setStretchFactor(2, 1);
  split->setSizes({370, 340, 800});
  root->addWidget(split, 1);
  setCentralWidget(central);

  // ---------------------------------------------------------- signals
  connect(bRun, &QPushButton::clicked, this, &MainWindow::onRun);
  connect(bCancel, &QPushButton::clicked, this, &MainWindow::onCancel);
  connect(bGraph, &QPushButton::clicked, this, &MainWindow::onGraph);
  connect(bLoad, &QPushButton::clicked, this, &MainWindow::onLoadSave);
  connect(bInv, &QPushButton::clicked, this, &MainWindow::onShowInventory);
  connect(bAll, &QPushButton::clicked, this, [this] { selectAll(true); });
  connect(bNone, &QPushButton::clicked, this, [this] { selectAll(false); });
  connect(bMult, &QPushButton::clicked, this, [this] {
    if (partyMult_ < 0) QMessageBox::information(this, "No savegame", "Load a savegame first.");
    else sLevel->setValue(partyMult_);
  });
  connect(cChampion, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onChampion);
  connect(cMonster, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { onMonster(); });
  connect(cMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { onModeChanged(); });
  connect(cDrain, &QCheckBox::toggled, this, [this] { onModeChanged(); });
  connect(cDrift, &QCheckBox::toggled, this, [this] { onModeChanged(); });
  for (QSpinBox *s : {sStr, sSta, sMaxSta}) connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { updateInfo(); });
  for (QSpinBox *s : {sLevel}) connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { updateInfo(); });
  for (auto *w : cWound) connect(w, &QCheckBox::toggled, this, [this] { updateInfo(); });
  connect(cBoots, &QCheckBox::toggled, this, [this] { updateInfo(); });
}

void MainWindow::autoRun(int mode, double trialsM, const QString &png)
{
  grabPath_ = png;
  show();
  if (mode == 3) {   // graph of the first ticked combo
    selectAll(false);
    wTree->topLevelItem(3)->child(0)->setCheckState(0, Qt::Checked);
    onGraph();
    QTimer::singleShot(300, this, [this] {
      for (QDialog *d : findChildren<QDialog *>()) d->grab().save(grabPath_);
      QApplication::quit();
    });
    return;
  }
  cMode->setCurrentIndex(mode);
  sTrialsK->setValue(int(trialsM * 1000));
  QTimer::singleShot(0, this, &MainWindow::onRun);
}

void MainWindow::selectAll(bool on)
{
  for (int i = 0; i < wTree->topLevelItemCount(); i++)
    wTree->topLevelItem(i)->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
}

void MainWindow::onModeChanged()
{
  sSeed->setEnabled(cMode->currentIndex() != ModeExact);
  sThreads->setEnabled(true);
  sFight->setEnabled(cDrain->isChecked() || cDrift->isChecked());
  bGraph->setEnabled(true);
}

void MainWindow::onMonster()
{
  int i = cMonster->currentIndex();
  if (i < 0) return;
  const Monster &m = monsters()[size_t(i)];
  eMonName->setText(QString::fromStdString(m.name));
  sArmor->setValue(m.armor);
  sMonDex->setValue(m.dex);
  sLevel->setValue(m.level);
  sMonExp->setValue(m.exp);
  cNonMat->setChecked(m.nonMaterial);
  updateInfo();
}

void MainWindow::updateInfo()
{
  Character ch = character();
  int ml = eng::maxLoad(ch, std::min(ch.stamina, ch.maxStamina));
  int d5 = ml / 16;
  int w2 = (d5 - 12) / 2 + d5;
  lWeight->setText(QString("MaxLoad=%1: full weight bonus ≤ %2 (%3 kg), no penalty "
                           "≤ %4 (%5 kg); heavier weapons lose power.")
                       .arg(ml).arg(d5).arg(d5 / 10.0, 0, 'f', 1).arg(w2).arg(w2 / 10.0, 0, 'f', 1));
  int i = cMonster->currentIndex();
  if (i >= 0) {
    int base = monsters()[size_t(i)].baseHealth, mult = sLevel->value();
    lHp->setText(QString("HP %1-%2 (base %3 x mult %4)")
                     .arg(base * mult).arg(base * mult + base / 4).arg(base).arg(mult));
  }
}

Character MainWindow::character() const
{
  Character c;
  c.strength = sStr->value();
  c.dexterity = sDex->value();
  c.luck = sLuck->value();
  c.luckMin = sLuckMin->value();
  c.luckMax = sLuckMax->value();
  c.load = sLoad->value();
  c.stamina = sSta->value();
  c.maxStamina = std::max(1, sMaxSta->value());
  c.elvenBoots = cBoots->isChecked();
  for (int i = 0; i < 6; i++)
    if (cWound[i]->isChecked()) c.wounds |= 1 << i;
  for (auto it = sMastery.begin(); it != sMastery.end(); ++it) c.mastery[it.key()] = it.value()->value();
  return c;
}

Monster MainWindow::monster() const
{
  return Monster{"", eMonName->text().toStdString(), sArmor->value(),
                 sMonDex->value(), sLevel->value(), 0, sMonExp->value(),
                 cNonMat->isChecked()};
}

Options MainWindow::options() const
{
  Options o;
  o.staminaDrain = cDrain->isChecked();
  o.luckDrift = cDrift->isChecked();
  o.moduloBias = cBias->isChecked();
  o.monsterAttacking = cAttacking->isChecked();
  o.swingsPerFight = sFight->value();
  return o;
}

std::vector<std::pair<int, int>> MainWindow::selectedCombos() const
{
  std::vector<std::pair<int, int>> out;
  for (int i = 0; i < wTree->topLevelItemCount(); i++) {
    auto *w = wTree->topLevelItem(i);
    for (int k = 0; k < w->childCount(); k++)
      if (w->child(k)->checkState(0) == Qt::Checked)
        out.push_back({w->data(0, Qt::UserRole).toInt(),
                       w->child(k)->data(0, Qt::UserRole).toInt()});
  }
  return out;
}

// ------------------------------------------------------------------- run
void MainWindow::setBusy(bool busy)
{
  bRun->setEnabled(!busy);
  bCancel->setEnabled(busy);
  bGraph->setEnabled(!busy);
  progress->setVisible(busy);
  if (busy) progress->setValue(0);
}

void MainWindow::onRun()
{
  auto combos = selectedCombos();
  if (combos.empty()) {
    QMessageBox::warning(this, "Nothing selected", "Tick at least one weapon attack.");
    return;
  }
  runChar_ = character();
  runOpt_ = options();
  runMon_ = monster();
  int64_t trials = int64_t(sTrialsK->value()) * 1000;
  const int F = runOpt_.swingsPerFight;
  const bool stateful = runOpt_.staminaDrain || runOpt_.luckDrift;
  if (stateful && F > 0 && trials % F) trials += F - trials % F;   // whole fights
  runTrials_ = trials;
  if (stateful && F > 0) sTrialsK->setValue(int((trials + 999) / 1000));
  const int mode = cMode->currentIndex();
  int threads = sThreads->value();
  McParams mp;
  mp.ch = runChar_;
  mp.mon = runMon_;
  mp.opt = runOpt_;
  mp.trials = trials;
  mp.seed = uint64_t(sSeed->value());
  mp.threads = threads;
  ExactParams ep;
  ep.ch = runChar_;
  ep.mon = runMon_;
  ep.opt = runOpt_;
  ep.trials = trials;
  mcProgress_.done = 0;
  mcProgress_.cancel = false;
  totalWork_ = mode == ModeExact ? 0 : uint64_t(trials) * combos.size();
  lStatus->setText(QString("Running %1 combos x %2 trials on %3 threads ...")
                       .arg(combos.size()).arg(trials).arg(threads));
  setBusy(true);
  startMs_ = QDateTime::currentMSecsSinceEpoch();
  timer_.start();
  McProgress *pp = &mcProgress_;
  watcher_.setFuture(QtConcurrent::run([=]() {
    RunOutput out;
    out.mode = mode;
    auto now = [] { return QDateTime::currentMSecsSinceEpoch(); };
    if (mode == ModeExact || mode == ModeCompare) {
      qint64 t0 = now();
      out.exact = exactAll(ep, combos, threads);
      out.secExact = (now() - t0) / 1000.0;
    }
    if (mode == ModeMonteCarlo || mode == ModeCompare) {
      qint64 t0 = now();
      out.mc = runMonteCarlo(mp, combos, pp);
      out.secMc = (now() - t0) / 1000.0;
      out.cancelled = pp->cancel.load();
    }
    return out;
  }));
}

void MainWindow::onCancel()
{
  mcProgress_.cancel = true;
}

void MainWindow::onTick()
{
  if (!totalWork_) return;
  double f = double(mcProgress_.done.load()) / double(totalWork_);
  progress->setValue(int(std::min(1.0, f) * 1000));
  double sec = (QDateTime::currentMSecsSinceEpoch() - startMs_) / 1000.0;
  if (f > 0.02 && sec > 0.5)
    progress->setFormat(QString("%p%   %1 M swings/s").arg(mcProgress_.done.load() / sec / 1e6, 0, 'f', 0));
}

void MainWindow::onFinished()
{
  timer_.stop();
  setBusy(false);
  RunOutput out = watcher_.result();
  if (out.cancelled) {
    lStatus->setText("Cancelled.");
    return;
  }
  QString extra;
  if (out.mode != ModeMonteCarlo) extra += QString("exact %1 s").arg(out.secExact, 0, 'f', 2);
  if (out.mode != ModeExact) {
    double n = double(runTrials_) * double(out.mc.size());
    extra += QString("%1Monte Carlo %2 s (%3 M swings/s, %4 threads)")
                 .arg(extra.isEmpty() ? "" : ", ").arg(out.secMc, 0, 'f', 2)
                 .arg(n / std::max(out.secMc, 1e-9) / 1e6, 0, 'f', 0).arg(sThreads->value());
  }
  const Character &ch = runChar_;
  int ml = eng::maxLoad(ch, std::min(ch.stamina, ch.maxStamina));
  QString head = QString("MaxLoad=%1  str=%2 dex=%3 luck=%4 load=%5 | Monster: %6 (armor=%7 dex=%8 lvl=%9%10) | %11 combos x %12 trials | %13")
                     .arg(ml).arg(ch.strength).arg(ch.dexterity).arg(ch.luck).arg(ch.load)
                     .arg(QString::fromStdString(runMon_.name)).arg(runMon_.armor).arg(runMon_.dex).arg(runMon_.level)
                     .arg(runMon_.nonMaterial ? ", non-material" : "")
                     .arg(std::max(out.exact.size(), out.mc.size())).arg(runTrials_).arg(extra);
  lStatus->setText(head);
  if (out.mode == ModeExact) showTable(out.exact, "exact");
  else if (out.mode == ModeMonteCarlo) showTable(out.mc, "Monte Carlo");
  else showCompare(out);
  if (!grabPath_.isEmpty()) {
    compare->expandAll();
    QTimer::singleShot(300, this, [this] {
      grab().save(grabPath_);
      QApplication::quit();
    });
  }
}

void MainWindow::showTable(const std::vector<ComboResult> &rows, const QString &)
{
  stack->setCurrentWidget(table);
  table->setSortingEnabled(false);
  table->clear();
  for (auto &cr : rows) {
    if (!cr.valid) continue;
    const Row &r = cr.row;
    double v[12] = {0, r.hitPct, r.dmgPerTry, r.dmgPerHit, double(r.minHit), double(r.maxHit),
                    r.dps, r.dmgPerSta, r.staPerTry, r.xpPerTry, r.xps, r.endStamina};
    auto *it = new NumItem(table);
    it->setText(0, QString::fromStdString(r.combo));
    for (int c = 1; c < 12; c++) {
      it->setText(c, (c == 4 || c == 5) ? QString::number(int(v[c])) : fmt(v[c], c == 1 || c == 11 ? 1 : 2));
      it->setData(c, Qt::UserRole, v[c]);
      it->setTextAlignment(c, Qt::AlignRight);
    }
  }
  table->setSortingEnabled(true);
  table->sortByColumn(6, Qt::DescendingOrder);
}

void MainWindow::showCompare(const RunOutput &out)
{
  stack->setCurrentWidget(compare);
  compare->clear();
  int agree = 0, total = 0;
  const QColor green(30, 140, 60), red(210, 40, 40);
  for (size_t i = 0; i < out.exact.size() && i < out.mc.size(); i++) {
    const ComboResult &ex = out.exact[i], &mc = out.mc[i];
    if (!ex.valid || !mc.valid) continue;
    CompareReport rep = compareResults(ex, mc);
    total++;
    agree += rep.ok;
    auto *top = new QTreeWidgetItem(compare);
    top->setText(0, QString::fromStdString(ex.row.combo));
    top->setText(6, rep.ok ? "agree" : "DIFFERS");
    top->setForeground(6, rep.ok ? green : red);
    top->setFont(0, [&] { QFont f = font(); f.setBold(true); return f; }());
    for (auto &c : rep.checks) {
      auto *it = new QTreeWidgetItem(top);
      it->setText(0, c.name);
      bool pct = std::string(c.name) == "hit rate";
      int prec = std::string(c.name) == "end stamina" ? 3 : 5;
      it->setText(1, fmt(c.exact, prec));
      it->setText(2, fmt(c.mc, prec));
      it->setText(3, QString::number(c.mc - c.exact, 'g', 3));
      it->setText(4, c.se > 0 ? QString::number(c.se, 'g', 3) : "-");
      it->setText(5, std::fabs(c.z) > 1e8 ? "inf" : fmt(c.z, 2));
      it->setText(6, c.ok ? "ok" : "DIFFERS");
      it->setForeground(6, c.ok ? green : red);
      (void)pct;
      for (int k = 1; k < 6; k++) it->setTextAlignment(k, Qt::AlignRight);
    }
    auto *hr = new QTreeWidgetItem(top);
    hr->setText(0, "hit range (min..max)");
    hr->setText(1, QString("%1..%2").arg(ex.row.minHit).arg(ex.row.maxHit));
    hr->setText(2, QString("%1..%2").arg(mc.row.minHit).arg(mc.row.maxHit));
    hr->setText(6, rep.rangeOk ? "ok (sampled range inside exact)" : "DIFFERS");
    hr->setForeground(6, rep.rangeOk ? green : red);
    if (rep.cells >= 2) {
      auto *ch = new QTreeWidgetItem(top);
      ch->setText(0, QString("damage histogram (chi-square, %1 cells)").arg(rep.cells));
      ch->setText(5, fmt(rep.chi2z, 2));
      bool okc = rep.chi2z <= 5.0 + (ex.structured ? 3.0 : 0.0);
      ch->setText(6, okc ? "ok" : "DIFFERS");
      ch->setForeground(6, okc ? green : red);
      ch->setTextAlignment(5, Qt::AlignRight);
    }
    top->setExpanded(!rep.ok);
  }
  lStatus->setText(lStatus->text() + QString("\n%1 of %2 combos agree (every |z| ≤ 4.5; "
                                             "z = (Monte Carlo − exact) / standard error).")
                                         .arg(agree).arg(total));
  // size the columns for the expanded rows, then fold the agreeing combos
  std::vector<bool> open;
  for (int i = 0; i < compare->topLevelItemCount(); i++) {
    open.push_back(compare->topLevelItem(i)->isExpanded());
    compare->topLevelItem(i)->setExpanded(true);
  }
  for (int i = 1; i < 7; i++) compare->resizeColumnToContents(i);
  for (int i = 0; i < compare->topLevelItemCount(); i++)
    compare->topLevelItem(i)->setExpanded(open[size_t(i)]);
}

// ------------------------------------------------------------------ graph
void MainWindow::onGraph()
{
  auto combos = selectedCombos();
  if (combos.size() != 1) {
    QMessageBox::information(this, "Graph DPS vs strength", "Tick exactly one weapon+attack for the graph.");
    return;
  }
  const Weapon &w = weapons()[size_t(combos[0].first)];
  const Attack &a = attacks()[size_t(combos[0].second)];
  Character base = character();
  Monster mon = monster();
  Options o = options();
  std::vector<GraphWidget::Pt> pts;
  base.stamina = base.maxStamina;
  for (int s = 10; s <= 120; s++) {
    Character c = base;
    c.strength = s;
    c.elvenBoots = false;
    double d0 = exactDps(c, w, a, mon, o);
    c.elvenBoots = true;
    double d1 = exactDps(c, w, a, mon, o);
    pts.push_back({s, d0, d1});
  }
  auto *dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QString("DPS vs strength - %1+%2 vs %3").arg(w.name).arg(a.name).arg(QString::fromStdString(mon.name)));
  auto *l = new QVBoxLayout(dlg);
  l->addWidget(new GraphWidget(pts, QString("DPS vs strength: %1+%2 vs %3 (exact)").arg(w.name).arg(a.name).arg(QString::fromStdString(mon.name))));
  dlg->resize(920, 620);
  dlg->show();
  lStatus->setText(QString("Graphed %1+%2 (strength 10-120).").arg(w.name).arg(a.name));
}

// ------------------------------------------------------------- savegames
void MainWindow::onLoadSave()
{
  QString path = QFileDialog::getOpenFileName(
      this, "Open DM/CSB savegame", lastDir_,
      "Savegame (DMGAME.DAT CSBGAME.DAT *.DAT *.dat *.adf *.ADF);;All files (*)");
  if (path.isEmpty()) return;
  QString script = QDir(QCoreApplication::applicationDirPath()).filePath("dump_champions.py");
  if (!QFile::exists(script)) script = QDir(QCoreApplication::applicationDirPath()).filePath("../dump_champions.py");
  if (!QFile::exists(script)) script = QString(DAMAGE_SRC_DIR) + "/dump_champions.py";
  QProcess proc;
  proc.start("python3", {script, path});
  if (!proc.waitForFinished(30000)) {
    QMessageBox::critical(this, "Cannot load savegame", "python3 did not finish (is python3 installed?)");
    return;
  }
  QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
  QJsonObject o = doc.object();
  if (o.isEmpty() || o.contains("error")) {
    QMessageBox::critical(this, "Cannot load savegame",
                          o.isEmpty() ? QString::fromUtf8(proc.readAllStandardError()) : o["error"].toString());
    return;
  }
  lastDir_ = QFileInfo(path).absolutePath();
  champions_ = o["champions"].toArray();
  slotNames_ = o["slot_names"].toObject();
  QJsonValue pv = o["party"];
  if (pv.isObject()) {
    QJsonObject p = pv.toObject();
    partyMult_ = p["exp_mult"].toInt();
    lParty->setText(QString("Level %1 @ (%2, %3) facing %4").arg(p["level"].toInt())
                        .arg(p["gx"].toInt()).arg(p["gy"].toInt()).arg(p["facing"].toString()));
  } else {
    partyMult_ = -1;
    lParty->clear();
  }
  cChampion->blockSignals(true);
  cChampion->clear();
  for (auto v : champions_) {
    QJsonObject c = v.toObject();
    QString t = c["title"].toString().trimmed();
    cChampion->addItem(t.isEmpty() ? c["name"].toString() : c["name"].toString() + " - " + t);
  }
  cChampion->blockSignals(false);
  cChampion->setEnabled(true);
  cChampion->setCurrentIndex(0);
  onChampion(0);
  QString src = o["source"].toString();
  if (!src.isEmpty()) lStatus->setText(lStatus->text() + " [" + src + "]");
}

void MainWindow::onChampion(int i)
{
  if (i < 0 || i >= champions_.size()) return;
  QJsonObject c = champions_[i].toObject();
  sStr->setValue(c["strength"].toInt());
  sDex->setValue(c["dexterity"].toInt());
  sLuck->setValue(c["luck"].toInt());
  sLuckMin->setValue(c["luck_min"].toInt());
  sLuckMax->setValue(c["luck_max"].toInt());
  sLoad->setValue(c["load"].toInt());
  sSta->setValue(c["stamina"].toInt());
  sMaxSta->setValue(std::max(1, c["max_stamina"].toInt()));
  int ou = c["ouches"].toInt();
  for (int k = 0; k < 6; k++) cWound[k]->setChecked(ou & (1 << k));
  cBoots->setChecked(c["boots"].toBool());
  QJsonObject m = c["mastery"].toObject();
  for (auto it = sMastery.begin(); it != sMastery.end(); ++it)
    it.value()->setValue(m[QString::number(it.key())].toInt());
  lStatus->setText(QString("Loaded %1 (hp %2, mana %3, stamina %4) - tick weapons/attacks, then Run.")
                       .arg(cChampion->currentText()).arg(c["hp"].toInt()).arg(c["mana"].toInt())
                       .arg(c["stamina"].toInt()));
  updateInfo();
}

void MainWindow::onShowInventory()
{
  int i = cChampion->currentIndex();
  if (i < 0 || i >= champions_.size()) return;
  QJsonObject c = champions_[i].toObject();
  QString text;
  QJsonArray inv = c["inventory"].toArray();
  for (int s = 0; s < inv.size(); s++) {
    QString slot = slotNames_.contains(QString::number(s)) ? slotNames_[QString::number(s)].toString()
                                                           : QString::number(s);
    QString name = inv[s].isString() ? inv[s].toString() : "-";
    text += QString("%1 %2\n").arg(slot, -12).arg(name);
  }
  auto *dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle("Inventory - " + cChampion->currentText());
  auto *l = new QVBoxLayout(dlg);
  auto *te = new QPlainTextEdit(text);
  te->setReadOnly(true);
  te->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  l->addWidget(te);
  dlg->resize(380, 600);
  dlg->show();
}
