#pragma once

#include <QString>
#include <QVariantMap>

/// Execution state of a Claude Code session within a workspace.
enum class Claude_state {
  NOT_RUNNING,  ///< No active Claude session
  IDLE,         ///< Session active but not executing tools
  REQUESTING,   ///< User prompt submitted, waiting for first tool call
  WORKING,      ///< Executing a tool call
  WAITING       ///< Blocked on user input (permission prompt or elicitation dialog)
};

/// Background color hex string for the given state (e.g. "#1d5fa0").
const char* state_color_hex(Claude_state state);

/// Text color hex string for the given state.
const char* state_text_color_hex(Claude_state state);

/// Single-character label for the given state (e.g. "R", "W").
const char* state_label(Claude_state state);

/// Per-workspace snapshot of Claude Code status.
struct Claude_workspace_status {
  QString workspace_name;
  Claude_state state = Claude_state::NOT_RUNNING;
  QString tool_name;     ///< Current tool (only meaningful in WORKING state)
  QString wait_reason;   ///< Why Claude is waiting (only meaningful in WAITING state)
  QString wait_message;  ///< User-facing wait message (only meaningful in WAITING state)
  qint64 state_since_ms = 0;  ///< Epoch millis when current state began
  QString session_id;

  /// Convert to QVariantMap for QML property access.
  /// State is converted to wire-format string ("idle", "working", etc.).
  QVariantMap to_variant_map() const;
};
