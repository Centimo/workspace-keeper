#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>

class Workspace_db;

/// D-Bus adaptor exposing workspace management on org.workspace.Manager /Manager.
/// Replaces file-based state storage — clients use D-Bus instead of reading
/// ~/.config/workspaces/ files directly.
class Workspace_manager_dbus : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.workspace.Manager")

 public:
  Workspace_manager_dbus(Workspace_db& db, QObject* parent);

 public slots:
  void CreateWorkspace(const QString& name, const QString& project_dir);
  QString GetProjectDir(const QString& workspace_name);
  QString FindWorkspaceByPath(const QString& path);
  QString ListWorkspaces();
  void SetTabs(const QString& workspace_name, const QString& urls);
  QString GetTabs(const QString& workspace_name);
  void ReportWeztermTabs(const QString& workspace_name, const QString& tabs_json);

 private:
  Workspace_db& _db;
};
