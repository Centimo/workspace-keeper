#include "global_shortcut.h"
#include "journal_log.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>

static const QString kglobalaccel_service = "org.kde.kglobalaccel";
static const QString kglobalaccel_path = "/kglobalaccel";
static const QString kglobalaccel_iface = "org.kde.KGlobalAccel";
static const QString component_iface = "org.kde.kglobalaccel.Component";

Global_shortcut::Global_shortcut(
  const QString& action_name,
  const QString& friendly_name,
  const QKeySequence& shortcut,
  QObject* parent
)
  : QObject(parent)
  , _component_name(QCoreApplication::applicationName())
  , _action_name(action_name)
  , _friendly_name(friendly_name)
  , _shortcut(shortcut)
{
  if (_shortcut.count() == 0) {
    qCWarning(logShortcut, "empty key sequence for action '%s'", qPrintable(action_name));
    return;
  }

  auto bus = QDBusConnection::sessionBus();

  // Watch for kglobalaccel restarts to re-register
  auto* watcher = new QDBusServiceWatcher(
    kglobalaccel_service,
    bus,
    QDBusServiceWatcher::WatchForOwnerChange,
    this
  );
  connect(
    watcher, &QDBusServiceWatcher::serviceOwnerChanged,
    this, &Global_shortcut::on_service_owner_changed
  );

  register_shortcut();
}

void Global_shortcut::register_shortcut() {
  auto bus = QDBusConnection::sessionBus();

  QStringList action_id = {
    _component_name,
    _action_name,
    _component_name,
    _friendly_name
  };

  QList<int> keys = {static_cast<int>(_shortcut[0].toCombined())};

  QDBusInterface iface(kglobalaccel_service, kglobalaccel_path, kglobalaccel_iface, bus);

  auto reg_reply = iface.call("doRegister", action_id);
  if (reg_reply.type() == QDBusMessage::ErrorMessage) {
    qCWarning(logShortcut, "doRegister failed for '%s': %s",
      qPrintable(_action_name), qPrintable(reg_reply.errorMessage()));
    return;
  }

  // SetPresent=2 marks the shortcut as active, NoAutoloading=4 forces our keys
  // even if kglobalaccel has a previously saved (possibly empty) binding.
  auto set_reply = iface.call("setShortcut", action_id, QVariant::fromValue(keys), 6u);
  if (set_reply.type() == QDBusMessage::ErrorMessage) {
    qCWarning(logShortcut, "setShortcut failed for '%s': %s",
      qPrintable(_action_name), qPrintable(set_reply.errorMessage()));
    return;
  }

  QDBusReply<QDBusObjectPath> component_reply = iface.call("getComponent", _component_name);
  if (!component_reply.isValid()) {
    qCWarning(logShortcut, "failed to get component '%s': %s",
      qPrintable(_component_name), qPrintable(component_reply.error().message()));
    return;
  }

  auto component_path = component_reply.value().path();

  // Disconnect previous signal connection to avoid duplicates on re-register
  if (!_connected_component_path.isEmpty()) {
    bus.disconnect(
      kglobalaccel_service,
      _connected_component_path,
      component_iface,
      "globalShortcutPressed",
      this,
      SLOT(on_shortcut_pressed(QString, QString, qlonglong))
    );
  }

  bus.connect(
    kglobalaccel_service,
    component_path,
    component_iface,
    "globalShortcutPressed",
    this,
    SLOT(on_shortcut_pressed(QString, QString, qlonglong))
  );
  _connected_component_path = component_path;

  qCInfo(logShortcut, "registered '%s' (%s)",
    qPrintable(_action_name), qPrintable(_shortcut.toString()));
}

void Global_shortcut::on_service_owner_changed(
  const QString& /*service_name*/,
  const QString& /*old_owner*/,
  const QString& new_owner
) {
  if (new_owner.isEmpty())
    return;

  qCInfo(logShortcut, "kglobalaccel appeared, re-registering '%s'",
    qPrintable(_action_name));
  register_shortcut();
}

void Global_shortcut::on_shortcut_pressed(
  const QString& component_unique,
  const QString& shortcut_unique,
  qlonglong /*timestamp*/
) {
  if (component_unique == _component_name && shortcut_unique == _action_name) {
    emit triggered();
  }
}
