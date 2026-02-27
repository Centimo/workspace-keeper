#include "workspace_manager_dbus.h"
#include "claude_status_tracker.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QJsonDocument>

Workspace_manager_dbus::Workspace_manager_dbus(
  Workspace_db& db,
  Claude_status_tracker& claude_tracker,
  QObject* parent
)
  : QDBusAbstractAdaptor(parent)
  , _db(db)
  , _claude_tracker(claude_tracker)
{
  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    qCWarning(logServer, "session bus not available, Manager D-Bus interface disabled");
    return;
  }
  if (!bus.registerObject("/Manager", parent)) {
    qCWarning(logServer, "failed to register /Manager D-Bus object: %s",
      qPrintable(bus.lastError().message()));
  }
  if (!bus.registerService("org.workspace.Manager")) {
    qCWarning(logServer, "failed to register org.workspace.Manager D-Bus service: %s",
      qPrintable(bus.lastError().message()));
  }
}

void Workspace_manager_dbus::CreateWorkspace(const QString& name, const QString& project_dir) {
  _db.create_workspace(name, project_dir);
}

QString Workspace_manager_dbus::GetProjectDir(const QString& workspace_name) {
  return _db.get_project_dir(workspace_name);
}

QString Workspace_manager_dbus::FindWorkspaceByPath(const QString& path) {
  return _db.find_workspace_by_path(path);
}

QString Workspace_manager_dbus::ListWorkspaces() {
  return QJsonDocument(_db.all_workspaces()).toJson(QJsonDocument::Compact);
}

void Workspace_manager_dbus::SetTabs(const QString& workspace_name, const QString& urls) {
  auto list = urls.split('\n', Qt::SkipEmptyParts);
  _db.set_tabs(workspace_name, list);
}

QString Workspace_manager_dbus::GetTabs(const QString& workspace_name) {
  return _db.get_tabs(workspace_name).join('\n');
}

void Workspace_manager_dbus::ReportClaudeEvent(
  const QString& workspace,
  const QString& event_type,
  const QString& args_tsv
) {
  auto args = args_tsv.split('\t');
  _claude_tracker.process_event(workspace, event_type, args);
}
