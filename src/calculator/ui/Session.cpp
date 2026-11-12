#include "Session.h"

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

QString Session::summary() const
{
  if (!save) return tr("No savegame: all values are typed in");
  return tr("%1  ·  level %2  ·  %3 heroes  ·  tick %4")
    .arg(QString::fromStdString(save->label))
    .arg(save->level)
    .arg(save->heroes.size())
    .arg(save->time);
}

}  // namespace ui
