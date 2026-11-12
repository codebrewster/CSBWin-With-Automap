#include "Sidebar.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "../core/Defense.h"
#include "Widgets.h"

using namespace calc;

namespace ui {

namespace {
// Combo boxes in a scrolling panel must not eat the wheel
class Choice : public QComboBox {
 public:
  using QComboBox::QComboBox;
 protected:
  void wheelEvent(QWheelEvent *e) override
  {
    if (hasFocus()) QComboBox::wheelEvent(e);
    else e->ignore();
  }
};

Choice *choice()
{
  auto *c = new Choice;
  c->setFocusPolicy(Qt::StrongFocus);
  c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  c->setMinimumContentsLength(10);
  return c;
}

void selectData(QComboBox *box, int value)
{
  int i = box->findData(value);
  box->setCurrentIndex(i < 0 ? 0 : i);
}
}  // namespace

Sidebar::Sidebar(Session *session, QWidget *parent) : QWidget(parent), s_(session)
{
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *body = new QWidget;
  body->setObjectName(QStringLiteral("sidebarBody"));
  auto *column = new QVBoxLayout(body);
  column->setContentsMargins(12, 12, 6, 12);
  column->setSpacing(10);
  buildHero(column);
  buildMonster(column);
  buildRules(column);
  column->addStretch(1);
  scroll->setWidget(body);
  outer->addWidget(scroll);
  connect(s_, &Session::reloaded, this, &Sidebar::refill);
  connect(s_, &Session::changed, this, [this] {
    const MonsterType &m = s_->target.type;
    int level = qMax(1, s_->target.level);
    monsterNote_->setText(tr("A fresh one on this level has %1–%2 hit points.")
                            .arg(monsterHpMin(m, level)).arg(monsterHpMax(m, level)));
  });
  refill();
}

void Sidebar::buildHero(QVBoxLayout *column)
{
  Hero &h = s_->hero;
  const Hero &b = s_->heroBase;
  auto field = [&](int *v, const int *base, int lo, int hi) { return new IntField(s_, v, base, lo, hi); };

  auto *card = new Card(tr("Hero"));
  heroBox_ = choice();
  card->body()->addWidget(heroBox_);
  heroNote_ = noteLabel();
  card->body()->addWidget(heroNote_);
  connect(heroBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
    if (!filling_ && i >= 0) s_->selectHero(i);
  });
  QGridLayout *g = card->addGrid();
  int n = 0;
  Card::addPair(g, n++, 2, tr("Hit points"), field(&h.hp, &b.hp, 0, 9999));
  Card::addPair(g, n++, 2, tr("of"), field(&h.maxHp, &b.maxHp, 1, 9999));
  Card::addPair(g, n++, 2, tr("Stamina"), field(&h.stamina, &b.stamina, 0, 32000),
                tr("Below half of the maximum, strength and load suffer"));
  Card::addPair(g, n++, 2, tr("of"), field(&h.maxStamina, &b.maxStamina, 1, 32000));
  Card::addPair(g, n++, 2, tr("Mana"), field(&h.mana, &b.mana, 0, 9999));
  Card::addPair(g, n++, 2, tr("of"), field(&h.maxMana, &b.maxMana, 0, 9999),
                tr("More mana lets a spell missile fade more slowly"));
  Card::addPair(g, n++, 2, tr("Food"), field(&h.food, &b.food, -1024, 2048),
                tr("-1024 to 2048; below 0 the hero is hungry, below -512 starving"));
  Card::addPair(g, n++, 2, tr("Water"), field(&h.water, &b.water, -1024, 2048));
  Card::addPair(g, n++, 2, tr("Load"), field(&h.load, &b.load, 0, 9999),
                tr("What the hero carries, in tenths of a kilogram"));
  column->addWidget(card);

