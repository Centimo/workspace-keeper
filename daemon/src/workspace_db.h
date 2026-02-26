#pragma once

#include "claude_types.h"

#include <QSqlDatabase>
#include <QString>
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

  /// Update active desktop state from the window manager snapshot.
  /// Marks matching workspaces as active, clears active flag for the rest.
  /// Reads project_dir from ~/.config/workspaces/<name>/project_dir files.
  void sync_active_desktops(
    const QVector< Desktop_info>& desktops,
    const QString& workspace_config_dir
  );

  QVector< Workspace_info> active_desktops() const;
  QVector< Workspace_info> saved_workspaces() const;

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

 private:
  void create_tables();
  void ensure_workspace_exists(const QString& name);

  static constexpr const char* _connection_name = "workspace_db";

  QSqlDatabase _db;
};
