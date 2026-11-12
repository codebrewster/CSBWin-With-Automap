#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidget>
#include <QApplication>
#include <QVBoxLayout>
#include <atomic>
#include <thread>

#include "Pages.h"
#include "Theme.h"

using namespace calc;

namespace ui {

namespace {
enum Col { CWeapon, CAction, CLevel, CHit, CPerHit, CMin, CMax, CPerSwing, CDps, CStamina,
           CPerStamina, CXp, CKillSwings, CKillTime, ColCount };
}

HeroAttackPage::HeroAttackPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new QHBoxLayout;
  options->setSpacing(10);
  scope_ = new QComboBox;
  scope_->addItem(tr("The weapon in hand"));
  scope_->addItem(tr("Every weapon"));
  options->addWidget(scope_);
  usable_ = new QCheckBox(tr("Only actions the hero has the skill for"));
  usable_->setChecked(true);
  options->addWidget(usable_);
  options->addSpacing(12);
  options->addWidget(mutedLabel(tr("Swings in a fight")));
  swings_ = new QSpinBox;
  swings_->setRange(1, 200);
  swings_->setValue(1);
  swings_->setToolTip(tr("1 = one swing as the hero stands now. More = the average over a fight "
                         "of that many swings, with luck drifting and, if ticked, stamina draining."));
  options->addWidget(swings_);
  drain_ = new QCheckBox(tr("Stamina drains"));
  drain_->setToolTip(tr("Each swing costs stamina; below half the maximum the hero hits less hard"));
  options->addWidget(drain_);
  options->addStretch(1);
  column()->addLayout(options);

  auto *tiles = new FlowLayout(nullptr, 10);
  tiles->addWidget(tHit_ = new Tile(tr("Hit chance")));
  tiles->addWidget(tDamage_ = new Tile(tr("Damage per hit")));
  tiles->addWidget(tRange_ = new Tile(tr("Least – most")));
  tiles->addWidget(tDps_ = new Tile(tr("Damage per second")));
  tiles->addWidget(tKill_ = new Tile(tr("Time to kill")));
  tiles->addWidget(tStamina_ = new Tile(tr("Stamina per swing")));
  column()->addLayout(tiles);

  table_ = new QTableWidget(0, ColCount);
  styleTable(table_);
  table_->setHorizontalHeaderLabels({tr("Weapon"), tr("Action"), tr("Needs level"), tr("Hit"),
                                     tr("Dmg / hit"), tr("Least"), tr("Most"), tr("Dmg / swing"),
                                     tr("Dmg / s"), tr("Stamina"), tr("Dmg / stamina"), tr("XP / s"),
                                     tr("Swings to kill"), tr("Time to kill")});
  table_->horizontalHeaderItem(CWeapon)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  table_->horizontalHeaderItem(CAction)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  table_->setSortingEnabled(true);
  table_->sortByColumn(CDps, Qt::DescendingOrder);
  table_->setMinimumHeight(260);
  column()->addWidget(table_, 3);

  chart_ = new Histogram;
  chart_->setMinimumHeight(190);
  column()->addWidget(chart_, 2);
  note_ = noteLabel();
  column()->addWidget(note_);

  connect(scope_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { schedule(); });
  connect(usable_, &QCheckBox::toggled, this, [this] { schedule(); });
  connect(drain_, &QCheckBox::toggled, this, [this] { schedule(); });
  connect(swings_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { schedule(); });
  connect(table_, &QTableWidget::itemSelectionChanged, this, &HeroAttackPage::showRow);
}

