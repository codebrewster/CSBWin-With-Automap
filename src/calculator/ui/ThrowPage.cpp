#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <atomic>
#include <thread>

#include "../core/Thrown.h"
#include "Pages.h"
#include "Theme.h"

using namespace calc;

namespace ui {

namespace {
enum Col { CWeapon, CLevel, CArrive, CPerThrow, CMin, CMax, CDps, CStamina, CXp, CKillThrows,
           CKillTime, ColCount };

// The skill level the object's action set asks for `action`; -1 = it does not offer it
int levelNeeded(const GameData &g, const WeaponType &w, int action)
{
  if (w.actionSet < 0 || w.actionSet >= int(g.actionSets.size())) return -1;
  const ActionSet &set = g.actionSets[size_t(w.actionSet)];
  for (int i = 0; i < 3; i++)
    if (set.action[i] == action) return set.minMastery[i];
  return -1;
}

// "need ammo; fades ..." as separate sentences
QStringList sentences(const std::string &note)
{
  QStringList out;
  for (const QString &part : QString::fromStdString(note).split(QStringLiteral("; "), Qt::SkipEmptyParts)) {
    QString s = part.trimmed();
    if (s.isEmpty()) continue;
    s[0] = s[0].toUpper();
    if (!s.endsWith(QLatin1Char('.'))) s += QLatin1Char('.');
    out << s;
  }
  return out;
}
}  // namespace

ThrowPage::ThrowPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new QHBoxLayout;
  options->setSpacing(10);
  mode_ = new QComboBox;
  mode_->addItem(tr("Thrown by hand"));
  mode_->addItem(tr("Shot from a launcher"));
  options->addWidget(mode_);
  scope_ = new QComboBox;
  scope_->addItem(tr("The weapon in hand"));
  scope_->addItem(tr("Every weapon"));
  options->addWidget(scope_);
  usable_ = new QCheckBox(tr("Only rows the hero has the skill for"));
  usable_->setChecked(true);
  options->addWidget(usable_);
  options->addSpacing(12);
  options->addWidget(mutedLabel(tr("Target is")));
  squares_ = new QSpinBox;
  squares_->setRange(1, 30);
  squares_->setValue(3);
  squares_->setSuffix(tr(" squares away"));
  squares_->setToolTip(tr("1 = the next square. A missile fades with every half square it flies."));
  options->addWidget(squares_);
  backRow_ = new QCheckBox(tr("in the back row of its square"));
  options->addWidget(backRow_);
  options->addStretch(1);
  column()->addLayout(options);

  auto *tiles = new FlowLayout(nullptr, 10);
  tiles->addWidget(tArrive_ = new Tile(tr("Arrive chance")));
  tiles->addWidget(tDamage_ = new Tile(tr("Damage per throw")));
  tiles->addWidget(tRange_ = new Tile(tr("Least – most")));
  tiles->addWidget(tDps_ = new Tile(tr("Damage per second")));
  tiles->addWidget(tKill_ = new Tile(tr("Time to kill")));
  tiles->addWidget(tStamina_ = new Tile(tr("Stamina per throw")));
  column()->addLayout(tiles);

  table_ = new QTableWidget(0, ColCount);
  styleTable(table_);
  table_->setHorizontalHeaderLabels({tr("Weapon"), tr("Needs level"), tr("Arrive"), tr("Dmg / throw"),
                                     tr("Least"), tr("Most"), tr("Dmg / s"), tr("Stamina"),
                                     tr("XP / s"), tr("Throws to kill"), tr("Time to kill")});
  table_->horizontalHeaderItem(CWeapon)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  table_->setSortingEnabled(true);
  table_->sortByColumn(CDps, Qt::DescendingOrder);
  table_->setMinimumHeight(260);
  column()->addWidget(table_, 3);

  chart_ = new Histogram;
  chart_->setMinimumHeight(190);
  column()->addWidget(chart_, 2);
  curve_ = new LineChart;
  curve_->setMinimumHeight(170);
  column()->addWidget(curve_, 2);
  note_ = noteLabel();
  column()->addWidget(note_);

