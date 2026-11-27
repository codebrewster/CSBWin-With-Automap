// Dungeon Master Calculator: what heroes, monsters and spells do to each
// other, and how a hero recovers, worked out from the game's own formulas.
//   dm_calculator [SAVEGAME]
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QTimer>
#include <QScreen>

#include <cstdlib>
#ifdef Q_OS_WASM
#include <emscripten/val.h>
#endif
#include <cstring>

#include "MainWindow.h"
#include "Theme.h"

// The zoom: Qt's own scale factor, which grows fonts, spacing, tables and the drawn charts
// alike.  It has to be set before the application starts, so a new zoom restarts the window
// (MainWindow::setZoom).  --zoom on the command line wins over the saved one; a QT_SCALE_FACTOR
// the user set in the environment is left alone.
static void applyZoom(int argc, char **argv)
{
#ifdef Q_OS_WASM
  (void)argc;
  (void)argv;
  return;   // the browser zooms the page itself
#endif
  if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) return;
  int percent = QSettings(QStringLiteral("CSBwin"), QStringLiteral("Calculator"))
                  .value(QStringLiteral("zoom"), 100).toInt();
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--zoom")) percent = std::atoi(argv[i + 1]);
  percent = qBound(50, percent, 400);
  if (percent != 100) qputenv("QT_SCALE_FACTOR", QByteArray::number(percent / 100.0));
}

int main(int argc, char **argv)
{
  applyZoom(argc, argv);
#ifdef Q_OS_WASM
  // Chrome's newer file dialog (showOpenFilePicker) refuses whole folders it counts as
  // "system files". Without it Qt takes the classic file input, which hands over any file.
  emscripten::val::global("window").set("showOpenFilePicker", emscripten::val::undefined());
#endif
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
  QCommandLineOption zoom(QStringLiteral("zoom"), QStringLiteral("Zoom in percent (50-400), for this start only"),
                          QStringLiteral("percent"));
  QCommandLineOption noSetup(QStringLiteral("no-setup"), QStringLiteral("Start with the side panel hidden"));
  QCommandLineOption state(QStringLiteral("state"), QStringLiteral("What the window had when the zoom changed"),
                           QStringLiteral("data"));
  parser.addOptions({tab, hero, dark, light, size, shot, zoom, noSetup, state});
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
  if (parser.isSet(noSetup)) window.showSetup(false);
  if (parser.isSet(state)) window.restoreState(QByteArray::fromBase64(parser.value(state).toLatin1()));
  // zoomed in, the window's usual size may no longer fit the screen
  if (QScreen *screen = window.screen()) {
    const QSize room = screen->availableGeometry().size();
    if (window.width() > room.width() || window.height() > room.height())
      window.resize(window.size().boundedTo(room * 0.95));
  }
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
