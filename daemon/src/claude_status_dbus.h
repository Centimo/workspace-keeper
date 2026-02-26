#pragma once

#include "claude_status_tracker.h"

#include <QDBusAbstractAdaptor>
#include <QString>

/// D-Bus adaptor exposing Claude Code status on org.workspace.StatusMonitor /StatusMonitor.
/// Provides GetAllStatuses() method and StatusChanged() signal.
class Claude_status_dbus : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.workspace.StatusMonitor")

 public:
  explicit Claude_status_dbus(Claude_status_tracker& tracker);

 public slots:
  /// Returns JSON array of workspace statuses:
  /// [{name, state, tool_name, wait_reason, wait_message, state_since_ms}, ...]
  QString GetAllStatuses();

 signals:
  void StatusChanged(
    const QString& workspace_name,
    const QString& state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qlonglong state_since_ms
  );

 private:
  void on_status_changed(
    const QString& workspace,
    Claude_state state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qint64 state_since_ms
  );

  static QString state_to_string(Claude_state state);

  Claude_status_tracker& _tracker;
};
