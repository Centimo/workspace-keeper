#include "claude_types.h"
#include "enum_strings.h"

QVariantMap Claude_workspace_status::to_variant_map() const {
  return {
    {"state", to_wire_string(state)},
    {"tool_name", tool_name},
    {"wait_reason", wait_reason},
    {"wait_message", wait_message},
    {"state_since_ms", state_since_ms}
  };
}