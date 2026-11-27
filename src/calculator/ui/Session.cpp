#include "Session.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>

#include "../io/Config.h"
#include "../io/GraphicsDat.h"

using namespace calc;

namespace ui {

Session::Session(QObject *parent) : QObject(parent)
{
  data = builtinDungeonMaster();
  // a hero worth looking at before any savegame is opened
  hero.name = "Hero";
  hero.weapon = 10;                         // Sword
  hero.handWeight[1] = data.weapons[10].weight;
  hero.armour[SlotReadyHand] = 31;          // Small Shield
  hero.handWeight[0] = data.armours[31].weight;
  hero.armour[SlotHead] = 26;
  hero.armour[SlotTorso] = 16;
  hero.armour[SlotLegs] = 17;
  hero.armour[SlotFeet] = 4;
  heroBase = hero;
  selectMonsterType(10);                    // Mummy
}

void Session::applyTables()
{
  if (tableSource == TablesFromFile && !graphicsPath.isEmpty()) {
    try {
      data = loadGraphicsDat(graphicsPath.toStdString());
      return;
    } catch (const std::exception &) {
      tableSource = TablesDungeonMaster;
    }
  }
  data = tableSource == TablesChaosStrikesBack ? builtinChaosStrikesBack() : builtinDungeonMaster();
}

void Session::refit()
{
  int id = target.type.id;
  if (id < 0 || id >= int(data.monsters.size())) id = 0;
  int level = target.level, hp = target.hp, count = target.count;
  bool attacking = target.attacking;
  target.type = data.monsters[size_t(id)];
  target.level = level;
  target.hp = hp;
  target.count = count;
  target.attacking = attacking;
  targetBase = target;
}

QString Session::openSave(const QString &path)
{
  // the tables first: the items of a savegame are named and weighed by them
  QFileInfo info(path);
  QString beside;
  for (const QString &name : info.dir().entryList(QDir::Files))
    if (name.compare(QLatin1String("graphics.dat"), Qt::CaseInsensitive) == 0)
      beside = info.dir().filePath(name);
  GameData tables = data;
  QString tablePath;
  if (!beside.isEmpty()) {
    try {
      tables = loadGraphicsDat(beside.toStdString());
      tablePath = beside;
    } catch (const std::exception &) {
    }
  }
  std::unique_ptr<SaveGame> loaded;
  try {
    loaded.reset(new SaveGame(loadSaveGame(path.toStdString(), tables)));
  } catch (const std::exception &e) {
    return QString::fromUtf8(e.what());
  }
  if (loaded->heroes.empty()) return tr("The savegame holds no heroes.");
  save = std::move(loaded);
  savePath = path;
  if (!tablePath.isEmpty()) {
    graphicsPath = tablePath;
    tableSource = TablesFromFile;
    data = tables;
  }
  // the game's rules start from its defaults for every savegame (no csb.cfg = the defaults);
  // dice and luck drift are the calculator's own settings and stay
  {
    const Rules defaults;
    rules.vulnerableOnAttack = defaults.vulnerableOnAttack;
    rules.shotDamageFactor = defaults.shotDamageFactor;
    rules.shootExperience20 = defaults.shootExperience20;
  }
  for (const QString &name : info.dir().entryList(QDir::Files))
    if (name.compare(QLatin1String("csb.cfg"), Qt::CaseInsensitive) == 0)
      readRulesFromConfig(info.dir().filePath(name).toStdString(), rules);
  party = partyBase = save->party;
  gameTime = save->time;
  hasBase = true;
  heroIndex = 0;
  hero = heroBase = save->heroes[0].hero;
  hero.negativeActionIsZero = negativeActionIsZero;
  levelIndex = save->level >= 0 && save->level < int(save->levels.size()) ? save->level : -1;
  groupIndex = -1;
  if (levelIndex >= 0) {
    const SavedLevel &l = save->levels[size_t(levelIndex)];
    target.level = l.expMultiplier;
    if (!l.groups.empty()) {
      selectGroup(0);
    } else {
      resetMonsterHp();
    }
  }
  refit();
  emit reloaded();
  emit changed();
  return QString();
}

void Session::closeSave()
{
  save.reset();
  savePath.clear();
  heroIndex = levelIndex = groupIndex = -1;
  hasBase = false;
  heroBase = hero;
  partyBase = party;
  targetBase = target;
  emit reloaded();
  emit changed();
}

void Session::selectHero(int index)
{
  if (!save || index < 0 || index >= int(save->heroes.size())) return;
  heroIndex = index;
  hero = heroBase = save->heroes[size_t(index)].hero;
  hero.negativeActionIsZero = negativeActionIsZero;
  emit reloaded();
  emit changed();
}

void Session::maxOutHero()
{
  calc::maxOut(hero);
  emit reloaded();
  emit changed();
}

void Session::restoreHero()
{
  if (save) selectHero(heroIndex);
}

void Session::setNegativeActionIsZero(bool on)
{
  negativeActionIsZero = on;
  hero.negativeActionIsZero = on;
  touch();
}

void Session::selectTables(int source)
{
  if (source == TablesFromFile && graphicsPath.isEmpty()) source = TablesDungeonMaster;
  tableSource = source;
  applyTables();
  refit();
  emit reloaded();
  emit changed();
}

void Session::selectLevel(int index)
{
  if (!save || index < 0 || index >= int(save->levels.size())) {
    levelIndex = -1;
    groupIndex = -1;
  } else {
    levelIndex = index;
    groupIndex = -1;
    target.level = save->levels[size_t(index)].expMultiplier;
    if (!save->levels[size_t(index)].groups.empty()) {
      selectGroup(0);
      return;
    }
    resetMonsterHp();
  }
  targetBase = target;
  emit reloaded();
  emit changed();
}

void Session::selectGroup(int index)
{
  if (!save || levelIndex < 0) return;
  const SavedLevel &l = save->levels[size_t(levelIndex)];
  if (index < 0 || index >= int(l.groups.size())) {
    groupIndex = -1;
    resetMonsterHp();
  } else {
    groupIndex = index;
    const SavedGroup &grp = l.groups[size_t(index)];
    if (grp.type < int(data.monsters.size())) target.type = data.monsters[size_t(grp.type)];
    target.count = grp.count;
    target.hp = grp.hp[0];
    for (int k = 1; k < grp.count; k++) target.hp = std::max(target.hp, grp.hp[k]);
  }
  targetBase = target;
  emit reloaded();
  emit changed();
}

void Session::selectMonsterType(int type)
{
  if (type < 0 || type >= int(data.monsters.size())) return;
  target.type = data.monsters[size_t(type)];
  groupIndex = -1;
  if (target.level < 1) target.level = 1;
  resetMonsterHp();
  targetBase = target;
  emit reloaded();
  emit changed();
}

void Session::resetMonsterHp()
{
  // a newly made monster: base health times the level, plus up to a quarter of the base
  target.hp = monsterHpMin(target.type, std::max(1, target.level)) + target.type.baseHealth / 8;
}

QStringList Session::warnings() const
{
  QStringList out;
  if (save)
    for (const std::string &w : save->warnings) out << QString::fromStdString(w);
  return out;
}

namespace {

// Written field by field: a new field of Hero, Party, Target or MonsterType goes in both lists
const qint32 kStateVersion = 1;

template <class T, size_t N>
void putArray(QDataStream &out, const T (&a)[N])
{
  for (const T &v : a) out << qint32(v);
}
template <class T, size_t N>
void getArray(QDataStream &in, T (&a)[N])
{
  for (T &v : a) {
    qint32 x;
    in >> x;
    v = T(x);
  }
}
template <class T>
void get(QDataStream &in, T &v)
{
  qint32 x;
  in >> x;
  v = T(x);
}

void putHero(QDataStream &o, const calc::Hero &h)
{
  o << QString::fromStdString(h.name);
  putArray(o, h.attr); putArray(o, h.attrMin); putArray(o, h.attrMax);
  for (int v : {h.hp, h.maxHp, h.stamina, h.maxStamina, h.mana, h.maxMana, h.load, h.wounds, h.food,
                h.water, h.shield, h.actionDefense, h.weapon, int(h.negativeActionIsZero),
                int(h.elvenBoots), int(h.ekkhardCross), int(h.gemOfAges)})
    o << qint32(v);
  putArray(o, h.mastery); putArray(o, h.armour); putArray(o, h.handWeight);
}
void getHero(QDataStream &i, calc::Hero &h)
{
  QString name;
  i >> name;
  h.name = name.toStdString();
  getArray(i, h.attr); getArray(i, h.attrMin); getArray(i, h.attrMax);
  get(i, h.hp); get(i, h.maxHp); get(i, h.stamina); get(i, h.maxStamina); get(i, h.mana);
  get(i, h.maxMana); get(i, h.load); get(i, h.wounds); get(i, h.food); get(i, h.water);
  get(i, h.shield); get(i, h.actionDefense); get(i, h.weapon); get(i, h.negativeActionIsZero);
  get(i, h.elvenBoots); get(i, h.ekkhardCross); get(i, h.gemOfAges);
  getArray(i, h.mastery); getArray(i, h.armour); getArray(i, h.handWeight);
}

void putMonster(QDataStream &o, const calc::MonsterType &m)
{
  o << QString::fromStdString(m.name) << QString::fromStdString(m.otherName);
  for (int v : {m.id, m.moveTicks, m.attackTicks, m.defense, m.baseHealth, m.attack, m.poison, m.dexterity,
                m.missileClass, m.expClass, m.fireResistance, m.poisonResistance, m.attackPause,
                m.woundNibbles, m.attackKind, m.size, int(m.nonMaterial), int(m.invincible),
                int(m.hitsAnyHero)})
    o << qint32(v);
}
void getMonster(QDataStream &i, calc::MonsterType &m)
{
  QString name, other;
  i >> name >> other;
  m.name = name.toStdString();
  m.otherName = other.toStdString();
  get(i, m.id); get(i, m.moveTicks); get(i, m.attackTicks); get(i, m.defense); get(i, m.baseHealth);
  get(i, m.attack); get(i, m.poison); get(i, m.dexterity); get(i, m.missileClass); get(i, m.expClass);
  get(i, m.fireResistance); get(i, m.poisonResistance); get(i, m.attackPause); get(i, m.woundNibbles);
  get(i, m.attackKind); get(i, m.size); get(i, m.nonMaterial); get(i, m.invincible); get(i, m.hitsAnyHero);
}

}  // namespace

QByteArray Session::saveState() const
{
  QByteArray bytes;
  QDataStream o(&bytes, QIODevice::WriteOnly);
  o << kStateVersion << savePath;
  for (int v : {tableSource, heroIndex, levelIndex, groupIndex, int(negativeActionIsZero)}) o << qint32(v);
  putHero(o, hero);
  for (int v : {party.partyShield, party.spellShield, party.fireShield, int(party.sleeping)}) o << qint32(v);
  putMonster(o, target.type);
  for (int v : {target.level, target.hp, target.count, int(target.attacking)}) o << qint32(v);
  for (int v : {int(rules.vulnerableOnAttack), rules.shotDamageFactor, int(rules.moduloBias),
                int(rules.luckDrift), int(rules.shootExperience20), calc::swingTiming().extra})
    o << qint32(v);
  o << calc::tickRates().awake << calc::tickRates().asleep;
  return bytes;
}

bool Session::restoreState(const QByteArray &state)
{
  QDataStream i(state);
  qint32 version;
  QString path;
  i >> version >> path;
  if (version != kStateVersion || path != savePath) return false;
  int tables, heroAt, levelAt, groupAt;
  get(i, tables); get(i, heroAt); get(i, levelAt); get(i, groupAt); get(i, negativeActionIsZero);
  calc::Hero h;
  getHero(i, h);
  calc::Party p;
  get(i, p.partyShield); get(i, p.spellShield); get(i, p.fireShield); get(i, p.sleeping);
  calc::Target t;
  getMonster(i, t.type);
  get(i, t.level); get(i, t.hp); get(i, t.count); get(i, t.attacking);
  calc::Rules r;
  get(i, r.vulnerableOnAttack); get(i, r.shotDamageFactor); get(i, r.moduloBias); get(i, r.luckDrift);
  get(i, r.shootExperience20); get(i, calc::swingTiming().extra);
  i >> calc::tickRates().awake >> calc::tickRates().asleep;
  if (i.status() != QDataStream::Ok) return false;
  // the tables first, as a click on them would; then what was picked and typed in over the top
  if (tables != tableSource) selectTables(tables);
  if (save && heroAt >= 0 && heroAt < int(save->heroes.size())) {
    heroIndex = heroAt;
    heroBase = save->heroes[size_t(heroAt)].hero;
  }
  // the monster: the level and group (or kind) as the side panel picks them, so the base values
  // for the "changed" marks are the savegame's; the values typed in come on top below
  selectLevel(levelAt);
  if (groupAt >= 0 && levelIndex >= 0) selectGroup(groupAt);
  else selectMonsterType(t.type.id);
  hero = h;
  party = p;
  target = t;
  rules = r;
  emit reloaded();
  emit changed();
  return true;
}

QString Session::summary() const
{
  if (!save) return tr("No savegame: all values are typed in");
  const int n = int(save->heroes.size());
  return tr("%1  ·  level %2  ·  %3  ·  tick %4")
    .arg(QString::fromStdString(save->label))
    .arg(save->level)
    .arg(n == 1 ? tr("1 hero") : tr("%1 heroes").arg(n))
    .arg(save->time);
}

}  // namespace ui
