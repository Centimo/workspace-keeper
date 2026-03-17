#pragma once

#include "claude_status_tracker.h"

#include <QDBusAbstractAdaptor>
#include <QString>
#include <QVariantMap>

/// D-Bus adaptor exposing Claude Code status on org.workspace.StatusMonitor /StatusMonitor.
/// Provides GetAllStatuses() method and StatusChanged() signal.
class Claude_status_dbus : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.workspace.StatusMonitor")

 public:
  explicit Claude_status_dbus(Claude_status_tracker& tracker);

 public slots:
  /// Returns JSON array of tab statuses:
  /// [{workspace_name, pane_id, state, tool_name, wait_reason, wait_message, state_since_ms}, ...]
  QString GetAllStatuses();

  void ReportClaudeEvent(
    const QString& workspace,
    const QString& event_type,
    const QString& args_tsv,
    int pane_id
  );

 signals:
  void StatusChanged(const QString& workspace_name, int pane_id, const QVariantMap& status);

 private:
  void on_status_changed(
    const QString& workspace,
    int pane_id,
    Claude_state state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qint64 state_since_ms
  );

  Claude_status_tracker& _tracker;
};
