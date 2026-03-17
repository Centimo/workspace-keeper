#include "claude_status_tracker.h"
#include "claude_event_types.h"
#include "enum_strings.h"
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
  const QStringList& args,
  int pane_id
) {
  qCInfo(logClaude, "'%s' pane=%d event=%s", qPrintable(workspace), pane_id, qPrintable(event_type));

  auto event = from_wire_string< Claude_event>(event_type);
  if (!event) {
    qCWarning(logClaude, "unknown event type '%s' for workspace '%s'",
      qPrintable(event_type), qPrintable(workspace));
    return;
  }

  switch (*event) {
    case Claude_event::SESSION_START:  handle_session_start(workspace, pane_id, args);  break;
    case Claude_event::PROMPT_SUBMIT:  handle_prompt_submit(workspace, pane_id, args);  break;
    case Claude_event::WORKING:        handle_working(workspace, pane_id, args);        break;
    case Claude_event::POST_TOOL:      handle_post_tool(workspace, pane_id, args);      break;
    case Claude_event::STOP:           handle_stop(workspace, pane_id, args);           break;
    case Claude_event::NOTIFICATION:   handle_notification(workspace, pane_id, args);   break;
    case Claude_event::SESSION_END:    handle_session_end(workspace, pane_id, args);    break;
  }
}

void Claude_status_tracker::handle_session_start(
  const QString& workspace, int pane_id, const QStringList& args
) {
  auto since = _db.start_claude_tab_session(workspace, pane_id, args.value(0));
  if (since >= 0) {
    emit status_changed(workspace, pane_id, Claude_state::IDLE, {}, {}, {}, since);
  }
}

void Claude_status_tracker::handle_prompt_submit(
  const QString& workspace, int pane_id, const QStringList& /*args*/
) {
  set_state(workspace, pane_id, Claude_state::REQUESTING);
}

void Claude_status_tracker::handle_working(
  const QString& workspace, int pane_id, const QStringList& args
) {
  set_state(workspace, pane_id, Claude_state::WORKING, args.value(0));
}

void Claude_status_tracker::handle_post_tool(
  const QString& workspace, int pane_id, const QStringList& /*args*/
) {
  auto current = _db.claude_tab_status(workspace, pane_id);
  if (current && current->state == Claude_state::WORKING) {
    auto since = _db.set_claude_tab_state(workspace, pane_id, Claude_state::WORKING, current->tool_name);
    if (since >= 0) {
      emit status_changed(workspace, pane_id, Claude_state::WORKING, current->tool_name, {}, {}, since);
    }
  }
  else {
    set_state(workspace, pane_id, Claude_state::WORKING);
  }
}

void Claude_status_tracker::handle_stop(
  const QString& workspace, int pane_id, const QStringList& /*args*/
) {
  set_state(workspace, pane_id, Claude_state::IDLE);
}

void Claude_status_tracker::handle_notification(
  const QString& workspace, int pane_id, const QStringList& args
) {
  auto type = from_wire_string< Claude_notification>(args.value(0));
  if (!type) {
    qCWarning(logClaude, "unknown notification type '%s' for workspace '%s'",
      qPrintable(args.value(0)), qPrintable(workspace));
    return;
  }

  switch (*type) {
    case Claude_notification::PERMISSION_PROMPT:
    case Claude_notification::ELICITATION_DIALOG:
      set_state(workspace, pane_id, Claude_state::WAITING, {}, args.value(0), args.value(1));
      break;
    case Claude_notification::IDLE_PROMPT:
      set_state(workspace, pane_id, Claude_state::IDLE);
      break;
  }
}

void Claude_status_tracker::handle_session_end(
  const QString& workspace, int pane_id, const QStringList& /*args*/
) {
  auto since = _db.end_claude_tab_session(workspace, pane_id);
  if (since >= 0) {
    emit status_changed(workspace, pane_id, Claude_state::NOT_RUNNING, {}, {}, {}, since);
  }
}

QVector< Claude_tab_status> Claude_status_tracker::all_statuses() const {
  return _db.all_claude_tab_statuses();
}

void Claude_status_tracker::set_state(
  const QString& workspace,
  int pane_id,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message
) {
  auto current = _db.claude_tab_status(workspace, pane_id);
  if (current && current->state == state
    && current->tool_name == tool_name
    && current->wait_reason == wait_reason
    && current->wait_message == wait_message)
  {
    return;
  }

  auto since = _db.set_claude_tab_state(workspace, pane_id, state, tool_name, wait_reason, wait_message);
  if (since < 0) {
    return;
  }

  if (!tool_name.isEmpty()) {
    qCInfo(logClaude, "'%s' pane=%d -> %s [tool=%s]",
      qPrintable(workspace), pane_id, qPrintable(to_wire_string(state)), qPrintable(tool_name));
  }
  else {
    qCInfo(logClaude, "'%s' pane=%d -> %s", qPrintable(workspace), pane_id, qPrintable(to_wire_string(state)));
  }

  emit status_changed(workspace, pane_id, state, tool_name, wait_reason, wait_message, since);
}

void Claude_status_tracker::check_timeouts() {
  auto now = QDateTime::currentMSecsSinceEpoch();
  auto timeout_ms = std::chrono::duration_cast< std::chrono::milliseconds>(_working_timeout).count();

  auto statuses = _db.all_claude_tab_statuses();
  for (const auto& status : statuses) {
    if ((status.state == Claude_state::WORKING || status.state == Claude_state::REQUESTING)
      && (now - status.state_since_ms) > timeout_ms
    ) {
      qCWarning(logClaude, "workspace '%s' pane=%d %s timeout (%" PRId64 " min), resetting to IDLE",
        qPrintable(status.workspace_name),
        status.pane_id,
        qPrintable(to_wire_string(status.state)),
        static_cast< int64_t>(std::chrono::duration_cast< std::chrono::minutes>(_working_timeout).count()));
      set_state(status.workspace_name, status.pane_id, Claude_state::IDLE);
    }
  }
}