void HeroAttackPage::recalculate()
{
  const GameData &g = s_->data;
  SwingOptions opt;
  opt.swings = swings_->value();
  opt.staminaDrain = drain_->isChecked();
  drain_->setEnabled(opt.swings > 1);
  if (opt.swings == 1) opt.staminaDrain = false;

  std::vector<WeaponType> weapons;
  if (scope_->currentIndex() == 0) {
    weapons.push_back(s_->hero.weapon < 0 ? g.bareHands() : g.weapon(s_->hero.weapon));
  } else {
    weapons.push_back(g.bareHands());
    for (const WeaponType &w : g.weapons) weapons.push_back(w);
  }
  // every row is its own calculation: spread them over the cores, a long fight
  // with draining stamina takes a while
  struct Job {
    Hero hero;
    WeaponType weapon;
    const ActionType *action;
    int minMastery;
    bool usable;
  };
  std::vector<Job> jobs;
  for (const WeaponType &w : weapons) {
    Hero hero = s_->hero;
    if (w.id != s_->hero.weapon) {   // try it on: the hand's weight changes, the load does not
      hero.weapon = w.id;
      hero.handWeight[1] = w.weight;
    }
    for (const GameData::Offer &offer : g.physicalActions(w)) {
      const ActionType &a = g.actions[size_t(offer.action)];
      bool usable = hero.mastery[a.skill] >= offer.minMastery;
      if (!usable && usable_->isChecked()) continue;
      jobs.push_back({hero, w, &a, offer.minMastery, usable});
    }
  }
  rows_.assign(jobs.size(), SwingResult());
  const Target target = s_->target;
  const Rules rules = s_->rules;
  std::atomic<size_t> next{0};
  auto work = [&] {
    for (size_t i; (i = next.fetch_add(1)) < jobs.size();) {
      const Job &job = jobs[i];
      SwingResult r = heroAttack(job.hero, job.weapon, *job.action, target, rules, opt);
      r.minMastery = job.minMastery;
      r.usable = job.usable;
      rows_[i] = r;
    }
  };
  const bool slow = opt.staminaDrain && jobs.size() * size_t(opt.swings) > 400;
  if (slow) QApplication::setOverrideCursor(Qt::WaitCursor);
  unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), unsigned(jobs.size())));
  std::vector<std::thread> pool;
  for (unsigned i = 1; i < threads; i++) pool.emplace_back(work);
  work();
  for (std::thread &t : pool) t.join();
  if (slow) QApplication::restoreOverrideCursor();
  // fill, keeping the sort order and the selected combination
  const int sortCol = table_->horizontalHeader()->sortIndicatorSection();
  const Qt::SortOrder order = table_->horizontalHeader()->sortIndicatorOrder();
  table_->setSortingEnabled(false);
  table_->blockSignals(true);
  table_->setRowCount(int(rows_.size()));
  const Theme &t = theme();
  for (int i = 0; i < int(rows_.size()); i++) {
    const SwingResult &r = rows_[size_t(i)];
    auto *weapon = textCell(QString::fromStdString(g.weapon(r.weapon).name));
    weapon->setData(Qt::UserRole, i);
    table_->setItem(i, CWeapon, weapon);
    table_->setItem(i, CAction, textCell(QString::fromStdString(g.actions[size_t(r.action)].name)));
    auto *level = numberCell(r.minMastery, r.minMastery ? QString::number(r.minMastery) : QStringLiteral("–"));
    if (!r.usable) level->setForeground(t.bad);
    table_->setItem(i, CLevel, level);
    table_->setItem(i, CHit, numberCell(r.hitChance, percent(r.hitChance)));
    table_->setItem(i, CPerHit, numberCell(r.perHit, num(r.perHit)));
    table_->setItem(i, CMin, numberCell(r.minHit, QString::number(r.minHit)));
    table_->setItem(i, CMax, numberCell(r.maxHit, QString::number(r.maxHit)));
    table_->setItem(i, CPerSwing, numberCell(r.perSwing, num(r.perSwing, 2)));
    table_->setItem(i, CDps, numberCell(r.dps, num(r.dps, 2)));
    table_->setItem(i, CStamina, numberCell(r.staminaPerSwing, num(r.staminaPerSwing)));
    table_->setItem(i, CPerStamina, numberCell(r.damagePerStamina, num(r.damagePerStamina, 2)));
    table_->setItem(i, CXp, numberCell(r.expPerSecond, num(r.expPerSecond)));
    bool kills = r.swingsToKill > 0;
    table_->setItem(i, CKillSwings, numberCell(kills ? r.swingsToKill : 1e18,
                                               kills ? num(r.swingsToKill) : tr("never")));
    table_->setItem(i, CKillTime, numberCell(kills ? r.secondsToKill : 1e18,
                                             kills ? duration(r.secondsToKill) : tr("never")));
  }
  table_->blockSignals(false);
  table_->setSortingEnabled(true);
  table_->sortByColumn(sortCol, order);
  table_->resizeColumnsToContents();
  int select = 0;
  for (int row = 0; row < table_->rowCount(); row++) {
    const SwingResult &r = rows_[size_t(table_->item(row, CWeapon)->data(Qt::UserRole).toInt())];
    if (QStringLiteral("%1/%2").arg(r.weapon).arg(r.action) == picked_) select = row;
  }
  if (table_->rowCount()) table_->selectRow(select);
  showRow();
}

