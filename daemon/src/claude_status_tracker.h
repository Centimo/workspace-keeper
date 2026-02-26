#pragma once

#include "claude_types.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <chrono>

class Workspace_db;

/// Tracks Claude Code status per workspace via a simple state machine.
/// Events arrive from hook scripts through the status socket server.
/// Persists state to the database via Workspace_db.
class Claude_status_tracker : public QObject {
  Q_OBJECT

 public:
  explicit Claude_status_tracker(Workspace_db& db, QObject* parent = nullptr);

  void process_event(
    const QString& workspace,
    const QString& event_type,
    const QStringList& args
  );

  QVector< Claude_workspace_status> all_statuses() const;

 signals:
  void status_changed(
    const QString& workspace,
    Claude_state state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qint64 state_since_ms
  );

 private:
  void set_state(
    const QString& workspace,
    Claude_state state,
    const QString& tool_name = {},
    const QString& wait_reason = {},
    const QString& wait_message = {}
  );
  void check_timeouts();

  Workspace_db& _db;
  QTimer _timeout_timer;

  static constexpr auto _working_timeout = std::chrono::minutes(5);
};
