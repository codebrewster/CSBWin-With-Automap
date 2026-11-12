// The small building blocks of the window: input fields bound to a value of
// the session, cards, result tiles, a wrapping layout and two charts.
#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVector>
#include <QWidget>
#include <functional>

#include "../core/Dist.h"
#include "Session.h"

class QFormLayout;
class QGridLayout;
class QVBoxLayout;

namespace ui {

QString num(double v, int decimals = 1);
QString percent(double share, int decimals = 1);
QString duration(double seconds);          // "12.4 s", "3 min 20 s", "1 h 5 min"
QString ticksAndTime(double ticks);         // "384 ticks (1 min 4 s)"

// A whole number of the session.  It is marked when it differs from the
// value the savegame gave; the context menu puts that value back.
class IntField : public QSpinBox {
  Q_OBJECT
 public:
  IntField(Session *session, int *value, const int *base, int min, int max,
           QWidget *parent = nullptr);
  void refresh();
 protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
 private:
  void mark();
  Session *session_;
  int *value_;
  const int *base_;
  bool loading_ = false;
};

class BoolField : public QCheckBox {
  Q_OBJECT
 public:
  BoolField(const QString &text, Session *session, bool *value, QWidget *parent = nullptr);
  void refresh();
 private:
  bool *value_;
  bool loading_ = false;
};

// One bit of a mask
class BitField : public QCheckBox {
  Q_OBJECT
 public:
  BitField(const QString &text, Session *session, int *value, int bit, QWidget *parent = nullptr);
  void refresh();
 private:
  int *value_;
  int bit_;
  bool loading_ = false;
};

// A titled box whose body folds away
class Card : public QFrame {
  Q_OBJECT
 public:
  explicit Card(const QString &title, QWidget *parent = nullptr, bool collapsible = true);
  QVBoxLayout *body() const { return body_; }
  // A grid of label/field pairs, `columns` pairs to a row
  QGridLayout *addGrid();
  static void addPair(QGridLayout *grid, int index, int columns, const QString &label, QWidget *field,
                      const QString &tip = QString());
  void setCollapsed(bool collapsed);
 private:
  QPushButton *title_;
  QWidget *content_;
  QVBoxLayout *body_;
  QString text_;
};

class Tile : public QFrame {
  Q_OBJECT
 public:
  explicit Tile(const QString &label, QWidget *parent = nullptr);
  void set(const QString &value, const QString &note = QString());
  void setTone(const QColor &color);   // invalid = the normal text colour
 private:
  QLabel *label_, *value_, *note_;
};

// Lays its items out in rows and wraps them (Qt's flow layout example)
class FlowLayout : public QLayout {
 public:
  explicit FlowLayout(QWidget *parent = nullptr, int spacing = 10);
  ~FlowLayout() override;
  void addItem(QLayoutItem *item) override;
  int count() const override { return items_.size(); }
  QLayoutItem *itemAt(int index) const override { return items_.value(index); }
  QLayoutItem *takeAt(int index) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), true); }
  void setGeometry(const QRect &rect) override;
  QSize sizeHint() const override { return minimumSize(); }
  QSize minimumSize() const override;
  Qt::Orientations expandingDirections() const override { return {}; }
 private:
  int arrange(const QRect &rect, bool testOnly) const;
  QList<QLayoutItem *> items_;
  int spacing_;
};

// Bars over the values of a probability table, with an optional outline of a
// second table to compare with
class Histogram : public QWidget {
  Q_OBJECT
 public:
  explicit Histogram(QWidget *parent = nullptr);
  void set(const calc::Dist &bars, const QString &title, const QString &unit = QString());
  void setOutline(const calc::Dist &outline, const QString &name);
  void setMarker(int value, const QString &name);   // a vertical line, e.g. hit points
  void clearExtras();
  QSize sizeHint() const override { return {420, 190}; }
  QSize minimumSizeHint() const override { return {200, 140}; }
 protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
 private:
  calc::Dist bars_, outline_;
  QString title_, unit_, outlineName_, markerName_;
  int marker_ = -1;
  int hover_ = INT_MIN;
  QRectF plot_;
  int lo_ = 0, hi_ = 0;
};

struct Series {
  QString name;
  QColor color;
  QVector<QPointF> points;
  bool rightAxis = false;      // scaled by the right-hand axis
};
struct ChartMark {
  double x;
  QString name;
  QColor color;
};

class LineChart : public QWidget {
  Q_OBJECT
 public:
  explicit LineChart(QWidget *parent = nullptr);
  void set(const QVector<Series> &series, const QString &xLabel, const QString &yLabel,
           const QString &rightLabel = QString());
  void setMarks(const QVector<ChartMark> &marks);
  // How an x value reads on the axis and in the tooltip
  void setXFormat(std::function<QString(double)> format) { xFormat_ = std::move(format); }
  QSize sizeHint() const override { return {520, 260}; }
  QSize minimumSizeHint() const override { return {240, 160}; }
 protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
 private:
  QVector<Series> series_;
  QVector<ChartMark> marks_;
  QString xLabel_, yLabel_, rightLabel_;
  std::function<QString(double)> xFormat_;
  QRectF plot_;
  double x0_ = 0, x1_ = 1, y1_ = 1, r1_ = 1;
  double hoverX_ = -1;
  bool hovering_ = false;
};

QLabel *mutedLabel(const QString &text);
QLabel *noteLabel(const QString &text = QString());

}  // namespace ui
