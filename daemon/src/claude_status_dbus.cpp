#include "claude_status_dbus.h"
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

  const auto& statuses = _tracker.statuses();
  for (auto it = statuses.constBegin(); it != statuses.constEnd(); ++it) {
    QJsonObject obj;
    obj["name"] = it.key();
    obj["state"] = state_to_string(it->state);
    obj["tool_name"] = it->tool_name;
    obj["wait_reason"] = it->wait_reason;
    obj["wait_message"] = it->wait_message;
    obj["state_since_ms"] = it->state_since_ms;
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
    workspace, state_to_string(state),
    tool_name, wait_reason, wait_message, state_since_ms
  );
}

QString Claude_status_dbus::state_to_string(Claude_state state) {
  switch (state) {
    case Claude_state::NOT_RUNNING: return "not_running";
    case Claude_state::IDLE:        return "idle";
    case Claude_state::WORKING:     return "working";
    case Claude_state::WAITING:     return "waiting";
  }
  return "unknown";
}
