#include "MainWindow.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Pages.h"
#include "Sidebar.h"
#include "Theme.h"

namespace ui {

namespace {
const int kNarrow = 980;   // below this width the side panel folds away
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  setWindowTitle(tr("Dungeon Master Calculator"));
  setAcceptDrops(true);
  auto *central = new QWidget;
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *bar = new QWidget;
  bar->setObjectName(QStringLiteral("topBar"));
  auto *barLayout = new QHBoxLayout(bar);
  barLayout->setContentsMargins(12, 8, 12, 8);
  barLayout->setSpacing(10);
  setupButton_ = new QToolButton;
  setupButton_->setText(tr("☰  Setup"));
  setupButton_->setToolTip(tr("Show or hide the hero, monster and rules"));
  barLayout->addWidget(setupButton_);
  auto *open = new QPushButton(tr("Open savegame…"));
  open->setObjectName(QStringLiteral("primary"));
  open->setToolTip(tr("A savegame of Dungeon Master or Chaos Strikes Back (Atari ST, Amiga, CSBwin, "
                      "this port), or an Amiga save disk (.adf). You can also drop the file here."));
  barLayout->addWidget(open);
  saveLabel_ = new QLabel;
  saveLabel_->setObjectName(QStringLiteral("saveLabel"));
  saveLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  barLayout->addWidget(saveLabel_, 1);
  warning_ = new QLabel;
  warning_->setObjectName(QStringLiteral("warning"));
  barLayout->addWidget(warning_);
  themeButton_ = new QToolButton;
  themeButton_->setToolTip(tr("Light or dark"));
  barLayout->addWidget(themeButton_);
  root->addWidget(bar);

  splitter_ = new QSplitter(Qt::Horizontal);
  splitter_->setChildrenCollapsible(false);
  sidebar_ = new Sidebar(&session_);
  sidebar_->setMinimumWidth(300);
  sidebar_->setMaximumWidth(460);
  splitter_->addWidget(sidebar_);
  tabs_ = new QTabWidget;
  tabs_->setDocumentMode(true);
  tabs_->addTab(new HeroAttackPage(&session_), tr("Hero attacks"));
  tabs_->addTab(new MonsterAttackPage(&session_), tr("Monster attacks"));
  tabs_->addTab(new SpellPage(&session_), tr("Spells"));
  tabs_->addTab(new RecoveryPage(&session_), tr("Recovery"));
  tabs_->addTab(new WhatIfPage(&session_), tr("What if"));
  splitter_->addWidget(tabs_);
  splitter_->setStretchFactor(0, 0);
  splitter_->setStretchFactor(1, 1);
  splitter_->setSizes({340, 900});
  root->addWidget(splitter_, 1);
  setCentralWidget(central);

  connect(open, &QPushButton::clicked, this, &MainWindow::chooseSave);
  connect(themeButton_, &QToolButton::clicked, this, &MainWindow::toggleTheme);
  connect(setupButton_, &QToolButton::clicked, this, [this] {
    bool show = !sidebar_->isVisible();
    if (!narrow_) sidebarWanted_ = show;
    sidebar_->setVisible(show);
  });
  connect(&session_, &Session::reloaded, this, &MainWindow::refreshBar);
  refreshBar();
  resize(1320, 880);
}

void MainWindow::refreshBar()
{
  saveLabel_->setText(session_.summary());
  QStringList w = session_.warnings();
  warning_->setText(w.isEmpty() ? QString() : tr("⚠ %1").arg(w.first()));
  warning_->setToolTip(w.join(QStringLiteral("\n")));
  themeButton_->setText(theme().dark ? tr("☀  Light") : tr("☾  Dark"));
}

void MainWindow::chooseSave()
{
  QSettings settings;
  QString dir = settings.value(QStringLiteral("lastSaveDir")).toString();
  QString path = QFileDialog::getOpenFileName(
    this, tr("Open a savegame"), dir,
    tr("Savegames (*.dat *.DAT *.bak *.BAK *.adf *.ADF);;All files (*)"));
  if (!path.isEmpty()) openSave(path);
}

void MainWindow::openSave(const QString &path)
{
  QString problem = session_.openSave(path);
  if (!problem.isEmpty()) {
    QMessageBox::warning(this, tr("That file could not be read"),
                         tr("%1\n\n%2").arg(QFileInfo(path).fileName(), problem));
    return;
  }
  QSettings().setValue(QStringLiteral("lastSaveDir"), QFileInfo(path).absolutePath());
}

void MainWindow::showTab(int index) { tabs_->setCurrentIndex(index); }

void MainWindow::toggleTheme()
{
  bool dark = !theme().dark;
  applyTheme(*qApp, dark);
  QSettings().setValue(QStringLiteral("dark"), dark);
  refreshBar();
  for (QWidget *w : findChildren<QWidget *>()) w->update();
  session_.touch();   // colours baked into table cells and charts
}

void MainWindow::fitSidebar()
{
  bool narrow = width() < kNarrow;
  if (narrow == narrow_) return;
  narrow_ = narrow;
  sidebar_->setVisible(narrow ? false : sidebarWanted_);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  fitSidebar();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
  for (const QUrl &url : event->mimeData()->urls())
    if (url.isLocalFile()) {
      openSave(url.toLocalFile());
      break;
    }
}

}  // namespace ui
