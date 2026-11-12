#pragma once
#include <QMainWindow>
#include "Session.h"

class QLabel;
class QSplitter;
class QTabWidget;
class QToolButton;

namespace ui {

class Sidebar;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = nullptr);
  void openSave(const QString &path);
  void showTab(int index);
  Session *session() { return &session_; }

 protected:
  void resizeEvent(QResizeEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

 private:
  void chooseSave();
  void toggleTheme();
  void refreshBar();
  void fitSidebar();

  Session session_;
  Sidebar *sidebar_;
  QSplitter *splitter_;
  QTabWidget *tabs_;
  QLabel *saveLabel_, *warning_;
  QToolButton *setupButton_, *themeButton_;
  bool sidebarWanted_ = true;   // the user's choice once the window is wide enough
  bool narrow_ = false;
};

}  // namespace ui
