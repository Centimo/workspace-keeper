#include "daemon_server.h"
#include "menu_window.h"

#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);

  Menu_window window;

  Daemon_server server(window);
  if (!server.start()) {
    return 1;
  }

  return app.exec();
}
