#!/bin/bash
# Generate bash constants from C++ enum definitions in claude_types.h.
# Parses enum class blocks and converts SCREAMING_SNAKE values to snake_case wire strings.
# Output: hooks/claude_constants.sh

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TYPES_FILES=(
  "$REPO_DIR/common/claude_types.h"
  "$REPO_DIR/daemon/src/claude_event_types.h"
)
OUTPUT_FILE="$REPO_DIR/hooks/claude_constants.sh"

for f in "${TYPES_FILES[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "Error: $f not found" >&2
    exit 1
  fi
done

{
  echo "# Auto-generated from common/claude_types.h and daemon/src/claude_event_types.h — do not edit"
  echo ""

  for TYPES_FILE in "${TYPES_FILES[@]}"; do
    current_enum=""
    while IFS= read -r line; do
      # Match "enum class Name {"
      if [[ "$line" =~ ^[[:space:]]*enum[[:space:]]+class[[:space:]]+([A-Za-z_][A-Za-z0-9_]*) ]]; then
        current_enum="${BASH_REMATCH[1]}"
        # Convert class name to SCREAMING prefix: Claude_state -> CLAUDE_STATE
        prefix=$(echo "$current_enum" | sed 's/\([a-z]\)\([A-Z]\)/\1_\2/g' | tr '[:lower:]' '[:upper:]')
        continue
      fi

      if [[ -n "$current_enum" ]]; then
        # Match enum value like "  VALUE_NAME,"  or "  VALUE_NAME  ///< comment"
        if [[ "$line" =~ ^[[:space:]]*([A-Z][A-Z0-9_]*) ]]; then
          value="${BASH_REMATCH[1]}"
          wire_string=$(echo "$value" | tr '[:upper:]' '[:lower:]')
          echo "${prefix}_${value}=\"${wire_string}\""
        fi

        # End of enum block
        if [[ "$line" =~ ^[[:space:]]*\} ]]; then
          current_enum=""
          echo ""
        fi
      fi
    done < "$TYPES_FILE"
  done
} > "$OUTPUT_FILE"

echo "Generated $OUTPUT_FILE"
