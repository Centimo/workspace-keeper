#include "claude_status_tracker.h"
#include "journal_log.h"

#include <QDateTime>

#include <cinttypes>

Claude_status_tracker::Claude_status_tracker(QObject* parent)
  : QObject(parent)
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
  auto& status = _statuses[workspace];

  if (event_type == "session_start") {
    status.session_id = args.value(0);
    set_state(workspace, Claude_state::IDLE);
  }
  else if (event_type == "working") {
    status.tool_name = args.value(0);
    set_state(workspace, Claude_state::WORKING);
  }
  else if (event_type == "post_tool") {
    if (status.state == Claude_state::WORKING) {
      // Between tool calls — still working, refresh timestamp
      status.state_since_ms = QDateTime::currentMSecsSinceEpoch();
    }
    else {
      // post_tool in non-WORKING state: tool_name is unknown at this point
      set_state(workspace, Claude_state::WORKING);
    }
  }
  else if (event_type == "stop") {
    set_state(workspace, Claude_state::IDLE);
  }
  else if (event_type == "notification") {
    const auto& type = args.value(0);
    if (type == "permission_prompt" || type == "elicitation_dialog") {
      status.wait_reason = type;
      status.wait_message = args.value(1);
      set_state(workspace, Claude_state::WAITING);
    }
    else if (type == "idle_prompt") {
      set_state(workspace, Claude_state::IDLE);
    }
  }
  else if (event_type == "session_end") {
    status.session_id.clear();
    set_state(workspace, Claude_state::NOT_RUNNING);
  }
}

const QHash< QString, Claude_workspace_status>& Claude_status_tracker::statuses() const {
  return _statuses;
}

void Claude_status_tracker::set_state(const QString& workspace, Claude_state state) {
  auto& status = _statuses[workspace];
  auto previous = status.state;
  if (state == previous) {
    return;
  }

  auto now = QDateTime::currentMSecsSinceEpoch();

  // Clear context fields that belong to the previous state BEFORE updating,
  // so the emitted signal carries only fields relevant to the new state
  if (state != Claude_state::WORKING) {
    status.tool_name.clear();
  }
  if (state != Claude_state::WAITING) {
    status.wait_reason.clear();
    status.wait_message.clear();
  }

  status.state = state;
  status.state_since_ms = now;

  emit status_changed(
    workspace, state,
    status.tool_name, status.wait_reason, status.wait_message,
    status.state_since_ms
  );
}

void Claude_status_tracker::check_timeouts() {
  auto now = QDateTime::currentMSecsSinceEpoch();
  auto timeout_ms = std::chrono::duration_cast< std::chrono::milliseconds>(_working_timeout).count();

  // Collect keys first to avoid mutation during iteration
  QStringList timed_out;
  for (auto it = _statuses.constBegin(); it != _statuses.constEnd(); ++it) {
    if (it->state == Claude_state::WORKING
      && (now - it->state_since_ms) > timeout_ms
    ) {
      timed_out.append(it.key());
    }
  }

  for (const auto& workspace : timed_out) {
    qCWarning(logClaude, "workspace '%s' WORKING timeout (%" PRId64 " min), resetting to IDLE",
      qPrintable(workspace),
      static_cast< int64_t>(std::chrono::duration_cast< std::chrono::minutes>(_working_timeout).count()));
    set_state(workspace, Claude_state::IDLE);
  }
}
