#pragma once
#include <QMainWindow>
#include "Session.h"

class QLabel;
class QSplitter;
class QTabWidget;
class QToolButton;

namespace ui {

class Sidebar;
class HeroAttackPage;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = nullptr);
  void openSave(const QString &path);
  void showTab(int index);
  void showSetup(bool show);   // the side panel with the hero, monster and rules
  // All the window shows and was given, for a new window to start as this one left off
  QByteArray saveState() const;
  void restoreState(const QByteArray &state);
  Session *session() { return &session_; }

 protected:
  void resizeEvent(QResizeEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

 private:
  void chooseSave();
  void toggleTheme();
  void setZoom(int percent);   // saves it and starts the window again at that size
  void refreshBar();
  void fitSidebar();

  Session session_;
  Sidebar *sidebar_;
  QSplitter *splitter_;
  QTabWidget *tabs_;
  HeroAttackPage *heroPage_;
  QLabel *saveLabel_, *warning_;
  QToolButton *setupButton_, *themeButton_, *zoomButton_;
  bool sidebarWanted_ = true;   // the user's choice once the window is wide enough
  bool narrow_ = false;
};

}  // namespace ui