  auto *attrs = new Card(tr("Attributes"));
  g = attrs->addGrid();
  n = 0;
  for (int a : {Strength, Dexterity, Wisdom, Vitality, AntiMagic, AntiFire, Luck})
    Card::addPair(g, n++, 2, tr(attributeName(a)), field(&h.attr[a], &b.attr[a], 0, 255),
                  a == Strength
                    ? tr("Strength sets the throwing power (halved, at most 100) and the load the hero "
                         "can carry. With a weapon the damage stops growing at a strength of about "
                         "100, bare-handed at about 200: more strength does nothing")
                    : QString());
  Card::addPair(g, n++, 2, tr("Luck min"), field(&h.attrMin[Luck], &b.attrMin[Luck], 0, 255),
                tr("Luck moves by 2 with every luck roll, between these bounds"));
  Card::addPair(g, n++, 2, tr("Luck max"), field(&h.attrMax[Luck], &b.attrMax[Luck], 0, 255));
  column->addWidget(attrs);

  auto *skills = new Card(tr("Skill levels"));
  skills->body()->addWidget(noteLabel(tr("Levels as the game counts them: 1 = none, 2 = Neophyte … "
                                         "Bonuses of held items are included.")));
  g = skills->addGrid();
  n = 0;
  for (int k = 0; k < SkillCount; k++)
    Card::addPair(g, n++, 2, tr(skillName(k)), field(&h.mastery[k], &b.mastery[k], 0, 40));
  column->addWidget(skills);
  skills->setCollapsed(true);

  auto *gear = new Card(tr("Weapon and armour"));
  g = gear->addGrid();
  weaponBox_ = choice();
  Card::addPair(g, 0, 1, tr("Action hand"), weaponBox_);
  connect(weaponBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (filling_) return;
    Hero &hero = s_->hero;
    hero.weapon = weaponBox_->currentData().toInt();
    const ArmourType *shield = nullptr;
    if (hero.armour[SlotActionHand] >= 0) shield = &s_->data.armours[size_t(hero.armour[SlotActionHand])];
    if (hero.weapon >= 0) {
      hero.armour[SlotActionHand] = -1;
      hero.handWeight[1] = s_->data.weapon(hero.weapon).weight;
    } else if (!shield) {
      hero.handWeight[1] = 0;
    }
    s_->touch();
  });
  static const char *const slotLabels[6] = {"Ready hand", "", "Head", "Torso", "Legs", "Feet"};
  int row = 1;
  for (int slot : {0, 2, 3, 4, 5}) {
    armourBox_[slot] = choice();
    Card::addPair(g, row++, 1, tr(slotLabels[slot]), armourBox_[slot]);
    connect(armourBox_[slot], QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, slot](int) {
              if (filling_) return;
              Hero &hero = s_->hero;
              hero.armour[slot] = armourBox_[slot]->currentData().toInt();
              if (slot == 0)
                hero.handWeight[0] = hero.armour[0] >= 0 ? s_->data.armours[size_t(hero.armour[0])].weight : 0;
              if (slot == SlotFeet) hero.elvenBoots = hero.armour[slot] == ArmourElvenBoots;
              s_->touch();
            });
  }
  g = gear->addGrid();
  n = 0;
  Card::addPair(g, n++, 2, tr("Own shield"), field(&h.shield, &b.shield, 0, 999),
                tr("Protection from a Ya potion or the Spellshield action, added to every body part"));
  Card::addPair(g, n++, 2, tr("Action bonus"), field(&h.actionDefense, &b.actionDefense, -99, 99),
                tr("Protection of the action under way: Parry +29, Block +36, Berzerk -10 …"));
  auto *cross = new BoolField(tr("Ekkhard Cross worn (faster healing)"), s_, &h.ekkhardCross);
  gear->body()->addWidget(cross);
  auto *wounds = new QGridLayout;
  wounds->setSpacing(4);
  static const char *const woundNames[6] = {"Ready hand", "Action hand", "Head", "Torso", "Legs", "Feet"};
  gear->body()->addWidget(mutedLabel(tr("Wounded")));
  for (int k = 0; k < 6; k++)
    wounds->addWidget(new BitField(tr(woundNames[k]), s_, &h.wounds, 1 << k), k / 2, k % 2);
  gear->body()->addLayout(wounds);
  column->addWidget(gear);

  auto *partyCard = new Card(tr("Party shields"));
  g = partyCard->addGrid();
  n = 0;
  Party &p = s_->party;
  const Party &pb = s_->partyBase;
  Card::addPair(g, n++, 3, tr("Party"), field(&p.partyShield, &pb.partyShield, 0, 999),
                tr("YA IR: added to the armour of every body part"));
  Card::addPair(g, n++, 3, tr("Spell"), field(&p.spellShield, &pb.spellShield, 0, 999),
                tr("Taken off magic damage"));
  Card::addPair(g, n++, 3, tr("Fire"), field(&p.fireShield, &pb.fireShield, 0, 999),
                tr("FUL BRO NETA: taken off fire damage"));
  column->addWidget(partyCard);
  partyCard->setCollapsed(true);
}

