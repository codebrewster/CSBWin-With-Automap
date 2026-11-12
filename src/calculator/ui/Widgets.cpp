#include "Widgets.h"

#include <QContextMenuEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QVBoxLayout>
#include <cmath>

#include "Theme.h"

namespace ui {

QString num(double v, int decimals)
{
  if (!std::isfinite(v)) return QStringLiteral("–");
  return QString::number(v, 'f', decimals);
}

QString percent(double share, int decimals)
{
  return QString::number(100.0 * share, 'f', decimals) + QStringLiteral(" %");
}

QString duration(double seconds)
{
  if (!std::isfinite(seconds) || seconds < 0) return QStringLiteral("–");
  if (seconds < 60) return QString::number(seconds, 'f', seconds < 10 ? 1 : 0) + QStringLiteral(" s");
  long s = std::lround(seconds);
  if (s < 3600) return QStringLiteral("%1 min %2 s").arg(s / 60).arg(s % 60);
  return QStringLiteral("%1 h %2 min").arg(s / 3600).arg((s % 3600) / 60);
}

QString ticksAndTime(double ticks)
{
  return QStringLiteral("%1 ticks (%2)")
    .arg(QString::number(ticks, 'f', 0), duration(ticks / calc::ticksPerSecond()));
}

QLabel *mutedLabel(const QString &text)
{
  auto *l = new QLabel(text);
  l->setObjectName(QStringLiteral("fieldLabel"));
  return l;
}

QLabel *noteLabel(const QString &text)
{
  auto *l = new QLabel(text);
  l->setObjectName(QStringLiteral("note"));
  l->setWordWrap(true);
  return l;
}

// ------------------------------------------------------------ fields
IntField::IntField(Session *session, int *value, const int *base, int min, int max, QWidget *parent)
    : QSpinBox(parent), session_(session), value_(value), base_(base)
{
  setRange(min, max);
  setButtonSymbols(QAbstractSpinBox::NoButtons);
  setAlignment(Qt::AlignRight);
  setKeyboardTracking(true);
  setMinimumWidth(58);
  setFocusPolicy(Qt::StrongFocus);
  refresh();
  connect(this, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
    if (loading_) return;
    *value_ = v;
    mark();
    session_->touch();
  });
  connect(session, &Session::reloaded, this, &IntField::refresh);
}

void IntField::refresh()
{
  loading_ = true;
  setValue(*value_);
  loading_ = false;
  mark();
}

void IntField::mark()
{
  bool edited = base_ && session_->hasBase && *value_ != *base_;
  if (property("edited").toBool() != edited) {
    setProperty("edited", edited);
    style()->unpolish(this);
    style()->polish(this);
  }
  setToolTip(edited ? tr("Savegame: %1 (right-click to put it back)").arg(*base_) : QString());
}

void IntField::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
  QAction *reset = nullptr;
  if (base_ && session_->hasBase && *value_ != *base_)
    reset = menu.addAction(tr("Put back the savegame's value (%1)").arg(*base_));
  if (!reset) {
    QSpinBox::contextMenuEvent(event);
    return;
  }
  if (menu.exec(event->globalPos()) == reset) setValue(*base_);
}

void IntField::wheelEvent(QWheelEvent *event)
{
  // only a field one is typing in follows the wheel; the panel scrolls otherwise
  if (hasFocus()) QSpinBox::wheelEvent(event);
  else event->ignore();
}

BoolField::BoolField(const QString &text, Session *session, bool *value, QWidget *parent)
    : QCheckBox(text, parent), value_(value)
{
  refresh();
  connect(this, &QCheckBox::toggled, this, [this, session](bool on) {
    if (loading_) return;
    *value_ = on;
    session->touch();
  });
  connect(session, &Session::reloaded, this, &BoolField::refresh);
}

void BoolField::refresh()
{
  loading_ = true;
  setChecked(*value_);
  loading_ = false;
}

BitField::BitField(const QString &text, Session *session, int *value, int bit, QWidget *parent)
    : QCheckBox(text, parent), value_(value), bit_(bit)
{
  refresh();
  connect(this, &QCheckBox::toggled, this, [this, session](bool on) {
    if (loading_) return;
    *value_ = on ? (*value_ | bit_) : (*value_ & ~bit_);
    session->touch();
  });
  connect(session, &Session::reloaded, this, &BitField::refresh);
}

void BitField::refresh()
{
  loading_ = true;
  setChecked((*value_ & bit_) != 0);
  loading_ = false;
}

