#include "workspace_db.h"
#include "journal_log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

static Claude_state state_from_string(const QString& s) {
  if (s == "idle")        return Claude_state::IDLE;
  if (s == "working")     return Claude_state::WORKING;
  if (s == "waiting")     return Claude_state::WAITING;
  return Claude_state::NOT_RUNNING;
}

static QString state_to_string(Claude_state state) {
  switch (state) {
    case Claude_state::NOT_RUNNING: return "not_running";
    case Claude_state::IDLE:        return "idle";
    case Claude_state::WORKING:     return "working";
    case Claude_state::WAITING:     return "waiting";
  }
  return "not_running";
}

Workspace_db::Workspace_db(const QString& db_path) {
  QDir().mkpath(QFileInfo(db_path).absolutePath());

  _db = QSqlDatabase::addDatabase("QSQLITE", _connection_name);
  _db.setDatabaseName(db_path);

  if (!_db.open()) {
    qCCritical(logServer, "failed to open database '%s': %s",
      qPrintable(db_path), qPrintable(_db.lastError().text()));
    return;
  }

  QSqlQuery pragma(_db);
  if (!pragma.exec("PRAGMA journal_mode=WAL")) {
    qCWarning(logServer, "PRAGMA journal_mode=WAL failed: %s",
      qPrintable(pragma.lastError().text()));
  }
  if (!pragma.exec("PRAGMA foreign_keys=ON")) {
    qCWarning(logServer, "PRAGMA foreign_keys=ON failed: %s",
      qPrintable(pragma.lastError().text()));
  }

  create_tables();
}

Workspace_db::~Workspace_db() {
  // Must reset member before removeDatabase to avoid Qt warning
  // about connection still being in use.
  _db.close();
  _db = QSqlDatabase();
  QSqlDatabase::removeDatabase(_connection_name);
}

bool Workspace_db::is_open() const {
  return _db.isOpen();
}

void Workspace_db::create_tables() {
  QSqlQuery query(_db);

  if (!query.exec(
    "CREATE TABLE IF NOT EXISTS workspace ("
    "  name TEXT PRIMARY KEY,"
    "  project_dir TEXT,"
    "  is_active INTEGER NOT NULL DEFAULT 0,"
    "  desktop_index INTEGER"
    ")"
  )) {
    qCCritical(logServer, "failed to create workspace table: %s",
      qPrintable(query.lastError().text()));
  }

  if (!query.exec(
    "CREATE TABLE IF NOT EXISTS claude_session ("
    "  workspace_name TEXT PRIMARY KEY REFERENCES workspace(name),"
    "  session_id TEXT,"
    "  state TEXT NOT NULL DEFAULT 'not_running',"
    "  tool_name TEXT,"
    "  wait_reason TEXT,"
    "  wait_message TEXT,"
    "  state_since_ms INTEGER NOT NULL DEFAULT 0"
    ")"
  )) {
    qCCritical(logServer, "failed to create claude_session table: %s",
      qPrintable(query.lastError().text()));
  }
}

void Workspace_db::ensure_workspace_exists(const QString& name) {
  QSqlQuery query(_db);
  query.prepare("INSERT OR IGNORE INTO workspace (name) VALUES (?)");
  query.addBindValue(name);
  if (!query.exec()) {
    qCWarning(logServer, "ensure_workspace_exists failed for '%s': %s",
      qPrintable(name), qPrintable(query.lastError().text()));
  }
}

// --- Workspaces ---

static QString read_project_dir(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QTextStream(&file).readLine().trimmed();
}

void Workspace_db::sync_active_desktops(
  const QVector< Desktop_info>& desktops,
  const QString& workspace_config_dir
) {
  _db.transaction();

  // Reset all workspaces to inactive
  QSqlQuery reset(_db);
  reset.exec("UPDATE workspace SET is_active = 0, desktop_index = NULL");

  // Collect names of active desktops for the second loop
  QSet< QString> active_names;

  for (const auto& desktop : desktops) {
    active_names.insert(desktop.name);
    auto project_dir = read_project_dir(
      workspace_config_dir + '/' + desktop.name + "/project_dir"
    );

    QSqlQuery query(_db);
    query.prepare(
      "INSERT INTO workspace (name, project_dir, is_active, desktop_index)"
      " VALUES (?, ?, 1, ?)"
      " ON CONFLICT(name) DO UPDATE"
      " SET project_dir = excluded.project_dir,"
      "     is_active = 1,"
      "     desktop_index = excluded.desktop_index"
    );
    query.addBindValue(desktop.name);
    query.addBindValue(project_dir.isEmpty() ? QVariant() : project_dir);
    query.addBindValue(desktop.index);

    if (!query.exec()) {
      qCWarning(logServer, "sync_active_desktops: failed for '%s': %s",
        qPrintable(desktop.name), qPrintable(query.lastError().text()));
    }
  }

  // Ensure saved workspaces (dirs without active desktop) are also in the DB
  QDir ws_dir(workspace_config_dir);
  if (ws_dir.exists()) {
    auto entries = ws_dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& entry : entries) {
      auto name = entry.fileName();
      if (active_names.contains(name)) {
        continue;  // Already processed above
      }

      auto project_dir = read_project_dir(entry.filePath() + "/project_dir");

      QSqlQuery query(_db);
      query.prepare(
        "INSERT INTO workspace (name, project_dir)"
        " VALUES (?, ?)"
        " ON CONFLICT(name) DO UPDATE"
        " SET project_dir = excluded.project_dir"
      );
      query.addBindValue(name);
      query.addBindValue(project_dir.isEmpty() ? QVariant() : project_dir);
      query.exec();
    }
  }

  _db.commit();
}