void Sidebar::buildMonster(QVBoxLayout *column)
{
  Target &t = s_->target;
  const Target &b = s_->targetBase;
  auto field = [&](int *v, const int *base, int lo, int hi) { return new IntField(s_, v, base, lo, hi); };

  auto *card = new Card(tr("Monster"));
  QGridLayout *g = card->addGrid();
  levelBox_ = choice();
  groupBox_ = choice();
  monsterBox_ = choice();
  Card::addPair(g, 0, 1, tr("Level"), levelBox_);
  Card::addPair(g, 1, 1, tr("Group"), groupBox_);
  Card::addPair(g, 2, 1, tr("Kind"), monsterBox_);
  connect(levelBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!filling_) s_->selectLevel(levelBox_->currentData().toInt());
  });
  connect(groupBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!filling_) s_->selectGroup(groupBox_->currentData().toInt());
  });
  connect(monsterBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!filling_) s_->selectMonsterType(monsterBox_->currentData().toInt());
  });
  monsterNote_ = noteLabel();
  card->body()->addWidget(monsterNote_);
  g = card->addGrid();
  int n = 0;
  Card::addPair(g, n++, 2, tr("Level factor"), field(&t.level, &b.level, 0, 15),
                tr("The level's experience multiplier: it adds to the monster's defence, speed and blows"));
  Card::addPair(g, n++, 2, tr("Hit points"), field(&t.hp, &b.hp, 0, 65535), tr("Of one monster"));
  Card::addPair(g, n++, 2, tr("Monsters"), field(&t.count, &b.count, 1, 4),
                tr("How many strike the hero, or stand in a blast together"));
  card->body()->addWidget(new BoolField(tr("Caught while it attacks"), s_, &t.attacking));
  column->addWidget(card);

  auto *stats = new Card(tr("Monster numbers"));
  stats->body()->addWidget(noteLabel(tr("From the game's tables; change them to try a monster of your own.")));
  g = stats->addGrid();
  n = 0;
  MonsterType &m = t.type;
  const MonsterType &mb = b.type;
  Card::addPair(g, n++, 2, tr("Defence"), field(&m.defense, &mb.defense, 0, 255));
  Card::addPair(g, n++, 2, tr("Dexterity"), field(&m.dexterity, &mb.dexterity, 0, 255));
  Card::addPair(g, n++, 2, tr("Attack"), field(&m.attack, &mb.attack, 0, 255));
  Card::addPair(g, n++, 2, tr("Poison"), field(&m.poison, &mb.poison, 0, 255));
  Card::addPair(g, n++, 2, tr("Base health"), field(&m.baseHealth, &mb.baseHealth, 0, 255));
  Card::addPair(g, n++, 2, tr("Experience"), field(&m.expClass, &mb.expClass, 0, 15));
  Card::addPair(g, n++, 2, tr("Attack ticks"), field(&m.attackTicks, &mb.attackTicks, 0, 255),
                tr("Ticks an attack takes"));
  Card::addPair(g, n++, 2, tr("Pause"), field(&m.attackPause, &mb.attackPause, 0, 15),
                tr("Ticks added after each attack"));
  Card::addPair(g, n++, 2, tr("Fire resist."), field(&m.fireResistance, &mb.fireResistance, 0, 15),
                tr("15 = immune"));
  Card::addPair(g, n++, 2, tr("Poison resist."), field(&m.poisonResistance, &mb.poisonResistance, 0, 15),
                tr("15 = immune"));
  Card::addPair(g, n++, 2, tr("Aim"), field(&m.woundNibbles, &mb.woundNibbles, 0, 65535),
                tr("word22 of the monster's description: which body parts its blows land on"));
  kindBox_ = choice();
  for (int k = 0; k <= 7; k++) kindBox_->addItem(tr(damageKindName(k)), k);
  QGridLayout *g2 = stats->addGrid();
  Card::addPair(g2, 0, 1, tr("Blow"), kindBox_, tr("How the hero's defences meet its blows"));
  connect(kindBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (filling_) return;
    s_->target.type.attackKind = kindBox_->currentData().toInt();
    s_->touch();
  });
  stats->body()->addWidget(new BoolField(tr("Non-material"), s_, &m.nonMaterial));
  stats->body()->addWidget(new BoolField(tr("Cannot be harmed"), s_, &m.invincible));
  column->addWidget(stats);
  stats->setCollapsed(true);
}

