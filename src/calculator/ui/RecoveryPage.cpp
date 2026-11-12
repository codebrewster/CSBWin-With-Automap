#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Pages.h"
#include "Theme.h"

using namespace calc;

namespace ui {

RecoveryPage::RecoveryPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new QHBoxLayout;
  options->setSpacing(10);
  options->addWidget(mutedLabel(tr("Game clock")));
  clock_ = new QSpinBox;
  clock_->setRange(0, 2000000000);
  clock_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  clock_->setToolTip(tr("Recovery follows the clock: of every 8 updates a fixed few give hit points "
                        "or mana. The clock only decides where in that round the hero starts."));
  options->addWidget(clock_);
  options->addSpacing(12);
  options->addWidget(mutedLabel(tr("Chart")));
  mode_ = new QComboBox;
  for (int m = 0; m < RestModeCount; m++) mode_->addItem(tr(restModeName(m)));
  mode_->setCurrentIndex(RestSleeping);
  options->addWidget(mode_);
  options->addStretch(1);
  column()->addLayout(options);

  table_ = new QTableWidget(11, 4);
  styleTable(table_);
  table_->setSelectionMode(QAbstractItemView::NoSelection);
  table_->setHorizontalHeaderLabels({QString(), tr("Walking"), tr("Standing still"), tr("Sleeping")});
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  column()->addWidget(table_);
  note_ = noteLabel();
  column()->addWidget(note_);

  chart_ = new LineChart;
  chart_->setMinimumHeight(260);
  chart_->setXFormat([this](double ticks) {
    return duration(ticks / ticksPerSecond(mode_->currentIndex() == RestSleeping));
  });
  column()->addWidget(chart_, 2);
  foodNote_ = noteLabel();
  column()->addWidget(foodNote_);
  column()->addStretch(1);

  connect(clock_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
    if (loadingClock_) return;
    s_->gameTime = unsigned(v);
    schedule();
  });
  connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { schedule(); });
}

namespace {

QString goalTime(const RecoveryGoal &g, const RecoveryResult &r, bool sleeping)
{
  if (!g.needed) return QObject::tr("already full");
  if (!g.reached) return r.died ? QObject::tr("never: the hero dies first") : QObject::tr("never");
  return QObject::tr("%1  ·  %2 ticks").arg(duration(g.ticks / ticksPerSecond(sleeping))).arg(g.ticks);
}

QString goalCost(const RecoveryGoal &g)
{
  if (!g.needed || !g.reached) return QStringLiteral("–");
  return QObject::tr("food %1  ·  water %2").arg(g.food).arg(g.water);
}

// "1.3 × Bread, 0.4 × waterskin sip"
QString asMeals(const RecoveryGoal &g, const GameData &data)
{
  if (!g.needed || !g.reached) return QStringLiteral("–");
  QStringList parts;
  const FoodType *meal = nullptr, *drink = nullptr;
  for (const FoodType &f : data.foods) {
    if (f.food > 0 && f.name == "Bread") meal = &f;
    if (f.water > 0 && !drink) drink = &f;
  }
  if (meal && g.food > 0) parts << QObject::tr("%1 × %2").arg(num(double(g.food) / meal->food, 2), QString::fromStdString(meal->name));
  if (drink && g.water > 0) parts << QObject::tr("%1 × %2").arg(num(double(g.water) / drink->water, 2), QString::fromStdString(drink->name));
  return parts.isEmpty() ? QObject::tr("nothing") : parts.join(QStringLiteral(", "));
}

}  // namespace

