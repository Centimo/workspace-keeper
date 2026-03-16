#include "workspace_manager_dbus.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

Workspace_manager_dbus::Workspace_manager_dbus(Workspace_db& db, QObject* parent)
  : QDBusAbstractAdaptor(parent)
  , _db(db)
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
    return;
  }

  qCInfo(logServer, "D-Bus service org.workspace.Manager registered");
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

void Workspace_manager_dbus::ReportWeztermTabs(
  const QString& workspace_name,
  const QString& tabs_json
) {
  auto doc = QJsonDocument::fromJson(tabs_json.toUtf8());
  if (!doc.isArray()) {
    qCWarning(logServer, "ReportWeztermTabs: invalid JSON for workspace '%s'",
      qPrintable(workspace_name));
    return;
  }

  QVector< Wezterm_tab_info> tabs;
  for (const auto& val : doc.array()) {
    auto obj = val.toObject();
    tabs.append({
      obj["tab_index"].toInt(),
      obj["tab_id"].toInt(),
      obj["pane_id"].toInt(),
      obj["cwd"].toString(),
      obj["title"].toString()
    });
  }

  _db.set_wezterm_tabs(workspace_name, tabs);
  qCInfo(logServer, "ReportWeztermTabs: saved %d tabs for workspace '%s'",
    tabs.size(), qPrintable(workspace_name));
}
