#pragma once

#include <magic_enum.hpp>

#include <QString>

#include <optional>

/// Convert enum value to wire-format string (SCREAMING_SNAKE -> snake_case).
/// Example: Claude_state::NOT_RUNNING -> "not_running"
template< typename E>
QString to_wire_string(E value) {
  auto name = magic_enum::enum_name(value);
  QString result;
  result.reserve(static_cast< qsizetype>(name.size()));
  for (char c : name) {
    result += QChar(std::tolower(static_cast< unsigned char>(c)));
  }
  return result;
}

/// Parse wire-format string to enum (snake_case -> SCREAMING_SNAKE).
/// Example: "not_running" -> Claude_state::NOT_RUNNING
template< typename E>
std::optional< E> from_wire_string(const QString& s) {
  auto upper = s.toUpper().toStdString();
  return magic_enum::enum_cast< E>(upper);
}
