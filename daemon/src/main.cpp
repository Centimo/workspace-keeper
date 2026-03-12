#include "claude_status_dbus.h"
#include "claude_status_tracker.h"
#include "daemon_server.h"
#include "desktop_monitor.h"
#include "global_shortcut.h"
#include "journal_log.h"
#include "menu_window.h"
#include "status_overlay.h"
#include "tab_tracker.h"
#include "workspace_db.h"
#include "workspace_manager_dbus.h"

#include <QApplication>
#include <QDir>
#include <QSocketNotifier>
#include <QStandardPaths>

#include <csignal>
#include <unistd.h>

static int signal_pipe[2];

static void signal_handler(int) {
  char byte = 1;
  write(signal_pipe[1], &byte, 1);
}

int main(int argc, char* argv[]) {
  install_journal_handler();

  QApplication app(argc, argv);
  app.setApplicationName("workspace-menu");
  app.setQuitOnLastWindowClosed(false);

  qCInfo(logServer, "starting (pid=%d, built " __DATE__ " " __TIME__ ")", static_cast< int>(getpid()));

  pipe(signal_pipe);
  auto* signal_notifier = new QSocketNotifier(signal_pipe[0], QSocketNotifier::Read, &app);
  QObject::connect(signal_notifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);

  auto data_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
    + "/workspace-menu";
  Workspace_db db(data_dir + "/workspace.db");

  // One-time migration from file-based config
  auto config_dir = qEnvironmentVariable("WORKSPACE_DIR");
  if (config_dir.isEmpty()) {
    config_dir = QDir::homePath() + "/.config/workspaces";
  }
  db.migrate_from_config_dir(config_dir);

  Desktop_monitor desktop_monitor;
  Menu_window window(db, desktop_monitor);

  Claude_status_tracker claude_tracker(db);
  // Allocated on heap: QDBusAbstractAdaptor re-parents itself to its parent QObject,
  // so Qt object tree owns its lifetime. Stack allocation would cause double-delete.
  new Claude_status_dbus(claude_tracker);

  // Workspace manager D-Bus service (org.workspace.Manager /Manager).
  // Needs a stable QObject as parent for D-Bus object registration.
  QObject manager_host;
  new Workspace_manager_dbus(db, &manager_host);

  Status_overlay overlay(desktop_monitor, db);

  QObject::connect(&desktop_monitor, &Desktop_monitor::desktops_changed, &app, [&db, &desktop_monitor]() {
    QVector< Desktop_info> infos;
    int index = 0;
    const auto& current_name = desktop_monitor.current_desktop_name();
    for (const auto& d : desktop_monitor.desktops()) {
      infos.append({index++, d.name, d.name == current_name});
    }
    db.sync_active_desktops(infos);
  });

  QObject::connect(&claude_tracker, &Claude_status_tracker::status_changed,
    &overlay, &Status_overlay::on_status_changed);

  // Load initial claude statuses into overlay
  for (const auto& status : claude_tracker.all_statuses()) {
    overlay.on_status_changed(
      status.workspace_name, status.state,
      status.tool_name, status.wait_reason, status.wait_message,
      status.state_since_ms
    );
  }

  Tab_tracker tab_tracker(db);
  tab_tracker.start();

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

  constexpr int desktop_shortcut_count = 8;
  for (int i = 0; i < desktop_shortcut_count; ++i) {
    auto* sc = new Global_shortcut(
      QString("switch-desktop-%1").arg(i + 1),
      QString("Switch to Desktop %1").arg(i + 1),
      Qt::ALT | static_cast< Qt::Key>(Qt::Key_F1 + i),
      &app
    );

    QObject::connect(sc, &Global_shortcut::triggered, &app, [&db, &desktop_monitor, i]() {
      auto name = db.active_desktop_name_at(i);
      if (name.isEmpty()) {
        qCInfo(logShortcut, "Alt+F%d: no active desktop at position %d", i + 1, i);
        return;
      }

      qCInfo(logShortcut, "Alt+F%d: switching to '%s'", i + 1, qPrintable(name));
      desktop_monitor.switch_to_desktop_by_name(name);
    });
  }

  auto exit_code = app.exec();
  qCInfo(logServer, "shutting down (exit_code=%d)", exit_code);
  return exit_code;
}