// ------------------------------------------------------------ card, tile
Card::Card(const QString &title, QWidget *parent, bool collapsible) : QFrame(parent), text_(title)
{
  setObjectName(QStringLiteral("card"));
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(14, 10, 14, 12);
  outer->setSpacing(8);
  title_ = new QPushButton;
  title_->setObjectName(QStringLiteral("cardTitle"));
  title_->setFlat(true);
  title_->setCursor(collapsible ? Qt::PointingHandCursor : Qt::ArrowCursor);
  title_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  outer->addWidget(title_);
  content_ = new QWidget;
  body_ = new QVBoxLayout(content_);
  body_->setContentsMargins(0, 0, 0, 0);
  body_->setSpacing(8);
  outer->addWidget(content_);
  setCollapsed(false);
  if (collapsible)
    connect(title_, &QPushButton::clicked, this, [this] { setCollapsed(content_->isVisibleTo(this)); });
}

void Card::setCollapsed(bool collapsed)
{
  content_->setVisible(!collapsed);
  title_->setText((collapsed ? QStringLiteral("▸  ") : QStringLiteral("▾  ")) + text_.toUpper());
}

QGridLayout *Card::addGrid()
{
  auto *grid = new QGridLayout;
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  body_->addLayout(grid);
  return grid;
}

void Card::addPair(QGridLayout *grid, int index, int columns, const QString &label, QWidget *field,
                   const QString &tip)
{
  int row = index / columns, col = (index % columns) * 2;
  QLabel *l = mutedLabel(label);
  if (!tip.isEmpty()) {
    l->setToolTip(tip);
    if (field->toolTip().isEmpty()) field->setToolTip(tip);
  }
  grid->addWidget(l, row, col);
  grid->addWidget(field, row, col + 1);
  grid->setColumnStretch(col + 1, 1);
}

Tile::Tile(const QString &label, QWidget *parent) : QFrame(parent)
{
  setObjectName(QStringLiteral("tile"));
  setMinimumWidth(150);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto *l = new QVBoxLayout(this);
  l->setContentsMargins(14, 10, 14, 10);
  l->setSpacing(2);
  label_ = new QLabel(label.toUpper());
  label_->setObjectName(QStringLiteral("tileLabel"));
  value_ = new QLabel(QStringLiteral("–"));
  value_->setObjectName(QStringLiteral("tileValue"));
  note_ = new QLabel;
  note_->setObjectName(QStringLiteral("tileNote"));
  l->addWidget(label_);
  l->addWidget(value_);
  l->addWidget(note_);
}

void Tile::set(const QString &value, const QString &note)
{
  value_->setText(value);
  note_->setText(note.isEmpty() ? QStringLiteral(" ") : note);
}

void Tile::setTone(const QColor &color)
{
  value_->setStyleSheet(color.isValid() ? QStringLiteral("color: %1;").arg(color.name()) : QString());
}

