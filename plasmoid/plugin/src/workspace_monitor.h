#pragma once

#include <claude_types.h>
#include <kwin_desktop.h>

#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class QDBusMessage;

/// QML type that exposes virtual desktop list and Claude Code statuses
/// via D-Bus signal subscriptions (push model, no polling).
class Workspace_monitor : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList desktops READ desktops NOTIFY desktopsChanged)
  Q_PROPERTY(QVariantMap claudeStatuses READ claude_statuses NOTIFY claudeStatusesChanged)

 public:
  explicit Workspace_monitor(QObject* parent = nullptr);

  QVariantList desktops() const {
    QVariantList result;
    result.reserve(_desktops.size());
    for (const auto& d : _desktops)
      result.append(d.to_variant_map());
    return result;
  }

  QVariantMap claude_statuses() const {
    QVariantMap result;
    for (auto it = _claude_statuses.begin(); it != _claude_statuses.end(); ++it)
      result[it.key()] = it->to_variant_map();
    return result;
  }

  Q_INVOKABLE void switchToDesktop(int index);

 signals:
  void desktopsChanged();
  void claudeStatusesChanged();

 private slots:
  void on_desktop_created(const QDBusMessage& message);
  void on_desktop_removed(const QDBusMessage& message);
  void on_desktop_data_changed(const QDBusMessage& message);

  void on_status_changed(
    const QString& workspace_name,
    const QString& state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qlonglong state_since_ms
  );

  void on_daemon_registered();
  void on_daemon_unregistered();

  void on_desktops_fetched(QDBusPendingCallWatcher* watcher);
  void on_statuses_fetched(QDBusPendingCallWatcher* watcher);

 private:
  void fetch_desktops();
  void fetch_all_statuses();

  QVector< Kwin_desktop> _desktops;
  QHash< QString, Claude_workspace_status> _claude_statuses;
  QDBusServiceWatcher _daemon_watcher;
};
