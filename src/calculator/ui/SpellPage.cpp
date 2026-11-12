#include <QCheckBox>
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

namespace {
enum Metric { MAverage, MRange, MKill, MPerMana, MCast, MMana };
}

SpellPage::SpellPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new QHBoxLayout;
  options->setSpacing(10);
  options->addWidget(mutedLabel(tr("Show")));
  metric_ = new QComboBox;
  metric_->addItem(tr("Average damage"), MAverage);
  metric_->addItem(tr("Least – most damage"), MRange);
  metric_->addItem(tr("Chance to kill"), MKill);
  metric_->addItem(tr("Damage per mana"), MPerMana);
  metric_->addItem(tr("Chance the cast works"), MCast);
  metric_->addItem(tr("Mana"), MMana);
  options->addWidget(metric_);
  options->addSpacing(12);
  options->addWidget(mutedLabel(tr("Target is")));
  distance_ = new QSpinBox;
  distance_->setRange(1, 20);
  distance_->setSuffix(tr(" squares away"));
  distance_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  distance_->setToolTip(tr("1 = the next square. A missile fades with every half square it flies."));
  options->addWidget(distance_);
  backRow_ = new QCheckBox(tr("in the back row of its square"));
  options->addWidget(backRow_);
  options->addStretch(1);
  column()->addLayout(options);

  grid_ = new QTableWidget(int(damageMissiles().size()), 7);
  styleTable(grid_);
  grid_->setSelectionBehavior(QAbstractItemView::SelectItems);
  QStringList head{tr("Spell")};
  for (int p = 1; p <= 6; p++) head << QString::fromLatin1(powerName(p));
  grid_->setHorizontalHeaderLabels(head);
  grid_->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  grid_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  grid_->verticalHeader()->setDefaultSectionSize(34);
  column()->addWidget(grid_);

  auto *tiles = new FlowLayout(nullptr, 10);
  tiles->addWidget(tAvg_ = new Tile(tr("Average damage")));
  tiles->addWidget(tRange_ = new Tile(tr("Least – most")));
  tiles->addWidget(tKill_ = new Tile(tr("Chance to kill")));
  tiles->addWidget(tOthers_ = new Tile(tr("Others in the group")));
  tiles->addWidget(tMana_ = new Tile(tr("Mana")));
  tiles->addWidget(tCast_ = new Tile(tr("Cast works")));
  column()->addLayout(tiles);
  chart_ = new Histogram;
  chart_->setMinimumHeight(200);
  column()->addWidget(chart_, 2);
  note_ = noteLabel();
  column()->addWidget(note_);
  column()->addStretch(1);

  connect(metric_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { schedule(); });
  connect(distance_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { schedule(); });
  connect(backRow_, &QCheckBox::toggled, this, [this] { schedule(); });
  connect(grid_, &QTableWidget::itemSelectionChanged, this, &SpellPage::showCell);
}

void SpellPage::recalculate()
{
  const GameData &g = s_->data;
  const Theme &th = theme();
  Dice dice(s_->rules.moduloBias);
  const int steps = flightSteps(distance_->value(), backRow_->isChecked());
  const int metric = metric_->currentData().toInt();
  const std::vector<int> &missiles = damageMissiles();
  int keepRow = grid_->currentRow(), keepCol = grid_->currentColumn();
  cells_.assign(missiles.size(), std::vector<Cell>(7));
  grid_->blockSignals(true);
  for (size_t row = 0; row < missiles.size(); row++) {
    int spell = spellForMissile(g, missiles[row]);
    QString name = tr(missileName(missiles[row]));
    if (spell >= 0) name += QStringLiteral("   ") + QString::fromStdString(g.spells[size_t(spell)].incantation());
    auto *first = textCell(name);
    first->setFlags(Qt::ItemIsEnabled);
    grid_->setItem(int(row), 0, first);
    for (int power = 1; power <= 6; power++) {
      Cell &c = cells_[row][size_t(power)];
      c.cast = castSpell(g, s_->hero, missiles[row], power);
      c.at = flyMissile(c.cast.range, c.cast.damage, c.cast.decay, steps);
      c.hit = missileOnMonster(missiles[row], c.at, s_->target, dice);
      const Summary &sum = c.hit.stats;
      QString text;
      double value = 0;
      bool nothing = !c.cast.known || !c.hit.arrives || c.hit.noEffect || sum.chance <= 0;
      switch (metric) {
        case MAverage: value = sum.perTry; text = nothing ? QStringLiteral("–") : num(sum.perTry); break;
        case MRange: value = sum.max; text = nothing ? QStringLiteral("–") : QStringLiteral("%1 – %2").arg(sum.min).arg(sum.max); break;
        case MKill: value = c.hit.killChance; text = nothing ? QStringLiteral("–") : percent(c.hit.killChance, 0); break;
        case MPerMana: value = c.cast.mana ? sum.perTry / c.cast.mana : 0; text = nothing ? QStringLiteral("–") : num(value, 2); break;
        case MCast: value = c.cast.success; text = percent(c.cast.success, 0); break;
        case MMana: value = c.cast.mana; text = QString::number(c.cast.mana); break;
      }
      auto *item = numberCell(value, text);
      item->setTextAlignment(Qt::AlignCenter);
      if (!c.hit.arrives) item->setToolTip(tr("Fades before it gets there"));
      else if (c.hit.noEffect) item->setToolTip(QString::fromStdString(c.hit.note));
      if (c.cast.mana > s_->hero.mana) item->setForeground(th.muted);
      if (c.cast.success < 1 && (metric == MCast || metric == MAverage)) item->setForeground(th.warn);
      grid_->setItem(int(row), power, item);
    }
  }
  grid_->blockSignals(false);
  fitTableHeight(grid_);
  if (keepRow < 0 || keepCol < 1) {
    keepRow = 0;
    keepCol = 4;
  }
  grid_->setCurrentCell(keepRow, keepCol);
  showCell();
}

