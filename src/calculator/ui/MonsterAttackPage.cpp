#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Pages.h"
#include "Theme.h"

using namespace calc;

namespace ui {

MonsterAttackPage::MonsterAttackPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new QHBoxLayout;
  options->setSpacing(10);
  options->addWidget(mutedLabel(tr("Ticks between attacks")));
  interval_ = new QDoubleSpinBox;
  interval_->setRange(0, 500);
  interval_->setDecimals(1);
  interval_->setSpecialValueText(tr("from the monster"));
  interval_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  interval_->setToolTip(tr("0 = what the monster's own timing gives when it stands and strikes. "
                           "A monster that has to turn or step in between takes longer."));
  options->addWidget(interval_);
  options->addSpacing(12);
  options->addWidget(mutedLabel(tr("Missiles thrown from")));
  distance_ = new QSpinBox;
  distance_->setRange(1, 12);
  distance_->setValue(3);
  distance_->setSuffix(tr(" squares"));
  distance_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  options->addWidget(distance_);
  options->addStretch(1);
  column()->addLayout(options);

  auto *tiles = new FlowLayout(nullptr, 10);
  tiles->addWidget(tHit_ = new Tile(tr("Hit chance")));
  tiles->addWidget(tAvg_ = new Tile(tr("Damage per hit")));
  tiles->addWidget(tRange_ = new Tile(tr("Least – most")));
  tiles->addWidget(tDps_ = new Tile(tr("Damage per second")));
  tiles->addWidget(tShare_ = new Tile(tr("A hit costs")));
  tiles->addWidget(tKill_ = new Tile(tr("Hero dies after")));
  tiles->addWidget(tWound_ = new Tile(tr("Wound chance")));
  tiles->addWidget(tPoison_ = new Tile(tr("Poison")));
  tiles->addWidget(tArmour_ = new Tile(tr("Armour saves")));
  column()->addLayout(tiles);

  chart_ = new Histogram;
  chart_->setMinimumHeight(200);
  column()->addWidget(chart_, 2);

  auto *partsCard = new Card(tr("Where the blow lands"), nullptr, false);
  parts_ = new QTableWidget(7, 4);
  styleTable(parts_);
  parts_->setHorizontalHeaderLabels({tr("Body part"), tr("Aimed at"), tr("Armour value"), tr("Wounded per attack")});
  parts_->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  parts_->setSelectionMode(QAbstractItemView::NoSelection);
  partsCard->body()->addWidget(parts_);
  note_ = noteLabel();
  partsCard->body()->addWidget(note_);
  column()->addWidget(partsCard);

  auto *missileCard = new Card(tr("Missiles it throws"), nullptr, false);
  missileHead_ = noteLabel();
  missileCard->body()->addWidget(missileHead_);
  missiles_ = new QTableWidget(0, 8);
  styleTable(missiles_);
  missiles_->setHorizontalHeaderLabels({tr("Missile"), tr("Chosen"), tr("Arrives"), tr("Hero it hits: average"),
                                        tr("Least"), tr("Most"), tr("Each other hero"), tr("Poisons")});
  missiles_->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  missiles_->setSelectionMode(QAbstractItemView::NoSelection);
  missileCard->body()->addWidget(missiles_);
  missileNote_ = noteLabel();
  missileCard->body()->addWidget(missileNote_);
  column()->addWidget(missileCard);
  column()->addStretch(1);

  connect(interval_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { schedule(); });
  connect(distance_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { schedule(); });
}

