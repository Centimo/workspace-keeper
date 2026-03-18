#include "workspace_manager_dbus.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

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

void Workspace_manager_dbus::DeleteWorkspace(const QString& name) {
  _db.delete_workspace(name);
  qCInfo(logServer, "DeleteWorkspace: removed '%s'", qPrintable(name));
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

// Find X11 desktop index for a wezterm-gui process by its PID.
// Uses wmctrl -lxp to list all windows with their PIDs and desktop indices,
// then reads _NET_WM_DESKTOP via xprop for the matching window.
static int find_desktop_for_pid(int pid) {
  QProcess wmctrl;
  wmctrl.start("wmctrl", {"-lxp"});
  if (!wmctrl.waitForFinished(3000))
    return -1;

  static const QRegularExpression whitespace("\\s+");
  for (const auto& line : wmctrl.readAllStandardOutput().split('\n')) {
    if (line.isEmpty())
      continue;
    // Format: 0xWID  desktop  PID  class  host  title...
    auto parts = QString::fromUtf8(line).split(whitespace, Qt::SkipEmptyParts);
    // parts: [0]=wid [1]=desktop [2]=pid [3]=class [4]=host [5..]=title
    if (parts.size() < 5)
      continue;
    if (!parts[3].contains("wezterm", Qt::CaseInsensitive))
      continue;
    bool ok = false;
    int window_pid = parts[2].toInt(&ok);
    if (!ok || window_pid != pid)
      continue;

    bool ok2 = false;
    int desktop = parts[1].toInt(&ok2);
    if (!ok2 || desktop < 0)
      continue;
    return desktop;
  }
  return -1;
}

void Workspace_manager_dbus::ReportWeztermTabs(
  const QString& gui_pid,
  const QString& wezterm_window_id,
  const QString& tabs_json
) {
  auto doc = QJsonDocument::fromJson(tabs_json.toUtf8());
  if (!doc.isArray()) {
    qCWarning(logServer, "ReportWeztermTabs: invalid JSON from pid=%s window=%s",
      qPrintable(gui_pid), qPrintable(wezterm_window_id));
    return;
  }

  bool ok = false;
  int pid = gui_pid.toInt(&ok);
  if (!ok || pid <= 0) {
    qCWarning(logServer, "ReportWeztermTabs: invalid pid '%s'", qPrintable(gui_pid));
    return;
  }

  int desktop_index = find_desktop_for_pid(pid);
  if (desktop_index < 0) {
    qCWarning(logServer, "ReportWeztermTabs: could not find desktop for pid=%d", pid);
    return;
  }

  auto workspace_name = _db.workspace_name_by_desktop_index(desktop_index);
  if (workspace_name.isEmpty()) {
    qCWarning(logServer, "ReportWeztermTabs: no workspace for desktop_index=%d (pid=%d)",
      desktop_index, pid);
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
  qCInfo(logServer, "ReportWeztermTabs: saved %d tabs for workspace '%s' (pid=%d desktop=%d)",
    tabs.size(), qPrintable(workspace_name), pid, desktop_index);
}
