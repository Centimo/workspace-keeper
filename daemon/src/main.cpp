#include "claude_status_dbus.h"
#include "claude_status_tracker.h"
#include "daemon_server.h"
#include "global_shortcut.h"
#include "journal_log.h"
#include "menu_window.h"
#include "workspace_db.h"
#include "workspace_manager_dbus.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
  install_journal_handler();

  QApplication app(argc, argv);
  app.setApplicationName("workspace-menu");
  app.setQuitOnLastWindowClosed(false);

  auto data_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
    + "/workspace-menu";
  Workspace_db db(data_dir + "/workspace.db");

  // One-time migration from file-based config
  auto config_dir = qEnvironmentVariable("WORKSPACE_DIR");
  if (config_dir.isEmpty()) {
    config_dir = QDir::homePath() + "/.config/workspaces";
  }
  db.migrate_from_config_dir(config_dir);

  Menu_window window(db);

  Claude_status_tracker claude_tracker(db);
  // Allocated on heap: QDBusAbstractAdaptor re-parents itself to its parent QObject,
  // so Qt object tree owns its lifetime. Stack allocation would cause double-delete.
  new Claude_status_dbus(claude_tracker);

  // Workspace manager D-Bus service (org.workspace.Manager /Manager).
  // Needs a stable QObject as parent for D-Bus object registration.
  QObject manager_host;
  new Workspace_manager_dbus(db, claude_tracker, &manager_host);

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
