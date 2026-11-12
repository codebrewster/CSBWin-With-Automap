#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace ui {

namespace {
Theme current;

Theme make(bool dark)
{
  Theme t;
  t.dark = dark;
  if (dark) {
    t.window = QColor("#16181d");
    t.surface = QColor("#1e2128");
    t.raised = QColor("#272b34");
    t.border = QColor("#353a46");
    t.text = QColor("#e6e8ec");
    t.muted = QColor("#9299a8");
    t.accent = QColor("#e0a458");
    t.accentSoft = QColor("#4a3a22");
    t.good = QColor("#6fbf8b");
    t.warn = QColor("#e0a458");
    t.bad = QColor("#e07a6a");
    t.series[0] = QColor("#e07a6a");
    t.series[1] = QColor("#e0c15a");
    t.series[2] = QColor("#6aa9e0");
    t.series[3] = QColor("#c08a5a");
    t.series[4] = QColor("#5ac0c0");
  } else {
    t.window = QColor("#f3f1ec");
    t.surface = QColor("#fbfaf7");
    t.raised = QColor("#ffffff");
    t.border = QColor("#d9d4c8");
    t.text = QColor("#23252b");
    t.muted = QColor("#6b6f7a");
    t.accent = QColor("#a86a1c");
    t.accentSoft = QColor("#f3e3c8");
    t.good = QColor("#2f7d4f");
    t.warn = QColor("#a86a1c");
    t.bad = QColor("#b8443a");
    t.series[0] = QColor("#b8443a");
    t.series[1] = QColor("#b08a1a");
    t.series[2] = QColor("#2f6fb0");
    t.series[3] = QColor("#8a5a2c");
    t.series[4] = QColor("#1f8a8a");
  }
  return t;
}

QString sheet(const Theme &t)
{
  QString s = QStringLiteral(R"(
QWidget { color: %text%; font-size: 13px; }
QMainWindow, #page, #sidebarBody { background: %window%; }
QToolTip { background: %raised%; color: %text%; border: 1px solid %border%; padding: 4px 6px; }
QScrollArea { border: none; background: transparent; }
#card { background: %surface%; border: 1px solid %border%; border-radius: 10px; }
#cardTitle { font-size: 12px; font-weight: 600; letter-spacing: 1px; color: %muted%;
             border: none; background: transparent; text-align: left; padding: 2px 0; }
#cardTitle:hover { color: %accent%; }
#tile { background: %raised%; border: 1px solid %border%; border-radius: 10px; }
#tileValue { font-size: 22px; font-weight: 600; }
#tileLabel { font-size: 11px; color: %muted%; letter-spacing: 0.5px; }
#tileNote { font-size: 11px; color: %muted%; }
#muted, #fieldLabel { color: %muted%; }
#heading { font-size: 15px; font-weight: 600; }
#note { color: %muted%; font-size: 12px; }
#warning { color: %bad%; font-size: 12px; }
#topBar { background: %surface%; border-bottom: 1px solid %border%; }
#saveLabel { color: %muted%; }
QPushButton { background: %raised%; border: 1px solid %border%; border-radius: 7px; padding: 6px 14px; }
QPushButton:hover { border-color: %accent%; }
QPushButton:pressed { background: %accentSoft%; }
QPushButton#cardTitle { font-size: 12px; font-weight: 600; letter-spacing: 1px; color: %muted%;
             border: none; background: transparent; text-align: left; padding: 2px 0; }
QPushButton#cardTitle:hover { color: %accent%; }
QPushButton#primary { background: %accent%; color: %window%; border-color: %accent%; font-weight: 600; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 3px 6px; color: %muted%; }
QToolButton:hover { background: %accentSoft%; color: %text%; }
QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit {
  background: %raised%; border: 1px solid %border%; border-radius: 6px; padding: 3px 6px; min-height: 20px;
  selection-background-color: %accent%; selection-color: %window%; }
QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QLineEdit:focus { border-color: %accent%; }
QSpinBox[edited="true"] { border-color: %accent%; background: %accentSoft%; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; border: none; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: %raised%; border: 1px solid %border%; selection-background-color: %accentSoft%; selection-color: %text%; outline: none; }
QCheckBox { spacing: 6px; }
QTabWidget::pane { border: none; }
QTabBar { background: transparent; qproperty-drawBase: 0; }
QTabBar::tab { background: transparent; color: %muted%; padding: 9px 16px; margin-right: 2px;
               border: none; border-bottom: 2px solid transparent; font-weight: 600; }
QTabBar::tab:hover { color: %text%; }
QTabBar::tab:selected { color: %accent%; border-bottom: 2px solid %accent%; }
QTableView { background: %surface%; alternate-background-color: %window%; border: 1px solid %border%;
             border-radius: 10px; gridline-color: transparent; selection-background-color: %accentSoft%;
             selection-color: %text%; outline: none; }
QTableView::item { padding: 4px 8px; border: none; }
QHeaderView { background: transparent; }
QHeaderView::section { background: %surface%; color: %muted%; border: none; border-bottom: 1px solid %border%;
                       padding: 7px 8px; font-size: 11px; font-weight: 600; }
QTableCornerButton::section { background: %surface%; border: none; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: %border%; border-radius: 4px; min-height: 28px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: %border%; border-radius: 4px; min-width: 28px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSplitter::handle { background: %border%; width: 1px; }
)");
  const std::pair<const char *, QColor> names[] = {
    {"%window%", t.window}, {"%surface%", t.surface}, {"%raised%", t.raised},
    {"%border%", t.border}, {"%text%", t.text}, {"%muted%", t.muted}, {"%accent%", t.accent},
    {"%accentSoft%", t.accentSoft}, {"%bad%", t.bad}};
  for (auto &n : names) s.replace(QLatin1String(n.first), n.second.name());
  return s;
}
}  // namespace

const Theme &theme() { return current; }

bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
}

void applyTheme(QApplication &app, bool dark)
{
  current = make(dark);
  const Theme &t = current;
  app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  QPalette p;
  p.setColor(QPalette::Window, t.window);
  p.setColor(QPalette::WindowText, t.text);
  p.setColor(QPalette::Base, t.raised);
  p.setColor(QPalette::AlternateBase, t.window);
  p.setColor(QPalette::Text, t.text);
  p.setColor(QPalette::Button, t.raised);
  p.setColor(QPalette::ButtonText, t.text);
  p.setColor(QPalette::Highlight, t.accent);
  p.setColor(QPalette::HighlightedText, t.window);
  p.setColor(QPalette::ToolTipBase, t.raised);
  p.setColor(QPalette::ToolTipText, t.text);
  p.setColor(QPalette::PlaceholderText, t.muted);
  p.setColor(QPalette::Disabled, QPalette::Text, t.muted);
  p.setColor(QPalette::Disabled, QPalette::WindowText, t.muted);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, t.muted);
  app.setPalette(p);
  app.setStyleSheet(sheet(t));
}

}  // namespace ui