  auto changed = [this] { schedule(); };
  connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
  connect(scope_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
  connect(usable_, &QCheckBox::toggled, this, changed);
  connect(squares_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
  connect(backRow_, &QCheckBox::toggled, this, changed);
  connect(table_, &QTableWidget::itemSelectionChanged, this, &ThrowPage::showRow);
}

void ThrowPage::recalculate()
{
  const GameData &g = s_->data;
  const bool shot = mode_->currentIndex() == 1;
  const bool everything = scope_->currentIndex() == 1;
  const WeaponType *inHand = s_->hero.weapon < 0 ? nullptr : &g.weapon(s_->hero.weapon);
  empty_.clear();

  struct Job {
    Hero hero;
    WeaponType object;
    WeaponType launcher;
    bool shot;
    bool usable;
    int need;
  };
  std::vector<Job> jobs;
  bool skipped = false;
  auto add = [&](const WeaponType &object, const WeaponType *launcher) {
    // try it on: the hand's weight changes, the load does not
    const WeaponType &held = launcher ? *launcher : object;
    Hero hero = s_->hero;
    if (held.id != s_->hero.weapon) {
      hero.weapon = held.id;
      hero.handWeight[1] = held.weight;
    }
    const int action = launcher ? 32 : 42;
    int need = levelNeeded(g, held, action);
    bool usable = need < 0 || hero.mastery[g.actions.at(size_t(action)).skill] >= need;
    if (!usable && usable_->isChecked()) {
      skipped = true;
      return;
    }
    jobs.push_back({hero, object, launcher ? *launcher : WeaponType(), launcher != nullptr, usable, need});
  };
  if (!shot) {
    if (!everything) {
      if (inHand) add(*inHand, nullptr);
      else empty_ = tr("Bare hands cannot be thrown. Put a weapon in the action hand, or choose Every weapon.");
    } else {
      for (const WeaponType &w : g.weapons) add(w, nullptr);
    }
  } else if (!everything) {
    if (inHand) {
      for (const WeaponType &ammo : g.weapons)
        if (canShoot(*inHand, ammo)) add(ammo, inHand);
    }
    if (jobs.empty() && !skipped)
      empty_ = tr("The weapon in hand is not a launcher, or no ammunition of this game fits it.");
  } else {
    for (const WeaponType &l : g.weapons)
      for (const WeaponType &ammo : g.weapons)
        if (canShoot(l, ammo)) add(ammo, &l);
  }
  if (jobs.empty() && empty_.isEmpty())
    empty_ = skipped ? tr("The hero has no skill for any of these. Untick the skill filter to see what they would do.")
                     : tr("Nothing to show.");

  rows_.assign(jobs.size(), ThrowResult());
  needs_.assign(jobs.size(), -1);
  heroes_.assign(jobs.size(), Hero());
  const Target target = s_->target;
  const Rules rules = s_->rules;
  const int squares = squares_->value();
  const bool back = backRow_->isChecked();
  std::atomic<size_t> next{0};
  auto work = [&] {
    for (size_t i; (i = next.fetch_add(1)) < jobs.size();) {
      const Job &job = jobs[i];
      ThrowResult r = throwAttack(g, job.hero, job.object, job.shot ? &job.launcher : nullptr, target,
                                  rules, squares, back);
      r.usable = job.usable;
      rows_[i] = r;
      needs_[i] = job.need;
      heroes_[i] = job.hero;
    }
  };
  unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), unsigned(jobs.size())));
  const bool slow = jobs.size() > 100;
  if (slow) QApplication::setOverrideCursor(Qt::WaitCursor);
  std::vector<std::thread> pool;
  for (unsigned i = 1; i < threads; i++) pool.emplace_back(work);
  work();
  for (std::thread &t : pool) t.join();
  if (slow) QApplication::restoreOverrideCursor();

  const int sortCol = table_->horizontalHeader()->sortIndicatorSection();
  const Qt::SortOrder order = table_->horizontalHeader()->sortIndicatorOrder();
  table_->setSortingEnabled(false);
  table_->blockSignals(true);
  table_->setRowCount(int(rows_.size()));
  const Theme &t = theme();
  const QString dash = QStringLiteral("–");
  auto dashCell = [&](int row, int col) { table_->setItem(row, col, numberCell(-1, dash)); };
  for (int i = 0; i < int(rows_.size()); i++) {
    const ThrowResult &r = rows_[size_t(i)];
    QString name = QString::fromStdString(g.weapon(r.object).name);
    if (r.shot) name = QString::fromStdString(g.weapon(r.launcher).name) + QStringLiteral(" + ") + name;
    auto *weapon = textCell(name);
    weapon->setData(Qt::UserRole, i);
    table_->setItem(i, CWeapon, weapon);
    const int need = needs_[size_t(i)];
    auto *level = numberCell(qMax(0, need), need > 0 ? QString::number(need) : dash);
    if (!r.usable) level->setForeground(t.bad);
    table_->setItem(i, CLevel, level);
    if (!r.valid) {
      for (int c = CArrive; c < ColCount; c++) dashCell(i, c);
      continue;
    }
    table_->setItem(i, CArrive, numberCell(r.arriveChance, percent(r.arriveChance)));
    table_->setItem(i, CPerThrow, numberCell(r.perThrow, num(r.perThrow, 2)));
    table_->setItem(i, CMin, numberCell(r.minHit, QString::number(r.minHit)));
    table_->setItem(i, CMax, numberCell(r.maxHit, QString::number(r.maxHit)));
    table_->setItem(i, CDps, numberCell(r.dps, num(r.dps, 2)));
    table_->setItem(i, CStamina, numberCell(r.stamina, num(r.stamina)));
    table_->setItem(i, CXp, numberCell(r.expPerSecond, num(r.expPerSecond)));
    bool kills = r.throwsToKill > 0;
    table_->setItem(i, CKillThrows, numberCell(kills ? r.throwsToKill : 1e18,
                                               kills ? num(r.throwsToKill) : tr("never")));
    table_->setItem(i, CKillTime, numberCell(kills ? r.secondsToKill : 1e18,
                                             kills ? duration(r.secondsToKill) : tr("never")));
  }
  table_->blockSignals(false);
  table_->setSortingEnabled(true);
  table_->sortByColumn(sortCol, order);
  table_->resizeColumnsToContents();
  int select = 0;
  for (int row = 0; row < table_->rowCount(); row++) {
    const ThrowResult &r = rows_[size_t(table_->item(row, CWeapon)->data(Qt::UserRole).toInt())];
    if (QStringLiteral("%1/%2").arg(r.object).arg(r.launcher) == picked_) select = row;
  }
  if (table_->rowCount()) table_->selectRow(select);
  showRow();
}