// ------------------------------------------------------------ flow layout
FlowLayout::FlowLayout(QWidget *parent, int spacing) : QLayout(parent), spacing_(spacing)
{
  setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
  while (QLayoutItem *item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem *item) { items_.append(item); }

QLayoutItem *FlowLayout::takeAt(int index)
{
  return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

void FlowLayout::setGeometry(const QRect &rect)
{
  QLayout::setGeometry(rect);
  arrange(rect, false);
}

QSize FlowLayout::minimumSize() const
{
  QSize size;
  for (QLayoutItem *item : items_) size = size.expandedTo(item->minimumSize());
  return size;
}

// Rows of equal-width items: as many to a row as fit at their preferred width
int FlowLayout::arrange(const QRect &rect, bool testOnly) const
{
  QList<QLayoutItem *> shown;
  for (QLayoutItem *item : items_)
    if (!item->isEmpty()) shown.append(item);
  if (shown.isEmpty()) return 0;
  int want = 0;
  for (QLayoutItem *item : shown) want = qMax(want, item->sizeHint().width());
  int perRow = qMax(1, (rect.width() + spacing_) / (want + spacing_));
  perRow = qMin(perRow, int(shown.size()));
  int width = (rect.width() - (perRow - 1) * spacing_) / perRow;
  int y = rect.y(), rowHeight = 0;
  for (int i = 0; i < shown.size(); i++) {
    int col = i % perRow;
    if (col == 0 && i > 0) {
      y += rowHeight + spacing_;
      rowHeight = 0;
    }
    int h = shown[i]->sizeHint().height();
    rowHeight = qMax(rowHeight, h);
    if (!testOnly) shown[i]->setGeometry(QRect(rect.x() + col * (width + spacing_), y, width, h));
  }
  return y + rowHeight - rect.y();
}

// ------------------------------------------------------------ histogram
Histogram::Histogram(QWidget *parent) : QWidget(parent)
{
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void Histogram::set(const calc::Dist &bars, const QString &title, const QString &unit)
{
  bars_ = bars;
  bars_.trim();
  title_ = title;
  unit_ = unit;
  update();
}

void Histogram::setOutline(const calc::Dist &outline, const QString &name)
{
  outline_ = outline;
  outline_.trim();
  outlineName_ = name;
  update();
}

void Histogram::setMarker(int value, const QString &name)
{
  marker_ = value;
  markerName_ = name;
  update();
}

void Histogram::clearExtras()
{
  outline_ = calc::Dist();
  outlineName_.clear();
  marker_ = -1;
  markerName_.clear();
  update();
}

void Histogram::paintEvent(QPaintEvent *)
{
  const Theme &t = theme();
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(t.border);
  p.setBrush(t.surface);
  p.drawRoundedRect(r, 10, 10);
  QFont small = font();
  small.setPixelSize(11);
  p.setFont(small);
  p.setPen(t.muted);
  p.drawText(r.adjusted(14, 8, -14, 0), Qt::AlignLeft | Qt::AlignTop, title_.toUpper());
  if (!outlineName_.isEmpty())
    p.drawText(r.adjusted(14, 8, -14, 0), Qt::AlignRight | Qt::AlignTop,
               tr("outline: %1").arg(outlineName_));
  plot_ = r.adjusted(14, 30, -14, -24);
  if (bars_.empty() && outline_.empty()) {
    p.drawText(plot_, Qt::AlignCenter, tr("no damage"));
    return;
  }
  lo_ = bars_.empty() ? outline_.lo : bars_.lo;
  hi_ = bars_.empty() ? outline_.hi() : bars_.hi();
  if (!outline_.empty()) {
    lo_ = qMin(lo_, outline_.lo);
    hi_ = qMax(hi_, outline_.hi());
  }
  if (marker_ >= 0 && marker_ <= hi_ + (hi_ - lo_ + 1)) hi_ = qMax(hi_, marker_);
  double top = 0;
  for (double v : bars_.p) top = qMax(top, v);
  for (double v : outline_.p) top = qMax(top, v);
  if (top <= 0) top = 1;
  const int n = hi_ - lo_ + 1;
  const double w = plot_.width() / n;
  auto xOf = [&](int v) { return plot_.left() + (v - lo_) * w; };
  // bars
  p.setPen(Qt::NoPen);
  for (size_t i = 0; i < bars_.p.size(); i++) {
    int v = bars_.lo + int(i);
    double h = bars_.p[i] / top * plot_.height();
    if (h <= 0) continue;
    QRectF bar(xOf(v) + (w > 3 ? 0.5 : 0), plot_.bottom() - h, qMax(1.0, w - (w > 3 ? 1 : 0)), h);
    p.setBrush(v == hover_ ? t.text : t.accent);
    p.drawRect(bar);
  }
  if (!outline_.empty()) {
    QPainterPath path;
    path.moveTo(xOf(outline_.lo), plot_.bottom());
    for (size_t i = 0; i < outline_.p.size(); i++) {
      double y = plot_.bottom() - outline_.p[i] / top * plot_.height();
      path.lineTo(xOf(outline_.lo + int(i)), y);
      path.lineTo(xOf(outline_.lo + int(i) + 1), y);
    }
    path.lineTo(xOf(outline_.hi() + 1), plot_.bottom());
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(t.muted, 1.2, Qt::DashLine));
    p.drawPath(path);
  }
  if (marker_ >= lo_ && marker_ <= hi_) {
    p.setPen(QPen(t.bad, 1.4));
    double x = xOf(marker_);
    p.drawLine(QPointF(x, plot_.top()), QPointF(x, plot_.bottom()));
    p.drawText(QRectF(x - 80, plot_.top() - 2, 76, 14), Qt::AlignRight | Qt::AlignTop, markerName_);
  }
  // axis
  p.setPen(t.border);
  p.drawLine(plot_.bottomLeft(), plot_.bottomRight());
  p.setPen(t.muted);
  QRectF axis(plot_.left(), plot_.bottom() + 3, plot_.width(), 16);
  p.drawText(axis, Qt::AlignLeft | Qt::AlignTop, QString::number(lo_));
  p.drawText(axis, Qt::AlignRight | Qt::AlignTop, QString::number(hi_) + (unit_.isEmpty() ? QString() : QStringLiteral(" ") + unit_));
  if (n > 8) p.drawText(axis, Qt::AlignHCenter | Qt::AlignTop, QString::number((lo_ + hi_) / 2));
}

void Histogram::mouseMoveEvent(QMouseEvent *event)
{
  if (plot_.width() <= 0 || (bars_.empty() && outline_.empty())) return;
  const int n = hi_ - lo_ + 1;
  int v = lo_ + int((event->pos().x() - plot_.left()) / (plot_.width() / n));
  if (v < lo_ || v > hi_) {
    leaveEvent(nullptr);
    return;
  }
  if (v != hover_) {
    hover_ = v;
    update();
  }
  QString tip = tr("%1%2: %3").arg(v).arg(unit_.isEmpty() ? QString() : QStringLiteral(" ") + unit_,
                                         percent(bars_.at(v), 2));
  tip += tr("\n%1 or more: %2").arg(v).arg(percent(bars_.atLeast(v), 1));
  if (!outline_.empty()) tip += tr("\n%1: %2").arg(outlineName_, percent(outline_.at(v), 2));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QToolTip::showText(event->globalPosition().toPoint(), tip, this);
#else
  QToolTip::showText(event->globalPos(), tip, this);
#endif
}

void Histogram::leaveEvent(QEvent *)
{
  if (hover_ != INT_MIN) {
    hover_ = INT_MIN;
    update();
  }
  QToolTip::hideText();
}

// ------------------------------------------------------------ line chart
LineChart::LineChart(QWidget *parent) : QWidget(parent)
{
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LineChart::set(const QVector<Series> &series, const QString &xLabel, const QString &yLabel,
                    const QString &rightLabel)
{
  series_ = series;
  xLabel_ = xLabel;
  yLabel_ = yLabel;
  rightLabel_ = rightLabel;
  update();
}

void LineChart::setMarks(const QVector<ChartMark> &marks)
{
  marks_ = marks;
  update();
}

static double niceCeil(double v)
{
  if (v <= 0) return 1;
  double e = std::pow(10.0, std::floor(std::log10(v)));
  for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
    if (m * e >= v) return m * e;
  return 10 * e;
}

void LineChart::paintEvent(QPaintEvent *)
{
  const Theme &t = theme();
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(t.border);
  p.setBrush(t.surface);
  p.drawRoundedRect(r, 10, 10);
  QFont small = font();
  small.setPixelSize(11);
  p.setFont(small);
  QFontMetricsF fm(small);
  // legend
  double lx = r.left() + 14, ly = r.top() + 10;
  for (const Series &s : series_) {
    double w = fm.horizontalAdvance(s.name) + 26;
    if (lx + w > r.right() - 10) {
      lx = r.left() + 14;
      ly += 16;
    }
    p.setPen(QPen(s.color, 2.2));
    p.drawLine(QPointF(lx, ly + 6), QPointF(lx + 14, ly + 6));
    p.setPen(t.muted);
    p.drawText(QPointF(lx + 18, ly + 10), s.name);
    lx += w + 6;
  }
  bool hasRight = false;
  x0_ = 1e300;
  x1_ = -1e300;
  double ymax = 0, rmax = 0;
  for (const Series &s : series_) {
    for (const QPointF &pt : s.points) {
      x0_ = qMin(x0_, pt.x());
      x1_ = qMax(x1_, pt.x());
      (s.rightAxis ? rmax : ymax) = qMax(s.rightAxis ? rmax : ymax, pt.y());
    }
    hasRight = hasRight || s.rightAxis;
  }
  plot_ = r.adjusted(52, ly - r.top() + 40, hasRight ? -52 : -18, -34);
  if (x1_ < x0_) {
    p.setPen(t.muted);
    p.drawText(plot_, Qt::AlignCenter, tr("nothing to draw"));
    return;
  }
  if (x1_ == x0_) x1_ = x0_ + 1;
  y1_ = niceCeil(ymax);
  r1_ = niceCeil(rmax);
  auto px = [&](double x) { return plot_.left() + (x - x0_) / (x1_ - x0_) * plot_.width(); };
  auto py = [&](double y, bool right) {
    return plot_.bottom() - y / (right ? r1_ : y1_) * plot_.height();
  };
  // grid and axis labels
  for (int i = 0; i <= 4; i++) {
    double y = plot_.bottom() - plot_.height() * i / 4.0;
    p.setPen(QPen(t.border, 1, i ? Qt::DotLine : Qt::SolidLine));
    p.drawLine(QPointF(plot_.left(), y), QPointF(plot_.right(), y));
    p.setPen(t.muted);
    p.drawText(QRectF(r.left() + 2, y - 8, 44, 16), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(y1_ * i / 4.0, 'g', 4));
    if (hasRight)
      p.drawText(QRectF(plot_.right() + 6, y - 8, 44, 16), Qt::AlignLeft | Qt::AlignVCenter,
                 QString::number(r1_ * i / 4.0, 'g', 4));
  }
  auto xText = [&](double x) { return xFormat_ ? xFormat_(x) : QString::number(x, 'g', 5); };
  for (int i = 0; i <= 4; i++) {
    double x = x0_ + (x1_ - x0_) * i / 4.0;
    Qt::Alignment a = i == 0 ? Qt::AlignLeft : (i == 4 ? Qt::AlignRight : Qt::AlignHCenter);
    double left = i == 0 ? px(x) : (i == 4 ? px(x) - 120 : px(x) - 60);
    p.drawText(QRectF(left, plot_.bottom() + 3, 120, 14), a | Qt::AlignTop, xText(x));
  }
  p.drawText(QRectF(plot_.left(), plot_.bottom() + 17, plot_.width(), 14), Qt::AlignCenter, xLabel_);
  p.drawText(QRectF(r.left() + 14, plot_.top() - 20, 160, 14), Qt::AlignLeft, yLabel_);
  if (hasRight)
    p.drawText(QRectF(r.right() - 174, plot_.top() - 20, 160, 14), Qt::AlignRight, rightLabel_);
  // marks
  for (const ChartMark &m : marks_) {
    if (m.x < x0_ || m.x > x1_) continue;
    p.setPen(QPen(m.color, 1, Qt::DashLine));
    p.drawLine(QPointF(px(m.x), plot_.top()), QPointF(px(m.x), plot_.bottom()));
  }
  // lines
  p.setClipRect(plot_.adjusted(-2, -2, 2, 2));
  for (const Series &s : series_) {
    if (s.points.isEmpty()) continue;
    QPainterPath path;
    path.moveTo(px(s.points[0].x()), py(s.points[0].y(), s.rightAxis));
    for (int i = 1; i < s.points.size(); i++)
      path.lineTo(px(s.points[i].x()), py(s.points[i].y(), s.rightAxis));
    p.setPen(QPen(s.color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    if (s.points.size() <= 40) {
      p.setBrush(s.color);
      for (const QPointF &pt : s.points) p.drawEllipse(QPointF(px(pt.x()), py(pt.y(), s.rightAxis)), 2.4, 2.4);
    }
  }
  p.setClipping(false);
  if (hovering_ && hoverX_ >= x0_ && hoverX_ <= x1_) {
    p.setPen(QPen(t.muted, 1));
    p.drawLine(QPointF(px(hoverX_), plot_.top()), QPointF(px(hoverX_), plot_.bottom()));
  }
}

void LineChart::mouseMoveEvent(QMouseEvent *event)
{
  if (plot_.width() <= 0 || series_.isEmpty()) return;
  double x = x0_ + (event->pos().x() - plot_.left()) / plot_.width() * (x1_ - x0_);
  if (x < x0_ || x > x1_) {
    leaveEvent(nullptr);
    return;
  }
  // snap to the nearest point of the first series
  double best = x;
  double bestDist = 1e300;
  for (const Series &s : series_)
    for (const QPointF &pt : s.points)
      if (std::fabs(pt.x() - x) < bestDist) {
        bestDist = std::fabs(pt.x() - x);
        best = pt.x();
      }
  hoverX_ = best;
  hovering_ = true;
  QString tip = (xLabel_.isEmpty() ? QString() : xLabel_ + QStringLiteral(": ")) +
                (xFormat_ ? xFormat_(best) : QString::number(best, 'g', 6));
  for (const Series &s : series_) {
    double y = 0, d = 1e300;
    for (const QPointF &pt : s.points)
      if (std::fabs(pt.x() - best) < d) {
        d = std::fabs(pt.x() - best);
        y = pt.y();
      }
    tip += QStringLiteral("\n%1: %2").arg(s.name, QString::number(y, 'g', 6));
  }
  for (const ChartMark &m : marks_)
    if (std::fabs(m.x - best) <= (x1_ - x0_) / 60) tip += QStringLiteral("\n") + m.name;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QToolTip::showText(event->globalPosition().toPoint(), tip, this);
#else
  QToolTip::showText(event->globalPos(), tip, this);
#endif
  update();
}

void LineChart::leaveEvent(QEvent *)
{
  if (hovering_) {
    hovering_ = false;
    update();
  }
  QToolTip::hideText();
}

}  // namespace ui
