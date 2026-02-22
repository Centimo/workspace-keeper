#include "global_shortcut.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

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
{
  auto bus = QDBusConnection::sessionBus();

  // actionId: [componentUnique, actionUnique, componentFriendly, actionFriendly]
  QStringList action_id = {
    _component_name,
    _action_name,
    _component_name,
    friendly_name
  };

  if (shortcut.count() == 0) {
    qWarning("Global_shortcut: empty key sequence for action '%s'", qPrintable(action_name));
    return;
  }

  // D-Bus method setShortcut expects keys as QList<int> (D-Bus type "ai")
  QList<int> keys = {static_cast<int>(shortcut[0].toCombined())};

  QDBusInterface iface(kglobalaccel_service, kglobalaccel_path, kglobalaccel_iface, bus);

  // doRegister must be called before setShortcut to create the action in kglobalaccel
  auto reg_reply = iface.call("doRegister", action_id);
  if (reg_reply.type() == QDBusMessage::ErrorMessage) {
    qWarning("Global_shortcut: doRegister failed for '%s': %s",
      qPrintable(action_name), qPrintable(reg_reply.errorMessage()));
    return;
  }

  // SetPresent=2 marks the shortcut as active
  auto set_reply = iface.call("setShortcut", action_id, QVariant::fromValue(keys), 2u);
  if (set_reply.type() == QDBusMessage::ErrorMessage) {
    qWarning("Global_shortcut: setShortcut failed for '%s': %s",
      qPrintable(action_name), qPrintable(set_reply.errorMessage()));
    return;
  }

  QDBusReply<QDBusObjectPath> component_reply = iface.call("getComponent", _component_name);
  if (!component_reply.isValid()) {
    qWarning("Global_shortcut: failed to get component '%s': %s",
      qPrintable(_component_name), qPrintable(component_reply.error().message()));
    return;
  }

  auto component_path = component_reply.value().path();

  // Connect to the globalShortcutPressed signal on the component
  bus.connect(
    kglobalaccel_service,
    component_path,
    component_iface,
    "globalShortcutPressed",
    this,
    SLOT(on_shortcut_pressed(QString, QString, qlonglong))
  );
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
