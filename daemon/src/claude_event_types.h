#pragma once

#include <claude_types.h>

/// Event types received from hook scripts.
enum class Claude_event {
  SESSION_START,
  PROMPT_SUBMIT,
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
