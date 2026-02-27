#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>

class Claude_status_tracker;
class Workspace_db;

/// D-Bus adaptor exposing workspace management on org.workspace.Manager /Manager.
/// Replaces file-based state storage — clients use D-Bus instead of reading
/// ~/.config/workspaces/ files directly.
class Workspace_manager_dbus : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.workspace.Manager")

 public:
  /// @param db database reference
  /// @param claude_tracker Claude status tracker (for ReportClaudeEvent)
  /// @param parent QObject that owns this adaptor (used for D-Bus object registration)
  Workspace_manager_dbus(
    Workspace_db& db,
    Claude_status_tracker& claude_tracker,
    QObject* parent
  );

 public slots:
  void CreateWorkspace(const QString& name, const QString& project_dir);
  QString GetProjectDir(const QString& workspace_name);
  QString FindWorkspaceByPath(const QString& path);
  QString ListWorkspaces();
  void SetTabs(const QString& workspace_name, const QString& urls);
  QString GetTabs(const QString& workspace_name);
  void ReportClaudeEvent(const QString& workspace, const QString& event_type, const QString& args_tsv);

 private:
  Workspace_db& _db;
  Claude_status_tracker& _claude_tracker;
};
