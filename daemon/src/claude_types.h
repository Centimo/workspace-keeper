#pragma once

#include <QString>

/// Execution state of a Claude Code session within a workspace.
enum class Claude_state {
  NOT_RUNNING,  ///< No active Claude session
  IDLE,         ///< Session active but not executing tools
  WORKING,      ///< Executing a tool call
  WAITING       ///< Blocked on user input (permission prompt or elicitation dialog)
};

/// Event types received from hook scripts.
enum class Claude_event {
  SESSION_START,
  WORKING,
  POST_TOOL,
  STOP,
  NOTIFICATION,
  SESSION_END
};

/// Notification subtypes from Claude Code.
enum class Claude_notification {
  PERMISSION_PROMPT,
  ELICITATION_DIALOG,
  IDLE_PROMPT
};

/// Per-workspace snapshot of Claude Code status.
struct Claude_workspace_status {
  QString workspace_name;
  Claude_state state = Claude_state::NOT_RUNNING;
  QString tool_name;     ///< Current tool (only meaningful in WORKING state)
  QString wait_reason;   ///< Why Claude is waiting (only meaningful in WAITING state)
  QString wait_message;  ///< User-facing wait message (only meaningful in WAITING state)
  qint64 state_since_ms = 0;  ///< Epoch millis when current state began
  QString session_id;
};