void ThrowPage::showRow()
{
  const QList<QTableWidgetItem *> sel = table_->selectedItems();
  if (sel.isEmpty() || rows_.empty()) {
    for (Tile *tile : {tArrive_, tDamage_, tRange_, tDps_, tKill_, tStamina_}) tile->set(QStringLiteral("–"));
    chart_->set(Dist(), tr("Damage of a throw"));
    chart_->clearExtras();
    curve_->set({}, tr("Squares away"), tr("hp"));
    curve_->setMarks({});
    note_->setText(empty_);
    return;
  }
  const ThrowResult &r = rows_[size_t(table_->item(sel.first()->row(), CWeapon)->data(Qt::UserRole).toInt())];
  picked_ = QStringLiteral("%1/%2").arg(r.object).arg(r.launcher);
  const GameData &g = s_->data;
  const Target &t = s_->target;
  QStringList notes = sentences(r.note);
  if (!r.usable)
    notes.prepend(tr("The hero lacks the skill level for this."));
  if (!r.valid) {
    for (Tile *tile : {tArrive_, tDamage_, tRange_, tDps_, tKill_, tStamina_}) tile->set(QStringLiteral("–"));
    chart_->set(Dist(), tr("Damage of a throw"));
    chart_->clearExtras();
    curve_->set({}, tr("Squares away"), tr("hp"));
    curve_->setMarks({});
    note_->setText(notes.join(QStringLiteral("  ")));
    return;
  }
  tArrive_->set(percent(r.arriveChance));
  tDamage_->set(num(r.perHit), tr("%1 per throw").arg(num(r.perThrow, 2)));
  tRange_->set(r.maxHit > 0 ? QStringLiteral("%1 – %2").arg(r.minHit).arg(r.maxHit) : QStringLiteral("–"));
  tDps_->set(num(r.dps, 2), tr("a throw takes %1 ticks").arg(num(r.ticks)));
  if (r.throwsToKill > 0)
    tKill_->set(duration(r.secondsToKill), tr("%1 throws for %2 hp").arg(num(r.throwsToKill)).arg(t.hp));
  else
    tKill_->set(tr("never"), t.hp > 0 ? (r.arriveChance == 0 ? tr("it never arrives") : tr("it cannot be hurt this way")) : tr("no hit points set"));
  tStamina_->set(num(r.stamina),
                 r.stamina > 0 ? tr("%1 damage per point").arg(num(r.perThrow / r.stamina, 2)) : QString());
  QString name = QString::fromStdString(g.weapon(r.object).name);
  if (r.shot) name = QString::fromStdString(g.weapon(r.launcher).name) + QStringLiteral(" + ") + name;
  chart_->clearExtras();
  chart_->set(r.damage, tr("%1 on %2: damage of a throw that hurts").arg(name, QString::fromStdString(t.type.name)),
              tr("hp"));
  if (t.hp > 0 && t.hp <= r.maxHit * 3 / 2) chart_->setMarker(t.hp, tr("kills"));
  {
    const int row = table_->item(sel.first()->row(), CWeapon)->data(Qt::UserRole).toInt();
    Series line;
    line.name = tr("Damage per throw");
    line.color = theme().accent;
    const WeaponType &object = g.weapon(r.object);
    const WeaponType launcher = r.shot ? g.weapon(r.launcher) : WeaponType();
    for (int sq = 1; sq <= 30; sq++) {
      ThrowResult at = throwAttack(g, heroes_[size_t(row)], object, r.shot ? &launcher : nullptr, t,
                                   s_->rules, sq, backRow_->isChecked());
      line.points.append({double(sq), at.valid ? at.perThrow : 0.0});
    }
    curve_->set({line}, tr("Squares away"), tr("hp per throw"));
    curve_->setMarks({{double(squares_->value()), tr("now: %1").arg(squares_->value()), theme().muted}});
  }
  if (t.type.invincible) notes << tr("The game does not let this monster lose hit points.");
  if (scope_->currentIndex() == 1 && !r.shot)
    notes << tr("Other weapons are tried on with the hero's load as it is.");
  note_->setText(notes.join(QStringLiteral("  ")));
}

}  // namespace ui
