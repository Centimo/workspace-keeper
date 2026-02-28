#include "claude_status_dbus.h"
#include "enum_strings.h"
#include "journal_log.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

Claude_status_dbus::Claude_status_dbus(Claude_status_tracker& tracker)
  : QDBusAbstractAdaptor(&tracker)
  , _tracker(tracker)
{
  setAutoRelaySignals(false);

  connect(&_tracker, &Claude_status_tracker::status_changed,
    this, &Claude_status_dbus::on_status_changed);

  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    qCWarning(logClaude, "session bus not available, D-Bus interface disabled");
    return;
  }
  if (!bus.registerObject("/StatusMonitor", &tracker)) {
    qCWarning(logClaude, "failed to register D-Bus object: %s",
      qPrintable(bus.lastError().message()));
  }
  if (!bus.registerService("org.workspace.StatusMonitor")) {
    qCWarning(logClaude, "failed to register D-Bus service: %s",
      qPrintable(bus.lastError().message()));
  }
}

QString Claude_status_dbus::GetAllStatuses() {
  QJsonArray array;

  auto statuses = _tracker.all_statuses();
  for (const auto& status : statuses) {
    QJsonObject obj;
    obj["name"] = status.workspace_name;
    obj["state"] = to_wire_string(status.state);
    obj["tool_name"] = status.tool_name;
    obj["wait_reason"] = status.wait_reason;
    obj["wait_message"] = status.wait_message;
    obj["state_since_ms"] = status.state_since_ms;
    array.append(obj);
  }

  return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

void Claude_status_dbus::on_status_changed(
  const QString& workspace,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message,
  qint64 state_since_ms
) {
  emit StatusChanged(
    workspace, to_wire_string(state),
    tool_name, wait_reason, wait_message, state_since_ms
  );
}
