#include <QApplication>
#include "MainWindow.h"

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  MainWindow w;
  const QStringList args = app.arguments();
  int gi = args.indexOf("--grab");
  if (gi >= 0 && gi + 1 < args.size()) {
    int mi = args.indexOf("--mode"), ti = args.indexOf("--trials-m");
    w.autoRun(mi >= 0 ? args.value(mi + 1).toInt() : 2,
              ti >= 0 ? args.value(ti + 1).toDouble() : 5.0, args[gi + 1]);
    return app.exec();
  }
  w.show();
  return app.exec();
}