void MonsterAttackPage::recalculate()
{
  const GameData &g = s_->data;
  const Hero &h = s_->hero;
  const Target &t = s_->target;
  const Theme &th = theme();
  MonsterAttackResult r = monsterAttack(h, s_->party, t, g, s_->rules, interval_->value());

  tHit_->set(percent(r.hitChance), tr("%1 get past dodge and luck").arg(percent(r.reachChance)));
  tAvg_->set(num(r.stats.mean), tr("%1 per attack").arg(num(r.stats.perTry, 2)));
  tRange_->set(QStringLiteral("%1 – %2").arg(r.stats.min).arg(r.stats.max),
               tr("half of the hits ≤ %1").arg(r.stats.median));
  tDps_->set(num(r.groupDps, 2),
             r.attackers > 1 ? tr("%1 monsters, one attack per %2 ticks each").arg(r.attackers).arg(num(r.interval))
                             : tr("one attack per %1 ticks").arg(num(r.interval)));
  tShare_->set(h.hp > 0 ? percent(r.hpShare) : QStringLiteral("–"), tr("of %1 hit points").arg(h.hp));
  tShare_->setTone(r.hpShare >= 0.5 ? th.bad : (r.hpShare >= 0.2 ? th.warn : QColor()));
  if (r.hitChance > 0 && h.hp > 0) {
    tKill_->set(duration(r.secondsToKill),
                tr("%1 attacks (9 in 10 fights: %2–%3)").arg(r.attacksToKillMedian)
                  .arg(r.attacksToKillP05).arg(r.attacksToKillP95));
  } else {
    tKill_->set(tr("never"), tr("no blow gets through"));
  }
  tWound_->set(percent(r.woundChance), tr("per attack"));
  if (t.type.poison != 0)
    tPoison_->set(percent(r.poisonChance),
                  tr("strength %1: %2 hp over %3").arg(r.poison.strength).arg(r.poison.totalDamage)
                    .arg(duration(r.poison.ticks / kTicksPerSecond)));
  else
    tPoison_->set(tr("none"));
  double saved = r.bare.perTry > 0 ? 1.0 - r.stats.perTry / r.bare.perTry : 0;
  tArmour_->set(percent(saved, 0), tr("bare: %1 per attack").arg(num(r.bare.perTry, 2)));

  chart_->clearExtras();
  chart_->set(r.damage, tr("%1 on %2: hit points lost by a blow that hurts")
                          .arg(QString::fromStdString(t.type.name), QString::fromStdString(h.name)),
              tr("hp"));
  if (h.hp > 0 && h.hp <= r.stats.max * 3 / 2) chart_->setMarker(h.hp, tr("kills"));

  static const char *const partNames[6] = {"Ready hand", "Action hand", "Head", "Torso", "Legs", "Feet"};
  Dice dice(s_->rules.moduloBias);
  const bool sharp = t.type.attackKind == KindSharp;
  for (int slot = 0; slot < 6; slot++) {
    Dist prot = slotProtection(slot, sharp, h, s_->party, g, dice);
    parts_->setItem(slot, 0, textCell(tr(partNames[slot])));
    parts_->setItem(slot, 1, numberCell(r.slotChance[slot], percent(r.slotChance[slot])));
    parts_->setItem(slot, 2, numberCell(prot.mean(), tr("%1  (%2–%3)").arg(num(prot.mean()))
                                                       .arg(prot.minValue()).arg(prot.maxValue())));
    parts_->setItem(slot, 3, numberCell(r.woundSlot[slot], percent(r.woundSlot[slot], 2)));
  }
  parts_->setItem(6, 0, textCell(tr("No part: armour does not count")));
  parts_->setItem(6, 1, numberCell(r.noSlotChance, percent(r.noSlotChance)));
  parts_->setItem(6, 2, numberCell(0, QStringLiteral("0")));
  parts_->setItem(6, 3, numberCell(0, QStringLiteral("–")));
  parts_->resizeColumnsToContents();
  fitTableHeight(parts_);
  QString how;
  switch (t.type.attackKind) {
    case KindPlain: how = tr("Nothing reduces this monster's blows."); break;
    case KindFire: how = tr("Its blows are fire: Anti-Fire and the fire shield reduce them, then armour."); break;
    case KindMagic: how = tr("Its blows are magic: Anti-Magic and the spell shield reduce them, armour does not."); break;
    case KindPsychic: how = tr("Its blows are of the mind: only Wisdom reduces them."); break;
    case KindSharp: how = tr("Its blows are sharp: armour counts by its resistance to sharp weapons."); break;
    default: how = tr("Its blows are blunt: armour counts in full."); break;
  }
  note_->setText(how + QStringLiteral(" ") +
                 tr("An armour value of 100 leaves 47 % of a blow, 0 lets 203 % through. "
                    "The Parry skill (level %1) takes twice its level off the monster's attack.")
                   .arg(h.mastery[SkillParry]));

  // ---- missiles
  std::vector<MonsterMissile> list = monsterMissiles(t.type);
  missiles_->setRowCount(int(list.size()));
  missiles_->setVisible(!list.empty());
  if (list.empty()) {
    missileHead_->setText(tr("This monster throws nothing."));
    missileNote_->clear();
    return;
  }
  MonsterLaunch launch = monsterLaunch(t.type, dice);
  const int steps = flightSteps(distance_->value(), false);
  missileHead_->setText(tr("From more than one square away it throws; next to the party it throws half "
                           "of the time. Strength at launch: range %1–%2, fading by %3 each half square.")
                          .arg(launch.range.minValue()).arg(launch.range.maxValue()).arg(launch.decay));
  QStringList notes;
  for (int i = 0; i < int(list.size()); i++) {
    const MonsterMissile &mm = list[size_t(i)];
    Dist struck, others;
    double arrives = 0, poison = 0;
    QString note;
    for (size_t k = 0; k < launch.range.p.size(); k++) {
      double w = launch.range.p[k];
      if (w == 0) continue;
      MissileState at = flyMissile(launch.range.lo + int(k), launch.damage, launch.decay, steps);
      HeroHit hit = missileOnHero(mm.missile, at, h, s_->party, g, dice);
      if (!hit.arrives) continue;
      arrives += w;
      struck.add(hit.struck, w);
      others.add(hit.others, w);
      poison += hit.poisonChance * w;
      if (!hit.note.empty()) note = QString::fromStdString(hit.note);
    }
    Summary sum = summarize(struck);
    missiles_->setItem(i, 0, textCell(tr(missileName(mm.missile))));
    missiles_->setItem(i, 1, numberCell(mm.chance, percent(mm.chance)));
    missiles_->setItem(i, 2, numberCell(arrives, percent(arrives, 0)));
    missiles_->setItem(i, 3, numberCell(sum.mean, sum.chance > 0 ? num(sum.mean) : QStringLiteral("–")));
    missiles_->setItem(i, 4, numberCell(sum.min, sum.chance > 0 ? QString::number(sum.min) : QStringLiteral("–")));
    missiles_->setItem(i, 5, numberCell(sum.max, sum.chance > 0 ? QString::number(sum.max) : QStringLiteral("–")));
    Summary os = summarize(others);
    missiles_->setItem(i, 6, numberCell(os.mean, os.chance > 0 ? num(os.mean) : QStringLiteral("–")));
    missiles_->setItem(i, 7, numberCell(poison, poison > 0 ? percent(poison) : QStringLiteral("–")));
    if (!note.isEmpty()) notes << tr("%1: %2.").arg(tr(missileName(mm.missile)), note);
  }
  missiles_->resizeColumnsToContents();
  fitTableHeight(missiles_);
  missileNote_->setText(notes.join(QStringLiteral("  ")));
}

}  // namespace ui
