#pragma once

#include "claude_types.h"

#include <QJsonArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

struct Desktop_info {
  int index;
  QString name;
  bool is_current;
};

struct Workspace_info {
  QString name;
  QString project_dir;
};

/// Single point of access to the SQLite database.
/// All SQL is encapsulated here — the rest of the codebase uses only
/// the public methods of this class.
class Workspace_db {
 public:
  explicit Workspace_db(const QString& db_path);
  ~Workspace_db();

  Workspace_db(const Workspace_db&) = delete;
  Workspace_db& operator =(const Workspace_db&) = delete;

  bool is_open() const;

  // --- Workspaces ---

  /// Insert or update a workspace with its project directory.
  void create_workspace(const QString& name, const QString& project_dir);

  /// @return project directory for the workspace, empty if not found.
  QString get_project_dir(const QString& workspace_name) const;

  /// Find workspace whose project_dir is a prefix of @p path (longest match).
  /// @return workspace name, empty if no match.
  QString find_workspace_by_path(const QString& path) const;

  /// @return JSON array of all workspaces with name, project_dir, tab_count, is_active.
  QJsonArray all_workspaces() const;

  /// Update active desktop state from the window manager snapshot.
  /// Marks matching workspaces as active, clears active flag for the rest.
  void sync_active_desktops(const QVector< Desktop_info>& desktops);

  QVector< Workspace_info> active_desktops() const;
  QVector< Workspace_info> saved_workspaces() const;

  // --- Tabs ---

  void set_tabs(const QString& workspace_name, const QStringList& urls);
  QStringList get_tabs(const QString& workspace_name) const;

  // --- Claude status ---

  /// Set Claude state for a workspace. Returns the state_since_ms written to DB.
  qint64 set_claude_state(
    const QString& workspace,
    Claude_state state,
    const QString& tool_name = {},
    const QString& wait_reason = {},
    const QString& wait_message = {}
  );

  /// Start a new Claude session. Returns the state_since_ms written to DB.
  qint64 start_claude_session(const QString& workspace, const QString& session_id);

  /// End the Claude session. Returns the state_since_ms written to DB.
  qint64 end_claude_session(const QString& workspace);

  QVector< Claude_workspace_status> all_claude_statuses() const;
  std::optional< Claude_workspace_status> claude_status(const QString& workspace) const;

  // --- Migration ---

  /// One-time migration: import workspaces from config directory files into DB.
  /// Reads project_dir and tabs.txt from each subdirectory.
  void migrate_from_config_dir(const QString& config_dir);

 private:
  void create_tables();
  void ensure_workspace_exists(const QString& name);

  static constexpr const char* _connection_name = "workspace_db";

  QSqlDatabase _db;
};