void SpellPage::showCell()
{
  int row = grid_->currentRow(), col = grid_->currentColumn();
  if (row < 0 || col < 1 || size_t(row) >= cells_.size()) return;
  const Cell &c = cells_[size_t(row)][size_t(col)];
  const Target &t = s_->target;
  const Theme &th = theme();
  const int missile = damageMissiles()[size_t(row)];
  const Summary &sum = c.hit.stats;
  const bool nothing = !c.hit.arrives || c.hit.noEffect || sum.chance <= 0;
  tAvg_->set(nothing ? QStringLiteral("–") : num(sum.perTry),
             nothing ? QString() : tr("half of the casts ≤ %1").arg(sum.median));
  tRange_->set(nothing ? QStringLiteral("–") : QStringLiteral("%1 – %2").arg(sum.min).arg(sum.max),
               nothing ? QString() : tr("9 in 10: %1–%2").arg(sum.p05).arg(sum.p95));
  tKill_->set(nothing ? QStringLiteral("–") : percent(c.hit.killChance, 0), tr("of %1 hit points").arg(t.hp));
  Summary others = summarize(c.hit.others);
  tOthers_->set(others.chance > 0 ? num(others.perTry) : QStringLiteral("–"),
                others.chance > 0 ? tr("each, %1–%2").arg(others.min).arg(others.max) : tr("only the one it hits"));
  tMana_->set(QString::number(c.cast.mana),
              c.cast.mana > s_->hero.mana ? tr("the hero has %1").arg(s_->hero.mana)
                                          : (nothing ? QString() : tr("%1 damage per mana").arg(num(sum.perTry / qMax(1, c.cast.mana), 2))));
  tMana_->setTone(c.cast.mana > s_->hero.mana ? th.bad : QColor());
  tCast_->set(percent(c.cast.success, 0),
              tr("needs %1 level %2, has %3")
                .arg(tr(skillName(s_->data.spells[size_t(qMax(0, c.cast.spell))].skill)))
                .arg(c.cast.levelNeeded).arg(c.cast.level));
  tCast_->setTone(c.cast.success < 0.5 ? th.bad : (c.cast.success < 1 ? th.warn : QColor()));
  chart_->clearExtras();
  chart_->set(c.hit.struck, tr("%1 %2 on %3: hit points lost")
                              .arg(QString::fromLatin1(powerName(col)), tr(missileName(missile)),
                                   QString::fromStdString(t.type.name)), tr("hp"));
  if (!nothing && t.hp > 0 && t.hp <= sum.max * 3 / 2) chart_->setMarker(t.hp, tr("kills"));
  QStringList notes;
  notes << tr("Leaves the hand with range %1, fading by %2 each half square; arrives with %3.")
             .arg(c.cast.range).arg(c.cast.decay)
             .arg(c.at.arrives ? QString::number(c.at.range) : tr("nothing: it fades on the way"));
  if (!c.hit.note.empty()) notes << QString::fromStdString(c.hit.note) + QStringLiteral(".");
  if (missile == MissilePoisonCloud && !c.hit.perTick.empty())
    notes << tr("Each tick inside the cloud costs %1 on average.").arg(num(summarize(c.hit.perTick).perTry));
  if (missile == MissileFireball || missile == MissileLightning)
    notes << tr("The missile strikes one monster (weaker against high defence); its blast then "
                "hits every monster in the square, less by fire resistance.");
  if (missile == MissilePoisonBolt)
    notes << tr("In this port the bolt itself does 64 ÷ defence; the poison does the rest.");
  note_->setText(notes.join(QStringLiteral("  ")));
}

}  // namespace ui
