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

QTableWidgetItem *textCell(const QString &text)
{
  auto *item = new QTableWidgetItem(text);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  return item;
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

void fitTableHeight(QTableWidget *table)
{
  int h = table->horizontalHeader()->sizeHint().height() + 2 * table->frameWidth() + 2;
  for (int row = 0; row < table->rowCount(); row++) h += table->rowHeight(row);
  table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table->setFixedHeight(h);
}

}  // namespace ui
