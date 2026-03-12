#include "desktop_monitor.h"
#include "journal_log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusVariant>

Desktop_monitor::Desktop_monitor(QObject* parent)
  : QObject(parent)
{
  auto bus = QDBusConnection::sessionBus();

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopCreated",
    this, SLOT(on_desktop_created(QDBusMessage))
  ))
    qCWarning(logServer, "Desktop_monitor: failed to connect desktopCreated signal");

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopRemoved",
    this, SLOT(on_desktop_removed(QDBusMessage))
  ))
    qCWarning(logServer, "Desktop_monitor: failed to connect desktopRemoved signal");

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "desktopDataChanged",
    this, SLOT(on_desktop_data_changed(QDBusMessage))
  ))
    qCWarning(logServer, "Desktop_monitor: failed to connect desktopDataChanged signal");

  if (!bus.connect(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "currentDesktopChanged",
    this, SLOT(on_current_desktop_changed(QDBusMessage))
  ))
    qCWarning(logServer, "Desktop_monitor: failed to connect currentDesktopChanged signal");

  fetch_desktops();
}

void Desktop_monitor::switch_to_desktop(int index) {
  if (index < 0 || index >= _desktops.size())
    return;

  const auto& id = _desktops[index].id;
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.freedesktop.DBus.Properties", "Set"
  );
  message << "org.kde.KWin.VirtualDesktopManager"
          << "current"
          << QVariant::fromValue(QDBusVariant(id));
  QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

void Desktop_monitor::switch_to_desktop_by_name(const QString& name) {
  for (int i = 0; i < _desktops.size(); ++i) {
    if (_desktops[i].name == name) {
      switch_to_desktop(i);
      return;
    }
  }
}

void Desktop_monitor::on_desktop_created(const QDBusMessage&) {
  fetch_desktops();
}

void Desktop_monitor::on_desktop_removed(const QDBusMessage&) {
  fetch_desktops();
}

void Desktop_monitor::on_desktop_data_changed(const QDBusMessage&) {
  fetch_desktops();
}

void Desktop_monitor::on_current_desktop_changed(const QDBusMessage& message) {
  // currentDesktopChanged(string id, int timestamp)
  auto args = message.arguments();
  if (args.isEmpty()) {
    return;
  }
  QString current_id = args[0].toString();
  for (const auto& desktop : _desktops) {
    if (desktop.id == current_id) {
      _current_desktop_name = desktop.name;
      emit desktops_changed();
      return;
    }
  }
  qCWarning(logServer, "Desktop_monitor: currentDesktopChanged for unknown id '%s'",
    qPrintable(current_id));
}

void Desktop_monitor::fetch_current_desktop() {
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.freedesktop.DBus.Properties", "Get"
  );
  message << "org.kde.KWin.VirtualDesktopManager" << "current";

  auto pending = QDBusConnection::sessionBus().asyncCall(message);
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
    w->deleteLater();
    QDBusPendingReply< QDBusVariant> reply = *w;
    if (reply.isError()) {
      qCWarning(logServer, "Desktop_monitor: fetch current desktop failed: %s",
        qPrintable(reply.error().message()));
      return;
    }
    QString current_id = reply.value().variant().toString();
    for (const auto& desktop : _desktops) {
      if (desktop.id == current_id) {
        _current_desktop_name = desktop.name;
        emit desktops_changed();
        return;
      }
    }
    qCWarning(logServer, "Desktop_monitor: current desktop id '%s' not found in desktops list",
      qPrintable(current_id));
    emit desktops_changed();
  });
}

void Desktop_monitor::fetch_desktops() {
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.freedesktop.DBus.Properties", "Get"
  );
  message << "org.kde.KWin.VirtualDesktopManager" << "desktops";

  auto pending = QDBusConnection::sessionBus().asyncCall(message);
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished,
    this, &Desktop_monitor::on_desktops_fetched);
}

void Desktop_monitor::on_desktops_fetched(QDBusPendingCallWatcher* watcher) {
  watcher->deleteLater();
  QDBusPendingReply< QDBusVariant> reply = *watcher;
  if (reply.isError()) {
    qCWarning(logServer, "Desktop_monitor: fetch desktops failed: %s",
      qPrintable(reply.error().message()));
    return;
  }

  auto argument = reply.value().variant().value< QDBusArgument>();
  _desktops = parse_desktops(argument);
  sort_by_position(_desktops);
  qCInfo(logServer, "Desktop_monitor: fetched %d desktops", static_cast< int>(_desktops.size()));
  fetch_current_desktop();
}
