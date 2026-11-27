#include <thread>
#include <algorithm>
#include <QHeaderView>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Pages.h"

namespace ui {

Page::Page(Session *session, QWidget *parent) : QWidget(parent), s_(session)
{
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  auto *body = new QWidget;
  body->setObjectName(QStringLiteral("page"));
  column_ = new QVBoxLayout(body);
  column_->setContentsMargins(16, 14, 16, 16);
  column_->setSpacing(12);
  scroll->setWidget(body);
  outer->addWidget(scroll);
  timer_.setSingleShot(true);
  timer_.setInterval(40);
  connect(&timer_, &QTimer::timeout, this, [this] {
    if (!isVisible()) return;
    stale_ = false;
    recalculate();
  });
  connect(s_, &Session::changed, this, &Page::schedule);
}

void Page::schedule()
{
  stale_ = true;
  if (isVisible()) timer_.start();
}

void Page::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  if (stale_) timer_.start(0);
}

namespace {
class NumberItem : public QTableWidgetItem {
 public:
  NumberItem(double value, const QString &text) : QTableWidgetItem(text), value_(value)
  {
    setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }
  bool operator<(const QTableWidgetItem &other) const override
  {
    auto *o = dynamic_cast<const NumberItem *>(&other);
    return o ? value_ < o->value_ : QTableWidgetItem::operator<(other);
  }
 private:
  double value_;
};
}  // namespace

QTableWidgetItem *numberCell(double value, const QString &text) { return new NumberItem(value, text); }

QString otherGameTip(const std::string &otherName, bool chaosStrikesBack)
{
  if (otherName.empty()) return QString();
  return chaosStrikesBack
             ? QObject::tr("In Dungeon Master: %1").arg(QString::fromStdString(otherName))
             : QObject::tr("In Chaos Strikes Back: %1").arg(QString::fromStdString(otherName));
}

QString weightNote(const calc::Hero &hero, const calc::WeaponType &weapon, const QString &what)
{
  if (weapon.id < 0) return QString();
  const calc::eng::WeightFit f = calc::eng::weightFit(weapon.weight, calc::eng::maxLoad(hero, hero.stamina));
  if (f.term >= 0) return QString();
  auto kg = [](int tenths) { return QString::number(tenths / 10.0, 'f', 1); };
  const int lost = (-f.term + 1) / 2;   // the power is halved after the weight is counted
  const int need = calc::eng::strengthForWeight(hero, weapon.weight);
  QString text = QObject::tr("%1 is too heavy for this hero: at %2 kg it is %3 kg over the %4 kg the "
                             "hero's strength allows, which takes about %5 off the %6 power.")
                   .arg(QString::fromStdString(weapon.name), kg(weapon.weight),
                        kg(weapon.weight - f.limit), kg(f.limit))
                   .arg(lost)
                   .arg(what);
  if (need > 0)
    text += QObject::tr(" From strength %1 on it is no longer too heavy.").arg(need);
  else
    text += QObject::tr(" Not even strength 255 is enough.");
  return text;
}

QTableWidgetItem *textCell(const QString &text)
{
  auto *item = new QTableWidgetItem(text);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  return item;
}

unsigned workerThreads(size_t jobs)
{
#ifdef Q_OS_WASM
  (void)jobs;
  return 1;
#else
  return std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), unsigned(jobs)));
#endif
}

void styleTable(QTableWidget *table)
{
  table->setAlternatingRowColors(true);
  table->setShowGrid(false);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->hide();
  table->verticalHeader()->setDefaultSectionSize(28);
  table->horizontalHeader()->setHighlightSections(false);
  table->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table->setFocusPolicy(Qt::StrongFocus);
}

void sortBestFirst(QTableWidget *table, const std::vector<int> &textColumns,
                   const std::vector<int> &lowerIsBetter)
{
  // QHeaderView sorts a column clicked for the first time by its InitialSortOrderRole
  auto has = [](const std::vector<int> &v, int c) { return std::find(v.begin(), v.end(), c) != v.end(); };
  for (int c = 0; c < table->columnCount(); c++) {
    QTableWidgetItem *head = table->horizontalHeaderItem(c);
    if (!head) continue;
    const bool up = has(textColumns, c) || has(lowerIsBetter, c);
    head->setData(Qt::InitialSortOrderRole, int(up ? Qt::AscendingOrder : Qt::DescendingOrder));
  }
}

void fitTableHeight(QTableWidget *table)
{
  int h = table->horizontalHeader()->sizeHint().height() + 2 * table->frameWidth() + 2;
  for (int row = 0; row < table->rowCount(); row++) h += table->rowHeight(row);
  table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table->setFixedHeight(h);
}

}  // namespace ui
