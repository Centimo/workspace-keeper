#include "workspace_monitor.h"

#include <enum_strings.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

Workspace_monitor::Workspace_monitor(QObject* parent)
  : QObject(parent)
  , _daemon_watcher(
      "org.workspace.StatusMonitor",
      QDBusConnection::sessionBus(),
      QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration
    )
{
  auto bus = QDBusConnection::sessionBus();

  // --- KWin VirtualDesktopManager signals ---
  // All three use QDBusMessage slots to avoid signature matching issues
  // with KWin's custom struct types.
  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopCreated",
    this, SLOT(on_desktop_created(QDBusMessage))
  ))
    qWarning("Workspace_monitor: failed to connect desktopCreated signal");

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopRemoved",
    this, SLOT(on_desktop_removed(QDBusMessage))
  ))
    qWarning("Workspace_monitor: failed to connect desktopRemoved signal");

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopDataChanged",
    this, SLOT(on_desktop_data_changed(QDBusMessage))
  ))
    qWarning("Workspace_monitor: failed to connect desktopDataChanged signal");

  // --- Daemon StatusChanged signal ---
  if (!bus.connect(
    "org.workspace.StatusMonitor", "/StatusMonitor",
    "org.workspace.StatusMonitor", "StatusChanged",
    this, SLOT(on_status_changed(QString,QString,QString,QString,QString,qlonglong))
  ))
    qWarning("Workspace_monitor: failed to connect StatusChanged signal");

  // --- Watch for daemon appearance/disappearance ---
  connect(&_daemon_watcher, &QDBusServiceWatcher::serviceRegistered,
    this, &Workspace_monitor::on_daemon_registered);
  connect(&_daemon_watcher, &QDBusServiceWatcher::serviceUnregistered,
    this, &Workspace_monitor::on_daemon_unregistered);

  // --- Fetch initial state (async) ---
  fetch_desktops();
  fetch_all_statuses();
}

QString Workspace_monitor::stateColor(const QString& state) const {
  return state_color_hex(from_wire_string< Claude_state>(state).value_or(Claude_state::NOT_RUNNING));
}

QString Workspace_monitor::stateTextColor(const QString& state) const {
  return state_text_color_hex(from_wire_string< Claude_state>(state).value_or(Claude_state::NOT_RUNNING));
}

QString Workspace_monitor::stateLabel(const QString& state) const {
  return state_label(from_wire_string< Claude_state>(state).value_or(Claude_state::NOT_RUNNING));
}

void Workspace_monitor::switchToDesktop(int index) {
  if (index < 0 || index >= _desktops.size())
    return;

  const auto& id = _desktops[index].id;
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "setCurrentDesktop"
  );
  message << id;
  QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

// --- KWin signal handlers ---

void Workspace_monitor::on_desktop_created(const QDBusMessage& message) {
  Q_UNUSED(message);
  fetch_desktops();
}

void Workspace_monitor::on_desktop_removed(const QDBusMessage& message) {
  // Refetch all desktops to get correct positions after removal.
  Q_UNUSED(message);
  fetch_desktops();
}

void Workspace_monitor::on_desktop_data_changed(const QDBusMessage& message) {
  Q_UNUSED(message);
  fetch_desktops();
}

// --- Daemon signal handler ---

void Workspace_monitor::on_status_changed(
  const QString& workspace_name,
  const QString& state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message,
  qlonglong state_since_ms
) {
  _claude_statuses[workspace_name] = Claude_workspace_status{
    .workspace_name = workspace_name,
    .state = from_wire_string< Claude_state>(state).value_or(Claude_state::NOT_RUNNING),
    .tool_name = tool_name,
    .wait_reason = wait_reason,
    .wait_message = wait_message,
    .state_since_ms = state_since_ms
  };
  emit claudeStatusesChanged();
}

void Workspace_monitor::on_daemon_registered() {
  fetch_all_statuses();
}

void Workspace_monitor::on_daemon_unregistered() {
  _claude_statuses.clear();
  emit claudeStatusesChanged();
}

// --- Async data fetch ---

void Workspace_monitor::fetch_desktops() {
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.freedesktop.DBus.Properties", "Get"
  );
  message << "org.kde.KWin.VirtualDesktopManager" << "desktops";

  auto pending = QDBusConnection::sessionBus().asyncCall(message);
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished,
    this, &Workspace_monitor::on_desktops_fetched);
}

void Workspace_monitor::on_desktops_fetched(QDBusPendingCallWatcher* watcher) {
  watcher->deleteLater();
  QDBusPendingReply< QDBusVariant> reply = *watcher;
  if (reply.isError())
    return;

  auto argument = reply.value().variant().value< QDBusArgument>();
  _desktops = parse_desktops(argument);
  sort_by_position(_desktops);
  emit desktopsChanged();
}

void Workspace_monitor::fetch_all_statuses() {
  auto message = QDBusMessage::createMethodCall(
    "org.workspace.StatusMonitor", "/StatusMonitor",
    "org.workspace.StatusMonitor", "GetAllStatuses"
  );

  auto pending = QDBusConnection::sessionBus().asyncCall(message);
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished,
    this, &Workspace_monitor::on_statuses_fetched);
}

void Workspace_monitor::on_statuses_fetched(QDBusPendingCallWatcher* watcher) {
  watcher->deleteLater();
  QDBusPendingReply< QString> reply = *watcher;
  if (reply.isError())
    return;

  auto doc = QJsonDocument::fromJson(reply.value().toUtf8());
  if (!doc.isArray())
    return;

  QHash< QString, Claude_workspace_status> statuses;
  for (const auto& value : doc.array()) {
    auto obj = value.toObject();
    auto name = obj["name"].toString();
    statuses[name] = Claude_workspace_status{
      .workspace_name = name,
      .state = from_wire_string< Claude_state>(obj["state"].toString())
        .value_or(Claude_state::NOT_RUNNING),
      .tool_name = obj["tool_name"].toString(),
      .wait_reason = obj["wait_reason"].toString(),
      .wait_message = obj["wait_message"].toString(),
      .state_since_ms = obj["state_since_ms"].toVariant().toLongLong()
    };
  }
  _claude_statuses = statuses;
  emit claudeStatusesChanged();
}
