#include "desktop_monitor.h"
#include "journal_log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace {

/// Parse KWin VirtualDesktop struct (position: uint, id: string, name: string).
QVariantMap parse_desktop(const QDBusArgument& argument) {
  uint position = 0;
  QString id;
  QString name;
  argument.beginStructure();
  argument >> position >> id >> name;
  argument.endStructure();
  return {{"id", id}, {"name", name}, {"position", position}};
}

/// Parse desktops array from a QDBusArgument containing a(uss).
QVariantList parse_desktops(const QDBusArgument& argument) {
  QVariantList result;
  argument.beginArray();
  while (!argument.atEnd())
    result.append(parse_desktop(argument));
  argument.endArray();
  return result;
}

void sort_by_position(QVariantList& desktops) {
  std::sort(desktops.begin(), desktops.end(), [](const QVariant& a, const QVariant& b) {
    return a.toMap()["position"].toUInt() < b.toMap()["position"].toUInt();
  });
}

} // namespace

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

  fetch_desktops();
}

void Desktop_monitor::switch_to_desktop(int index) {
  if (index < 0 || index >= _desktops.size())
    return;

  auto id = _desktops[index].toMap()["id"].toString();
  auto message = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/VirtualDesktopManager",
    "org.kde.KWin.VirtualDesktopManager", "setCurrentDesktop"
  );
  message << id;
  QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
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
  emit desktops_changed();
}
