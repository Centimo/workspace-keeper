#include "claude_types.h"
#include "enum_strings.h"

const char* state_color_hex(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return "#1d5fa0";
    case Claude_state::WORKING:     return "#2d8a4e";
    case Claude_state::WAITING:     return "#b08020";
    case Claude_state::IDLE:        return "#555555";
    case Claude_state::NOT_RUNNING: return "#3a3a3a";
  }
  return "#3a3a3a";
}

const char* state_text_color_hex(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return "#ffffff";
    case Claude_state::WORKING:     return "#ffffff";
    case Claude_state::WAITING:     return "#ffffff";
    case Claude_state::IDLE:        return "#aaaaaa";
    case Claude_state::NOT_RUNNING: return "#666666";
  }
  return "#666666";
}

const char* state_label(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return "R";
    case Claude_state::WORKING:     return "W";
    case Claude_state::WAITING:     return "?";
    case Claude_state::IDLE:        return "_";
    case Claude_state::NOT_RUNNING: return "-";
  }
  return "-";
}

QVariantMap Claude_workspace_status::to_variant_map() const {
  return {
    {"state", to_wire_string(state)},
    {"tool_name", tool_name},
    {"wait_reason", wait_reason},
    {"wait_message", wait_message},
    {"state_since_ms", state_since_ms},
    {"color", state_color_hex(state)},
    {"text_color", state_text_color_hex(state)},
    {"label", state_label(state)}
  };
}
