// Dungeon Master Calculator: what heroes, monsters and spells do to each
// other, and how a hero recovers, worked out from the game's own formulas.
//   dm_calculator [SAVEGAME]
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QTimer>

#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("CSBwin"));
  QCoreApplication::setApplicationName(QStringLiteral("Calculator"));
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("Dungeon Master / Chaos Strikes Back calculator"));
  parser.addHelpOption();
  parser.addPositionalArgument(QStringLiteral("savegame"), QStringLiteral("A savegame to open"));
  QCommandLineOption tab(QStringLiteral("tab"), QStringLiteral("Tab to show first (1-6)"), QStringLiteral("n"));
  QCommandLineOption dark(QStringLiteral("dark"), QStringLiteral("Dark colours"));
  QCommandLineOption light(QStringLiteral("light"), QStringLiteral("Light colours"));
  QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size"), QStringLiteral("WxH"));
  QCommandLineOption shot(QStringLiteral("shot"), QStringLiteral("Save a picture of the window and quit"),
                          QStringLiteral("file.png"));
  QCommandLineOption hero(QStringLiteral("hero"), QStringLiteral("Hero of the savegame to start with (1-4)"),
                          QStringLiteral("n"));
  parser.addOptions({tab, hero, dark, light, size, shot});
  parser.process(app);

  QSettings settings;
  bool useDark = settings.contains(QStringLiteral("dark")) ? settings.value(QStringLiteral("dark")).toBool()
                                                           : ui::systemPrefersDark();
  if (parser.isSet(dark)) useDark = true;
  if (parser.isSet(light)) useDark = false;
  ui::applyTheme(app, useDark);

  ui::MainWindow window;
  if (parser.isSet(size)) {
    const QStringList wh = parser.value(size).split(QLatin1Char('x'));
    if (wh.size() == 2) window.resize(wh[0].toInt(), wh[1].toInt());
  }
  if (!parser.positionalArguments().isEmpty()) window.openSave(parser.positionalArguments().first());
  if (parser.isSet(hero)) window.session()->selectHero(parser.value(hero).toInt() - 1);
  if (parser.isSet(tab)) window.showTab(parser.value(tab).toInt() - 1);
  window.show();
  if (parser.isSet(shot)) {
    const QString file = parser.value(shot);
    QTimer::singleShot(600, &window, [&window, file] {
      window.grab().save(file);
      qApp->quit();
    });
  }
  return app.exec();
}
