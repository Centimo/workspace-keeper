#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>

/// Execution state of a Claude Code session within a workspace.
enum class Claude_state {
  NOT_RUNNING,  ///< No active Claude session
  IDLE,         ///< Session active but not executing tools
  WORKING,      ///< Executing a tool call
  WAITING       ///< Blocked on user input (permission prompt or elicitation dialog)
};

/// Per-workspace snapshot of Claude Code status.
struct Claude_workspace_status {
  Claude_state state = Claude_state::NOT_RUNNING;
  QString tool_name;     ///< Current tool (only meaningful in WORKING state)
  QString wait_reason;   ///< Why Claude is waiting (only meaningful in WAITING state)
  QString wait_message;  ///< User-facing wait message (only meaningful in WAITING state)
  qint64 state_since_ms = 0;  ///< Epoch millis when current state began
  QString session_id;
};

/// Tracks Claude Code status per workspace via a simple state machine.
/// Events arrive from hook scripts through the status socket server.
class Claude_status_tracker : public QObject {
  Q_OBJECT

 public:
  explicit Claude_status_tracker(QObject* parent = nullptr);

  void process_event(
    const QString& workspace,
    const QString& event_type,
    const QStringList& args
  );

  const QHash< QString, Claude_workspace_status>& statuses() const;

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
  void set_state(const QString& workspace, Claude_state state);
  void check_timeouts();

  QHash< QString, Claude_workspace_status> _statuses;
  QTimer _timeout_timer;

  static constexpr auto _working_timeout = std::chrono::minutes(5);
};
