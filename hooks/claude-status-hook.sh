#!/usr/bin/env bash
# Claude Code hook script for workspace status monitoring.
# Reads JSON from stdin, determines workspace from cwd via D-Bus, sends status via D-Bus.
# Configured as async hook in ~/.claude/settings.json for all relevant events.

set -euo pipefail

DBUS_SERVICE="org.workspace.Manager"
DBUS_PATH="/Manager"
DBUS_IFACE="org.workspace.Manager"

# Read JSON from stdin and extract all needed fields in a single jq call.
IFS=$'\t' read -r hook_event_name cwd tool_name notification_type notification_message session_id < <(
  jq -r '[
    (.hook_event_name // ""),
    (.cwd // ""),
    (.tool_name // ""),
    (.notification.type // ""),
    (.notification.message // ""),
    (.session_id // "")
  ] | @tsv' 2>/dev/null || printf '\t\t\t\t\t\n'
)

if [[ -z "$hook_event_name" || -z "$cwd" ]]; then
  exit 0
fi

# Determine workspace name by matching cwd against workspace project_dir entries via D-Bus
workspace=$(qdbus "$DBUS_SERVICE" "$DBUS_PATH" "${DBUS_IFACE}.FindWorkspaceByPath" "$cwd" 2>/dev/null) || exit 0

if [[ -z "$workspace" ]]; then
  exit 0
fi

# Build the event type and args based on hook event, then send via D-Bus.
event_type=""
args_tsv=""
case "$hook_event_name" in
  PreToolUse)
    event_type="working"
    args_tsv="${tool_name:-unknown}"
    ;;
  PostToolUse)
    event_type="post_tool"
    ;;
  Stop)
    event_type="stop"
    ;;
  Notification)
    case "$notification_type" in
      permission_prompt|elicitation_dialog)
        event_type="notification"
        args_tsv=$(printf '%s\t%s' "$notification_type" "$notification_message")
        ;;
      idle_prompt)
        event_type="notification"
        args_tsv="${notification_type}"
        ;;
      *)
        exit 0
        ;;
    esac
    ;;
  SessionStart)
    event_type="session_start"
    args_tsv="${session_id:-unknown}"
    ;;
  SessionEnd)
    event_type="session_end"
    ;;
  *)
    exit 0
    ;;
esac

if [[ -z "$event_type" ]]; then
  exit 0
fi

qdbus "$DBUS_SERVICE" "$DBUS_PATH" "${DBUS_IFACE}.ReportClaudeEvent" \
  "$workspace" "$event_type" "$args_tsv" 2>/dev/null || true
