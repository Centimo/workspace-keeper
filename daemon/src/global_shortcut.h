#pragma once

#include <QObject>
#include <QKeySequence>

/// Registers a global keyboard shortcut via KGlobalAccel D-Bus service.
/// Does not require linking against KF5/KF6 GlobalAccel library — uses D-Bus directly.
/// Automatically re-registers when kglobalaccel restarts.
class Global_shortcut : public QObject {
  Q_OBJECT

 public:
  explicit Global_shortcut(
    const QString& action_name,
    const QString& friendly_name,
    const QKeySequence& shortcut,
    QObject* parent = nullptr
  );

 signals:
  void triggered();

 private slots:
  void on_shortcut_pressed(
    const QString& component_unique,
    const QString& shortcut_unique,
    qlonglong timestamp
  );

  void on_service_owner_changed(
    const QString& service_name,
    const QString& old_owner,
    const QString& new_owner
  );

 private:
  void register_shortcut();

  QString _component_name;
  QString _action_name;
  QString _friendly_name;
  QKeySequence _shortcut;
  QString _connected_component_path;
};
