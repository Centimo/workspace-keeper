#include "claude_status_tracker.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDateTime>

#include <cinttypes>

Claude_status_tracker::Claude_status_tracker(Workspace_db& db, QObject* parent)
  : QObject(parent)
  , _db(db)
{
  _timeout_timer.setInterval(std::chrono::seconds(30));
  connect(&_timeout_timer, &QTimer::timeout, this, &Claude_status_tracker::check_timeouts);
  _timeout_timer.start();
}

void Claude_status_tracker::process_event(
  const QString& workspace,
  const QString& event_type,
  const QStringList& args
) {
  if (event_type == "session_start") {
    auto since = _db.start_claude_session(workspace, args.value(0));
    if (since >= 0) {
      emit status_changed(workspace, Claude_state::IDLE, {}, {}, {}, since);
    }
  }
  else if (event_type == "working") {
    auto tool_name = args.value(0);
    set_state(workspace, Claude_state::WORKING, tool_name);
  }
  else if (event_type == "post_tool") {
    auto current = _db.claude_status(workspace);
    if (current && current->state == Claude_state::WORKING) {
      // Between tool calls — still working, refresh timestamp and notify
      auto since = _db.set_claude_state(workspace, Claude_state::WORKING, current->tool_name);
      if (since >= 0) {
        emit status_changed(workspace, Claude_state::WORKING, current->tool_name, {}, {}, since);
      }
    }
    else {
      set_state(workspace, Claude_state::WORKING);
    }
  }
  else if (event_type == "stop") {
    set_state(workspace, Claude_state::IDLE);
  }
  else if (event_type == "notification") {
    const auto& type = args.value(0);
    if (type == "permission_prompt" || type == "elicitation_dialog") {
      set_state(workspace, Claude_state::WAITING, {}, type, args.value(1));
    }
    else if (type == "idle_prompt") {
      set_state(workspace, Claude_state::IDLE);
    }
  }
  else if (event_type == "session_end") {
    auto since = _db.end_claude_session(workspace);
    if (since >= 0) {
      emit status_changed(workspace, Claude_state::NOT_RUNNING, {}, {}, {}, since);
    }
  }
}

QVector< Claude_workspace_status> Claude_status_tracker::all_statuses() const {
  return _db.all_claude_statuses();
}

void Claude_status_tracker::set_state(
  const QString& workspace,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message
) {
  auto current = _db.claude_status(workspace);
  if (current && current->state == state
    && current->tool_name == tool_name
    && current->wait_reason == wait_reason
    && current->wait_message == wait_message)
  {
    return;
  }

  auto since = _db.set_claude_state(workspace, state, tool_name, wait_reason, wait_message);
  if (since < 0) {
    return;
  }
  emit status_changed(workspace, state, tool_name, wait_reason, wait_message, since);
}

void Claude_status_tracker::check_timeouts() {
  auto now = QDateTime::currentMSecsSinceEpoch();
  auto timeout_ms = std::chrono::duration_cast< std::chrono::milliseconds>(_working_timeout).count();

  auto statuses = _db.all_claude_statuses();
  for (const auto& status : statuses) {
    if (status.state == Claude_state::WORKING
      && (now - status.state_since_ms) > timeout_ms
    ) {
      qCWarning(logClaude, "workspace '%s' WORKING timeout (%" PRId64 " min), resetting to IDLE",
        qPrintable(status.workspace_name),
        static_cast< int64_t>(std::chrono::duration_cast< std::chrono::minutes>(_working_timeout).count()));
      set_state(status.workspace_name, Claude_state::IDLE);
    }
  }
}
