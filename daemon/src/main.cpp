#include "daemon_server.h"
#include "global_shortcut.h"
#include "journal_log.h"
#include "menu_window.h"

#include <QApplication>

int main(int argc, char* argv[]) {
  install_journal_handler();

  QApplication app(argc, argv);
  app.setApplicationName("workspace-menu");
  app.setQuitOnLastWindowClosed(false);

  Menu_window window;

  Daemon_server server(window);
  if (!server.start()) {
    return 1;
  }

  Global_shortcut shortcut(
    "show-workspace-menu",
    "Show Workspace Menu",
    Qt::ALT | Qt::Key_Tab
  );
  QObject::connect(&shortcut, &Global_shortcut::triggered,
    &server, &Daemon_server::trigger_from_shortcut);

  return app.exec();
}