void Sidebar::buildRules(QVBoxLayout *column)
{
  auto *card = new Card(tr("Rules and tables"));
  QGridLayout *g = card->addGrid();
  tablesBox_ = choice();
  Card::addPair(g, 0, 1, tr("Tables"), tablesBox_,
                tr("Where the numbers of monsters, weapons, armour and spells come from"));
  connect(tablesBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!filling_) s_->selectTables(tablesBox_->currentData().toInt());
  });
  tablesNote_ = noteLabel();
  card->body()->addWidget(tablesNote_);
  Rules &r = s_->rules;
  auto *vulnerable = new BoolField(tr("Monsters vulnerable on attack"), s_, &r.vulnerableOnAttack);
  vulnerable->setToolTip(tr("This port's rule: a monster caught while it attacks has half its defence"));
  card->body()->addWidget(vulnerable);
  auto *drift = new BoolField(tr("Luck drifts during a fight"), s_, &r.luckDrift);
  drift->setToolTip(tr("Every luck roll moves Luck by 2: down after a lucky one, up after an unlucky one"));
  card->body()->addWidget(drift);
  auto *bias = new BoolField(tr("Dice as on the Atari"), s_, &r.moduloBias);
  bias->setToolTip(tr("The game's dice favour low numbers very slightly; off = perfectly even dice"));
  card->body()->addWidget(bias);
  auto rate = [&](int row, const QString &label, double *value, const QString &tip) {
    auto *box = new QDoubleSpinBox;
    box->setRange(0.1, 1000.0);
    box->setDecimals(1);
    box->setSingleStep(0.1);
    box->setSuffix(tr(" ticks/s"));
    box->setValue(*value);
    box->setFocusPolicy(Qt::StrongFocus);
    Card::addPair(g, row, 1, label, box, tip);
    connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, value](double v) {
      *value = v;
      s_->touch();
    });
  };
  rate(1, tr("Speed awake"), &tickRates().awake,
       tr("Game ticks per real second at your speed setting. Count the ticks over a stretch of time; "
          "Normal speed is about 4.2"));
  rate(2, tr("Speed asleep"), &tickRates().asleep,
       tr("While the party sleeps the game ticks on every vertical blank, far faster than awake; "
          "58.8 was measured"));
  column->addWidget(card);
  card->setCollapsed(true);
}