void RecoveryPage::recalculate()
{
  loadingClock_ = true;
  clock_->setValue(int(s_->gameTime & 0x7fffffff));
  loadingClock_ = false;
  const Hero &h = s_->hero;
  const Theme &th = theme();
  RecoveryResult res[RestModeCount];
  for (int m = 0; m < RestModeCount; m++) res[m] = recover(h, m, s_->gameTime);

  const QStringList labels = {
    tr("Hit points full after"), tr("   eaten and drunk by then"),
    tr("Stamina full after"), tr("   eaten and drunk by then"),
    tr("Mana full after"), tr("   eaten and drunk by then"),
    tr("All three full after"), tr("   eaten and drunk by then"), tr("   which is about"),
    tr("One update gives"), tr("Mana costs stamina")};
  for (int row = 0; row < labels.size(); row++) table_->setItem(row, 0, textCell(labels[row]));
  for (int m = 0; m < RestModeCount; m++) {
    const RecoveryResult &r = res[m];
    const RecoveryGoal *goals[4] = {&r.hp, &r.stamina, &r.mana, &r.all};
    for (int k = 0; k < 4; k++) {
      auto *time = textCell(goalTime(*goals[k], r, m == RestSleeping));
      if (goals[k]->needed && !goals[k]->reached) time->setForeground(th.bad);
      table_->setItem(2 * k, m + 1, time);
      table_->setItem(2 * k + 1, m + 1, textCell(goalCost(*goals[k])));
    }
    table_->setItem(8, m + 1, textCell(asMeals(r.all, s_->data)));
    table_->setItem(9, m + 1,
                    textCell(tr("every %1 ticks: hp +%2 (%3 of 8), stamina +%4, mana +%5 (%6 of 8)")
                               .arg(r.updateEvery).arg(r.hpGain).arg(r.hpUpdates).arg(r.staminaGain)
                               .arg(r.manaGain).arg(r.manaUpdates)));
    table_->item(9, m + 1)->setToolTip(table_->item(9, m + 1)->text());
    table_->setItem(10, m + 1,
                    textCell(r.mana.needed ? tr("%1 per update, %2 until full").arg(r.manaStaminaCost).arg(r.staminaForMana)
                                           : tr("%1 per update").arg(r.manaStaminaCost)));
  }
  fitTableHeight(table_);
  QStringList notes;
  if (h.stamina < h.maxStamina / 4 && h.hp < h.maxHp)
    notes << tr("Stamina is below a quarter of its maximum: hit points do not come back until it is above.");
  if (h.food < 0 || h.water < 0)
    notes << tr("The hero is hungry or thirsty: stamina only comes back while food and water are above zero.");
  for (int m = 0; m < RestModeCount; m++) {
    if (res[m].died)
      notes << tr("%1: the hero starves to death after %2.").arg(tr(restModeName(m)), duration(res[m].ticksSimulated / ticksPerSecond(m == RestSleeping)));
    else if (res[m].starving && !(h.food < 0 || h.water < 0))
      notes << tr("%1: food or water runs out on the way.").arg(tr(restModeName(m)));
    if (res[m].hpLostToExhaustion > 0 && !res[m].died)
      notes << tr("%1: exhaustion costs %2 hit points.").arg(tr(restModeName(m))).arg(res[m].hpLostToExhaustion);
  }
  notes << tr("Walking counts no stamina spent on the steps themselves, only that the party is not at rest. "
              "Hit points cost no food of their own: the hero simply eats while waiting. "
              "While asleep the wizard and priest levels do not count, so mana comes in fewer updates and costs more stamina.");
  note_->setText(notes.join(QStringLiteral("  ")));

  // ---- chart of the chosen way to rest
  const RecoveryResult &r = res[mode_->currentIndex()];
  QVector<Series> series(5);
  const QString names[5] = {tr("Hit points %"), tr("Stamina %"), tr("Mana %"), tr("Food"), tr("Water")};
  for (int k = 0; k < 5; k++) {
    series[k].name = names[k];
    series[k].color = th.series[k];
    series[k].rightAxis = k >= 3;
  }
  size_t stride = qMax<size_t>(1, r.curve.size() / 600);
  for (size_t i = 0; i < r.curve.size(); i += stride) {
    const RecoveryPoint &pt = r.curve[i];
    series[0].points.append({double(pt.tick), 100.0 * pt.hp / qMax(1, h.maxHp)});
    series[1].points.append({double(pt.tick), 100.0 * pt.stamina / qMax(1, h.maxStamina)});
    series[2].points.append({double(pt.tick), h.maxMana ? 100.0 * pt.mana / h.maxMana : 100.0});
    series[3].points.append({double(pt.tick), double(qMax(0, pt.food))});
    series[4].points.append({double(pt.tick), double(qMax(0, pt.water))});
  }
  chart_->set(series, tr("time %1").arg(tr(restModeName(mode_->currentIndex())).toLower()),
              tr("% of maximum"), tr("food, water"));
  QVector<ChartMark> marks;
  if (r.hp.needed && r.hp.reached) marks.append({double(r.hp.ticks), tr("hit points full"), th.series[0]});
  if (r.stamina.needed && r.stamina.reached) marks.append({double(r.stamina.ticks), tr("stamina full"), th.series[1]});
  if (r.mana.needed && r.mana.reached) marks.append({double(r.mana.ticks), tr("mana full"), th.series[2]});
  chart_->setMarks(marks);

  QStringList foods;
  for (const FoodType &f : s_->data.foods)
    foods << (f.food ? tr("%1 %2").arg(QString::fromStdString(f.name)).arg(f.food)
                     : tr("%1 %2 water").arg(QString::fromStdString(f.name)).arg(f.water));
  foodNote_->setText(tr("What food is worth: %1. Food and water each hold at most 2048.").arg(foods.join(QStringLiteral(", "))));
}

}  // namespace ui
