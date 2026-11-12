#include <QComboBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Pages.h"
#include "Theme.h"

using namespace calc;

namespace ui {

namespace {

// Everything a calculation is given, as a copy one value of which is swept
struct Setup {
  const GameData *data;
  Hero hero;
  Party party;
  Target target;
  Rules rules;
  unsigned clock;
  int power;
  int squares = 3;
};

struct Variable {
  const char *name;
  std::function<int *(Setup &)> at;
  int lo, hi;
};

struct Result {
  const char *name;
  const char *unit;
  std::function<double(const Setup &)> of;
};

// The hero's best physical action with the weapon in hand
SwingResult bestSwing(const Setup &s)
{
  const GameData &g = *s.data;
  WeaponType w = s.hero.weapon < 0 ? g.bareHands() : g.weapon(s.hero.weapon);
  SwingResult best;
  for (const GameData::Offer &o : g.physicalActions(w)) {
    const ActionType &a = g.actions[size_t(o.action)];
    if (s.hero.mastery[a.skill] < o.minMastery) continue;
    SwingResult r = heroAttack(s.hero, w, a, s.target, s.rules, SwingOptions());
    if (!best.valid || r.dps > best.dps) best = r;
  }
  return best;
}

double spellAverage(const Setup &s, int missile)
{
  Dice dice(s.rules.moduloBias);
  Cast c = castSpell(*s.data, s.hero, missile, s.power);
  MissileState at = flyMissile(c.range, c.damage, c.decay, 0);
  return missileOnMonster(missile, at, s.target, dice).stats.perTry;
}

// The weapon in the hero's hand thrown by hand at s.squares squares
double thrownAverage(const Setup &s)
{
  if (s.hero.weapon < 0) return 0;
  const GameData &g = *s.data;
  ThrowResult r = throwAttack(g, s.hero, g.weapon(s.hero.weapon), nullptr, s.target, s.rules, s.squares, false);
  return r.valid ? r.perThrow : 0;
}

double recoverySeconds(const Setup &s, RecoveryGoal RecoveryResult::*goal, int mode)
{
  RecoveryResult r = recover(s.hero, mode, s.clock);
  const RecoveryGoal &g = r.*goal;
  return g.reached ? g.ticks / ticksPerSecond(mode == RestSleeping) : 0;
}

const std::vector<Variable> &variables()
{
  static const std::vector<Variable> v = {
    {"Hero: Strength", [](Setup &s) { return &s.hero.attr[Strength]; }, 1, 200},
    {"Hero: Dexterity", [](Setup &s) { return &s.hero.attr[Dexterity]; }, 1, 200},
    {"Hero: Wisdom", [](Setup &s) { return &s.hero.attr[Wisdom]; }, 1, 200},
    {"Hero: Vitality", [](Setup &s) { return &s.hero.attr[Vitality]; }, 1, 200},
    {"Hero: Anti-Magic", [](Setup &s) { return &s.hero.attr[AntiMagic]; }, 1, 200},
    {"Hero: Anti-Fire", [](Setup &s) { return &s.hero.attr[AntiFire]; }, 1, 200},
    {"Hero: Luck", [](Setup &s) { return &s.hero.attr[Luck]; }, 0, 200},
    {"Hero: load", [](Setup &s) { return &s.hero.load; }, 0, 1500},
    {"Hero: stamina", [](Setup &s) { return &s.hero.stamina; }, 0, 5000},
    {"Hero: maximum hit points", [](Setup &s) { return &s.hero.maxHp; }, 1, 2000},
    {"Hero: maximum mana", [](Setup &s) { return &s.hero.maxMana; }, 0, 900},
    {"Hero: Swing level", [](Setup &s) { return &s.hero.mastery[SkillSwing]; }, 1, 20},
    {"Hero: Thrust level", [](Setup &s) { return &s.hero.mastery[SkillThrust]; }, 1, 20},
    {"Hero: Club level", [](Setup &s) { return &s.hero.mastery[SkillClub]; }, 1, 20},
    {"Hero: Parry level", [](Setup &s) { return &s.hero.mastery[SkillParry]; }, 1, 20},
    {"Hero: Fire level", [](Setup &s) { return &s.hero.mastery[SkillFire]; }, 1, 20},
    {"Hero: Air level", [](Setup &s) { return &s.hero.mastery[SkillAir]; }, 1, 20},
    {"Hero: Earth level", [](Setup &s) { return &s.hero.mastery[SkillEarth]; }, 1, 20},
    {"Hero: Water level", [](Setup &s) { return &s.hero.mastery[SkillWater]; }, 1, 20},
    {"Hero: Wizard level", [](Setup &s) { return &s.hero.mastery[SkillWizard]; }, 1, 20},
    {"Hero: Priest level", [](Setup &s) { return &s.hero.mastery[SkillPriest]; }, 1, 20},
    {"Party shield", [](Setup &s) { return &s.party.partyShield; }, 0, 200},
    {"Fire shield", [](Setup &s) { return &s.party.fireShield; }, 0, 200},
    {"Monster: level factor", [](Setup &s) { return &s.target.level; }, 0, 15},
    {"Monster: defence", [](Setup &s) { return &s.target.type.defense; }, 0, 255},
    {"Monster: dexterity", [](Setup &s) { return &s.target.type.dexterity; }, 0, 255},
    {"Monster: attack", [](Setup &s) { return &s.target.type.attack; }, 0, 255},
    {"Monster: fire resistance", [](Setup &s) { return &s.target.type.fireResistance; }, 0, 15},
    {"Throw: distance (squares)", [](Setup &s) { return &s.squares; }, 1, 30},
  };
  return v;
}

const std::vector<Result> &results()
{
  static const std::vector<Result> v = {
    {"Hero: damage per second (best action)", "hp / s", [](const Setup &s) { return bestSwing(s).dps; }},
    {"Hero: damage per swing (best action)", "hp", [](const Setup &s) { return bestSwing(s).perSwing; }},
    {"Hero: hit chance (best action)", "%", [](const Setup &s) { return 100 * bestSwing(s).hitChance; }},
    {"Hero: seconds to kill the monster", "s", [](const Setup &s) { return bestSwing(s).secondsToKill; }},
    {"Monster: damage per attack", "hp", [](const Setup &s) { return monsterAttack(s.hero, s.party, s.target, *s.data, s.rules).stats.perTry; }},
    {"Monster: hit chance", "%", [](const Setup &s) { return 100 * monsterAttack(s.hero, s.party, s.target, *s.data, s.rules).hitChance; }},
    {"Monster: damage per second", "hp / s", [](const Setup &s) { return monsterAttack(s.hero, s.party, s.target, *s.data, s.rules).groupDps; }},
    {"Monster: seconds until the hero dies", "s", [](const Setup &s) { return monsterAttack(s.hero, s.party, s.target, *s.data, s.rules).secondsToKill; }},
    {"Monster: wound chance per attack", "%", [](const Setup &s) { return 100 * monsterAttack(s.hero, s.party, s.target, *s.data, s.rules).woundChance; }},
    {"Throw: damage per throw (weapon in hand, thrown)", "hp", [](const Setup &s) { return thrownAverage(s); }},
    {"Spell: Fireball, average damage", "hp", [](const Setup &s) { return spellAverage(s, MissileFireball); }},
    {"Spell: Lightning Bolt, average damage", "hp", [](const Setup &s) { return spellAverage(s, MissileLightning); }},
    {"Spell: Poison Bolt, average damage", "hp", [](const Setup &s) { return spellAverage(s, MissilePoisonBolt); }},
    {"Spell: Poison Cloud, average damage", "hp", [](const Setup &s) { return spellAverage(s, MissilePoisonCloud); }},
    {"Spell: Harm Non-Material, average damage", "hp", [](const Setup &s) { return spellAverage(s, MissileDispell); }},
    {"Spell: chance the Fireball cast works", "%", [](const Setup &s) { return 100 * castSpell(*s.data, s.hero, MissileFireball, s.power).success; }},
    {"Sleeping: seconds until hit points are full", "s", [](const Setup &s) { return recoverySeconds(s, &RecoveryResult::hp, RestSleeping); }},
    {"Sleeping: seconds until mana is full", "s", [](const Setup &s) { return recoverySeconds(s, &RecoveryResult::mana, RestSleeping); }},
    {"Sleeping: seconds until stamina is full", "s", [](const Setup &s) { return recoverySeconds(s, &RecoveryResult::stamina, RestSleeping); }},
    {"Standing: seconds until hit points are full", "s", [](const Setup &s) { return recoverySeconds(s, &RecoveryResult::hp, RestStanding); }},
  };
  return v;
}

}  // namespace

WhatIfPage::WhatIfPage(Session *session, QWidget *parent) : Page(session, parent)
{
  auto *options = new FlowLayout(nullptr, 10);
  auto pair = [&](const QString &label, QWidget *w) {
    auto *box = new QWidget;
    auto *l = new QHBoxLayout(box);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    l->addWidget(mutedLabel(label));
    l->addWidget(w, 1);
    options->addWidget(box);
  };
  result_ = new QComboBox;
  for (const Result &r : results()) result_->addItem(tr(r.name));
  pair(tr("Show"), result_);
  variable_ = new QComboBox;
  for (const Variable &v : variables()) variable_->addItem(tr(v.name));
  pair(tr("as it changes with"), variable_);
  from_ = new QSpinBox;
  to_ = new QSpinBox;
  for (QSpinBox *b : {from_, to_}) {
    b->setRange(-2000, 5000);
    b->setButtonSymbols(QAbstractSpinBox::NoButtons);
  }
  pair(tr("from"), from_);
  pair(tr("to"), to_);
  power_ = new QSpinBox;
  power_->setRange(1, 6);
  power_->setValue(4);
  power_->setToolTip(tr("1 = LO … 6 = MON"));
  pair(tr("Spell power"), power_);
  column()->addLayout(options);
  chart_ = new LineChart;
  chart_->setMinimumHeight(360);
  column()->addWidget(chart_, 1);
  note_ = noteLabel();
  column()->addWidget(note_);

  connect(result_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { schedule(); });
  connect(variable_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { variablePicked(); });
  for (QSpinBox *b : {from_, to_, power_})
    connect(b, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { schedule(); });
  variablePicked();
}

void WhatIfPage::variablePicked()
{
  const Variable &v = variables()[size_t(variable_->currentIndex())];
  Setup s{&s_->data, s_->hero, s_->party, s_->target, s_->rules, s_->gameTime, 4};
  int now = *v.at(s);
  // a window round the present value, inside what the value can be
  int span = qMax(10, (v.hi - v.lo) / 4);
  int lo = qMax(v.lo, now - span), hi = qMin(v.hi, now + span);
  if (v.hi - v.lo <= 40) {
    lo = v.lo;
    hi = v.hi;
  }
  from_->blockSignals(true);
  to_->blockSignals(true);
  from_->setValue(lo);
  to_->setValue(hi);
  from_->blockSignals(false);
  to_->blockSignals(false);
  schedule();
}

void WhatIfPage::recalculate()
{
  const Variable &v = variables()[size_t(variable_->currentIndex())];
  const Result &res = results()[size_t(result_->currentIndex())];
  const Theme &th = theme();
  Setup base{&s_->data, s_->hero, s_->party, s_->target, s_->rules, s_->gameTime, power_->value()};
  int lo = qMin(from_->value(), to_->value()), hi = qMax(from_->value(), to_->value());
  int step = qMax(1, (hi - lo) / 120);
  Series line;
  line.name = tr(res.name);
  line.color = th.accent;
  const int now = *v.at(base);
  double atNow = res.of(base);
  for (int x = lo; x <= hi; x += step) {
    Setup s = base;
    *v.at(s) = x;
    line.points.append({double(x), res.of(s)});
  }
  chart_->set({line}, tr(v.name), tr(res.unit));
  QVector<ChartMark> marks;
  if (now >= lo && now <= hi) marks.append({double(now), tr("now: %1").arg(now), th.muted});
  chart_->setMarks(marks);
  bool flat = true;
  for (const QPointF &pt : line.points) flat = flat && pt.y() == 0;
  if (flat && QString::fromLatin1(res.name).startsWith(QLatin1String("Throw:"))) {
    note_->setText(tr("Nothing to show: the hero holds nothing to throw, or the missile never arrives "
                      "or cannot hurt the monster."));
    return;
  }
  if (flat && QString::fromLatin1(res.name).startsWith(QLatin1String("Hero:"))) {
    note_->setText(tr("Nothing to show: the weapon in the hero's hand has no physical action the hero "
                      "can use, or the monster cannot be hurt by it."));
    return;
  }
  note_->setText(tr("Now, with %1 at %2: %3 %4. Everything else stays as set in the side panel. "
                    "A value of 0 for a time means it never happens. The Throw lines use the weapon in the hero's "
                    "hand, thrown by hand, at the distance set here (3 squares unless it is the input).")
                   .arg(tr(v.name)).arg(now).arg(num(atNow, 2), tr(res.unit)));
}

}  // namespace ui
