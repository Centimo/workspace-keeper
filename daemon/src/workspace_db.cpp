#include "workspace_db.h"
#include "journal_log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonObject>
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
  auto dir_path = QFileInfo(db_path).absolutePath();
  if (!QDir().mkpath(dir_path)) {
    qCCritical(logServer, "failed to create database directory '%s'", qPrintable(dir_path));
  }

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
    "CREATE TABLE IF NOT EXISTS workspace_tab ("
    "  workspace_name TEXT NOT NULL REFERENCES workspace(name) ON DELETE CASCADE,"
    "  position INTEGER NOT NULL,"
    "  url TEXT NOT NULL,"
    "  PRIMARY KEY (workspace_name, position)"
    ")"
  )) {
    qCCritical(logServer, "failed to create workspace_tab table: %s",
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

  if (!query.exec(
    "CREATE TABLE IF NOT EXISTS meta ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT"
    ")"
  )) {
    qCCritical(logServer, "failed to create meta table: %s",
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

void Workspace_db::create_workspace(const QString& name, const QString& project_dir) {
  QSqlQuery query(_db);
  query.prepare(
    "INSERT INTO workspace (name, project_dir)"
    " VALUES (?, ?)"
    " ON CONFLICT(name) DO UPDATE"
    " SET project_dir = excluded.project_dir"
  );
  query.addBindValue(name);
  query.addBindValue(project_dir);

  if (!query.exec()) {
    qCWarning(logServer, "create_workspace: failed for '%s': %s",
      qPrintable(name), qPrintable(query.lastError().text()));
  }
}

QString Workspace_db::get_project_dir(const QString& workspace_name) const {
  QSqlQuery query(_db);
  query.prepare("SELECT project_dir FROM workspace WHERE name = ?");
  query.addBindValue(workspace_name);

  if (query.exec() && query.next()) {
    return query.value(0).toString();
  }
  return {};
}

QString Workspace_db::find_workspace_by_path(const QString& path) const {
  QSqlQuery query(_db);
  // Find all workspaces where project_dir is a prefix of the given path.
  // The longest match wins (most specific workspace).
  // Uses SUBSTR instead of LIKE to avoid wildcards in project_dir (e.g. _ in paths).
  query.prepare(
    "SELECT name FROM workspace"
    " WHERE project_dir IS NOT NULL"
    "   AND project_dir != ''"
    "   AND (? = project_dir"
    "     OR SUBSTR(?, 1, LENGTH(project_dir) + 1) = project_dir || '/')"
    " ORDER BY LENGTH(project_dir) DESC"
    " LIMIT 1"
  );
  query.addBindValue(path);
  query.addBindValue(path);

  if (query.exec() && query.next()) {
    return query.value(0).toString();
  }
  return {};
}

QJsonArray Workspace_db::all_workspaces() const {
  QJsonArray result;
  QSqlQuery query(_db);

  if (query.exec(
    "SELECT w.name, w.project_dir, w.is_active,"
    "  (SELECT COUNT(*) FROM workspace_tab t WHERE t.workspace_name = w.name) AS tab_count"
    " FROM workspace w"
    " ORDER BY w.is_active DESC, w.desktop_index, w.name"
  )) {
    while (query.next()) {
      QJsonObject obj;
      obj["name"] = query.value(0).toString();
      obj["project_dir"] = query.value(1).toString();
      obj["is_active"] = query.value(2).toBool();
      obj["tab_count"] = query.value(3).toInt();
      result.append(obj);
    }
  }
  return result;
}

void Workspace_db::sync_active_desktops(const QVector< Desktop_info>& desktops) {
  _db.transaction();

  QSqlQuery reset(_db);
  if (!reset.exec("UPDATE workspace SET is_active = 0, desktop_index = NULL")) {
    qCWarning(logServer, "sync_active_desktops: reset failed: %s",
      qPrintable(reset.lastError().text()));
    _db.rollback();
    return;
  }

  for (const auto& desktop : desktops) {
    QSqlQuery query(_db);
    query.prepare(
      "INSERT INTO workspace (name, is_active, desktop_index)"
      " VALUES (?, 1, ?)"
      " ON CONFLICT(name) DO UPDATE"
      " SET is_active = 1,"
      "     desktop_index = excluded.desktop_index"
    );
    query.addBindValue(desktop.name);
    query.addBindValue(desktop.index);

    if (!query.exec()) {
      qCWarning(logServer, "sync_active_desktops: failed for '%s': %s",
        qPrintable(desktop.name), qPrintable(query.lastError().text()));
      _db.rollback();
      return;
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

// --- Tabs ---

void Workspace_db::set_tabs(const QString& workspace_name, const QStringList& urls) {
  ensure_workspace_exists(workspace_name);

  _db.transaction();

  QSqlQuery del(_db);
  del.prepare("DELETE FROM workspace_tab WHERE workspace_name = ?");
  del.addBindValue(workspace_name);
  if (!del.exec()) {
    qCWarning(logServer, "set_tabs: failed to delete tabs for '%s': %s",
      qPrintable(workspace_name), qPrintable(del.lastError().text()));
    _db.rollback();
    return;
  }

  QSqlQuery insert(_db);
  insert.prepare(
    "INSERT INTO workspace_tab (workspace_name, position, url)"
    " VALUES (?, ?, ?)"
  );

  bool ok = true;
  for (int i = 0; i < urls.size(); ++i) {
    insert.addBindValue(workspace_name);
    insert.addBindValue(i);
    insert.addBindValue(urls[i]);
    if (!insert.exec()) {
      qCWarning(logServer, "set_tabs: failed to insert tab %d for '%s': %s",
        i, qPrintable(workspace_name), qPrintable(insert.lastError().text()));
      ok = false;
      break;
    }
  }

  if (!ok) {
    _db.rollback();
    return;
  }

  _db.commit();
}

QStringList Workspace_db::get_tabs(const QString& workspace_name) const {
  QStringList result;
  QSqlQuery query(_db);
  query.prepare(
    "SELECT url FROM workspace_tab"
    " WHERE workspace_name = ?"
    " ORDER BY position"
  );
  query.addBindValue(workspace_name);

  if (query.exec()) {
    while (query.next()) {
      result.append(query.value(0).toString());
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
    return -1;
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
    return -1;
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
    return -1;
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
      status.session_id     = query.value(1).toString();
      status.state          = state_from_string(query.value(2).toString());
      status.tool_name      = query.value(3).toString();
      status.wait_reason    = query.value(4).toString();
      status.wait_message   = query.value(5).toString();
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
    status.session_id     = query.value(1).toString();
    status.state          = state_from_string(query.value(2).toString());
    status.tool_name      = query.value(3).toString();
    status.wait_reason    = query.value(4).toString();
    status.wait_message   = query.value(5).toString();
    status.state_since_ms = query.value(6).toLongLong();
    return status;
  }
  return std::nullopt;
}

// --- Migration ---

void Workspace_db::migrate_from_config_dir(const QString& config_dir) {
  QDir dir(config_dir);
  if (!dir.exists()) {
    return;
  }

  QSqlQuery check(_db);
  check.prepare("SELECT value FROM meta WHERE key = 'migration_completed'");
  if (check.exec() && check.next()) {
    return;
  }

  qCInfo(logServer, "migrating workspaces from '%s'", qPrintable(config_dir));

  auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const auto& entry : entries) {
    auto name = entry.fileName();
    auto project_dir_path = entry.filePath() + "/project_dir";

    QFile pd_file(project_dir_path);
    if (!pd_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    auto project_dir = QTextStream(&pd_file).readLine().trimmed();
    if (project_dir.isEmpty()) {
      continue;
    }

    create_workspace(name, project_dir);

    // Migrate tabs
    auto tabs_path = entry.filePath() + "/tabs.txt";
    QFile tabs_file(tabs_path);
    if (tabs_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QStringList urls;
      QTextStream stream(&tabs_file);
      while (!stream.atEnd()) {
        auto line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
          urls.append(line);
        }
      }
      if (!urls.isEmpty()) {
        set_tabs(name, urls);
      }
    }

    qCInfo(logServer, "migrated workspace '%s' (project_dir='%s')",
      qPrintable(name), qPrintable(project_dir));
  }

  QSqlQuery mark(_db);
  mark.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES ('migration_completed', '1')");
  if (!mark.exec()) {
    qCWarning(logServer, "failed to mark migration as completed: %s",
      qPrintable(mark.lastError().text()));
  }
}