QVector< Workspace_info> Workspace_db::active_desktops() const {
  QVector< Workspace_info> result;
  QSqlQuery query(_db);

  if (query.exec(
    "SELECT name, project_dir FROM workspace"
    " WHERE is_active = 1"
    " ORDER BY desktop_index"
  )) {
    while (query.next()) {
      result.append({
        query.value(0).toString(),
        query.value(1).toString()
      });
    }
  }
  return result;
}

QVector< Workspace_info> Workspace_db::saved_workspaces() const {
  QVector< Workspace_info> result;
  QSqlQuery query(_db);

  if (query.exec(
    "SELECT name, project_dir FROM workspace"
    " WHERE is_active = 0"
    " ORDER BY name"
  )) {
    while (query.next()) {
      result.append({
        query.value(0).toString(),
        query.value(1).toString()
      });
    }
  }
  return result;
}

// --- Claude status ---

qint64 Workspace_db::set_claude_state(
  const QString& workspace,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message
) {
  ensure_workspace_exists(workspace);

  auto now = QDateTime::currentMSecsSinceEpoch();

  QSqlQuery query(_db);
  query.prepare(
    "INSERT INTO claude_session (workspace_name, state, tool_name, wait_reason, wait_message, state_since_ms)"
    " VALUES (?, ?, ?, ?, ?, ?)"
    " ON CONFLICT(workspace_name) DO UPDATE SET"
    "   state = excluded.state,"
    "   tool_name = excluded.tool_name,"
    "   wait_reason = excluded.wait_reason,"
    "   wait_message = excluded.wait_message,"
    "   state_since_ms = excluded.state_since_ms"
  );
  query.addBindValue(workspace);
  query.addBindValue(state_to_string(state));
  query.addBindValue(tool_name.isEmpty() ? QVariant() : tool_name);
  query.addBindValue(wait_reason.isEmpty() ? QVariant() : wait_reason);
  query.addBindValue(wait_message.isEmpty() ? QVariant() : wait_message);
  query.addBindValue(now);

  if (!query.exec()) {
    qCWarning(logClaude, "set_claude_state: failed for '%s': %s",
      qPrintable(workspace), qPrintable(query.lastError().text()));
  }

  return now;
}

qint64 Workspace_db::start_claude_session(const QString& workspace, const QString& session_id) {
  ensure_workspace_exists(workspace);

  auto now = QDateTime::currentMSecsSinceEpoch();

  QSqlQuery query(_db);
  query.prepare(
    "INSERT INTO claude_session (workspace_name, session_id, state, state_since_ms)"
    " VALUES (?, ?, 'idle', ?)"
    " ON CONFLICT(workspace_name) DO UPDATE SET"
    "   session_id = excluded.session_id,"
    "   state = 'idle',"
    "   tool_name = NULL,"
    "   wait_reason = NULL,"
    "   wait_message = NULL,"
    "   state_since_ms = excluded.state_since_ms"
  );
  query.addBindValue(workspace);
  query.addBindValue(session_id);
  query.addBindValue(now);

  if (!query.exec()) {
    qCWarning(logClaude, "start_claude_session: failed for '%s': %s",
      qPrintable(workspace), qPrintable(query.lastError().text()));
  }

  return now;
}

qint64 Workspace_db::end_claude_session(const QString& workspace) {
  auto now = QDateTime::currentMSecsSinceEpoch();

  QSqlQuery query(_db);
  query.prepare(
    "UPDATE claude_session SET"
    "  session_id = NULL,"
    "  state = 'not_running',"
    "  tool_name = NULL,"
    "  wait_reason = NULL,"
    "  wait_message = NULL,"
    "  state_since_ms = ?"
    " WHERE workspace_name = ?"
  );
  query.addBindValue(now);
  query.addBindValue(workspace);

  if (!query.exec()) {
    qCWarning(logClaude, "end_claude_session: failed for '%s': %s",
      qPrintable(workspace), qPrintable(query.lastError().text()));
  }

  return now;
}

QVector< Claude_workspace_status> Workspace_db::all_claude_statuses() const {
  QVector< Claude_workspace_status> result;
  QSqlQuery query(_db);

  if (query.exec(
    "SELECT workspace_name, session_id, state, tool_name,"
    " wait_reason, wait_message, state_since_ms"
    " FROM claude_session"
    " WHERE state != 'not_running'"
  )) {
    while (query.next()) {
      Claude_workspace_status status;
      status.workspace_name = query.value(0).toString();
      status.session_id = query.value(1).toString();
      status.state = state_from_string(query.value(2).toString());
      status.tool_name = query.value(3).toString();
      status.wait_reason = query.value(4).toString();
      status.wait_message = query.value(5).toString();
      status.state_since_ms = query.value(6).toLongLong();
      result.append(status);
    }
  }
  return result;
}

std::optional< Claude_workspace_status> Workspace_db::claude_status(const QString& workspace) const {
  QSqlQuery query(_db);
  query.prepare(
    "SELECT workspace_name, session_id, state, tool_name,"
    " wait_reason, wait_message, state_since_ms"
    " FROM claude_session"
    " WHERE workspace_name = ?"
  );
  query.addBindValue(workspace);

  if (query.exec() && query.next()) {
    Claude_workspace_status status;
    status.workspace_name = query.value(0).toString();
    status.session_id = query.value(1).toString();
    status.state = state_from_string(query.value(2).toString());
    status.tool_name = query.value(3).toString();
    status.wait_reason = query.value(4).toString();
    status.wait_message = query.value(5).toString();
    status.state_since_ms = query.value(6).toLongLong();
    return status;
  }
  return std::nullopt;
}