void HeroAttackPage::showRow()
{
  const QList<QTableWidgetItem *> sel = table_->selectedItems();
  if (sel.isEmpty() || rows_.empty()) {
    for (Tile *tile : {tHit_, tDamage_, tRange_, tDps_, tKill_, tStamina_}) tile->set(QStringLiteral("–"));
    chart_->set(Dist(), tr("Damage of a swing"));
    chart_->clearExtras();
    note_->setText(rows_.empty() ? tr("This weapon has no physical action the hero can use. "
                                      "Untick the skill filter to see what it would do.")
                                 : QString());
    return;
  }
  const SwingResult &r = rows_[size_t(table_->item(sel.first()->row(), CWeapon)->data(Qt::UserRole).toInt())];
  picked_ = QStringLiteral("%1/%2").arg(r.weapon).arg(r.action);
  const GameData &g = s_->data;
  const Target &t = s_->target;
  tHit_->set(percent(r.hitChance));
  tDamage_->set(num(r.perHit), tr("%1 per swing").arg(num(r.perSwing, 2)));
  tRange_->set(QStringLiteral("%1 – %2").arg(r.minHit).arg(r.maxHit));
  tDps_->set(num(r.dps, 2), tr("a swing takes %1 ticks").arg(num(r.ticks)));
  if (r.swingsToKill > 0)
    tKill_->set(duration(r.secondsToKill), tr("%1 swings for %2 hp").arg(num(r.swingsToKill)).arg(t.hp));
  else
    tKill_->set(tr("never"), t.hp > 0 ? tr("it cannot be hurt this way") : tr("no hit points set"));
  tStamina_->set(num(r.staminaPerSwing),
                 swings_->value() > 1 && drain_->isChecked()
                   ? tr("%1 left after the fight").arg(num(r.endStamina, 0))
                   : tr("%1 damage per point").arg(num(r.damagePerStamina, 2)));
  QString title = tr("%1, %2 on %3: damage of a swing that hits")
                    .arg(QString::fromStdString(g.weapon(r.weapon).name),
                         QString::fromStdString(g.actions[size_t(r.action)].name),
                         QString::fromStdString(t.type.name));
  chart_->clearExtras();
  chart_->set(r.damage, title, tr("hp"));
  if (t.hp > 0 && t.hp <= r.maxHit * 3 / 2) chart_->setMarker(t.hp, tr("kills"));
  QStringList notes;
  if (!r.usable) notes << tr("The hero lacks the skill level (%1) for this action.").arg(r.minMastery);
  if (t.type.nonMaterial && r.hitChance == 0)
    notes << tr("Non-material: only the Vorpal Blade and Disrupt can hurt it.");
  if (t.type.invincible) notes << tr("The game does not let this monster lose hit points.");
  if (scope_->currentIndex() == 1)
    notes << tr("Other weapons are tried on with the hero's load as it is.");
  note_->setText(notes.join(QStringLiteral("  ")));
}

}  // namespace ui