void Sidebar::fillGroups()
{
  groupBox_->clear();
  groupBox_->addItem(tr("A fresh monster"), -1);
  if (s_->save && s_->levelIndex >= 0) {
    const SavedLevel &l = s_->save->levels[size_t(s_->levelIndex)];
    for (size_t i = 0; i < l.groups.size(); i++) {
      const SavedGroup &grp = l.groups[i];
      QString hp;
      for (int k = 0; k < grp.count; k++) hp += (k ? QStringLiteral(", ") : QString()) + QString::number(grp.hp[k]);
      groupBox_->addItem(tr("%1 ×%2 at %3,%4 (%5 hp)")
                           .arg(QString::fromStdString(s_->data.monsters[size_t(qMin(grp.type, 26))].name))
                           .arg(grp.count).arg(grp.x).arg(grp.y).arg(hp), int(i));
    }
  }
  selectData(groupBox_, s_->groupIndex);
  groupBox_->setEnabled(groupBox_->count() > 1);
}

void Sidebar::refill()
{
  filling_ = true;
  const GameData &d = s_->data;
  heroBox_->clear();
  if (s_->save) {
    for (const SavedHero &sh : s_->save->heroes)
      heroBox_->addItem(QString::fromStdString(sh.hero.name + "  " + sh.title));
    heroBox_->setCurrentIndex(s_->heroIndex);
    heroNote_->setText(tr("Values from the savegame. A changed field is marked; right-click puts its value back."));
  } else {
    heroBox_->addItem(tr("A hero of your own"));
    heroNote_->setText(tr("Open a savegame to fill these in from a real party."));
  }
  heroBox_->setEnabled(s_->save != nullptr);

  weaponBox_->clear();
  weaponBox_->addItem(tr("Bare hands"), -1);
  for (const WeaponType &w : d.weapons) weaponBox_->addItem(QString::fromStdString(w.name), w.id);
  selectData(weaponBox_, s_->hero.weapon);
  static const int slotBit[6] = {0, 0, 0x02, 0x08, 0x10, 0x20};
  for (int slot : {0, 2, 3, 4, 5}) {
    QComboBox *box = armourBox_[slot];
    box->clear();
    box->addItem(slot == 0 ? tr("No shield") : tr("Nothing"), -1);
    for (const ArmourType &a : d.armours) {
      bool fits = slot == 0 ? a.isShield() : (a.wearSlots & slotBit[slot]) != 0;
      if (fits || a.id == s_->hero.armour[slot])
        box->addItem(tr("%1  (%2)").arg(QString::fromStdString(a.name)).arg(a.defense), a.id);
    }
    selectData(box, s_->hero.armour[slot]);
  }

  levelBox_->clear();
  levelBox_->addItem(tr("Any level"), -1);
  if (s_->save)
    for (size_t i = 0; i < s_->save->levels.size(); i++) {
      const SavedLevel &l = s_->save->levels[i];
      levelBox_->addItem(tr("Level %1  (factor %2, %3 groups)%4").arg(i).arg(l.expMultiplier)
                           .arg(l.groups.size()).arg(int(i) == s_->save->level ? tr("  ← party") : QString()),
                         int(i));
    }
  selectData(levelBox_, s_->levelIndex);
  levelBox_->setEnabled(levelBox_->count() > 1);
  fillGroups();
  monsterBox_->clear();
  for (const MonsterType &m : d.monsters) monsterBox_->addItem(QString::fromStdString(m.name), m.id);
  selectData(monsterBox_, s_->target.type.id);
  selectData(kindBox_, s_->target.type.attackKind);
  const MonsterType &m = s_->target.type;
  monsterNote_->setText(tr("A fresh one on this level has %1–%2 hit points.")
                          .arg(monsterHpMin(m, qMax(1, s_->target.level)))
                          .arg(monsterHpMax(m, qMax(1, s_->target.level))));

  tablesBox_->clear();
  if (!s_->graphicsPath.isEmpty()) tablesBox_->addItem(tr("graphics.dat beside the savegame"), TablesFromFile);
  tablesBox_->addItem(tr("Dungeon Master (built in)"), TablesDungeonMaster);
  tablesBox_->addItem(tr("Chaos Strikes Back (built in)"), TablesChaosStrikesBack);
  selectData(tablesBox_, s_->tableSource);
  tablesNote_->setText(s_->tableSource == TablesFromFile ? s_->graphicsPath
                                                         : QString::fromStdString(d.source));
  filling_ = false;
}

}  // namespace ui
