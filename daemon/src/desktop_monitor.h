#pragma once

#include <kwin_desktop.h>

#include <QDBusPendingCallWatcher>
#include <QObject>
#include <QVector>

class QDBusMessage;

/// Monitors KWin virtual desktops via D-Bus signals (push model).
/// Provides desktop list sorted by position and desktop switching.
class Desktop_monitor : public QObject {
  Q_OBJECT

 public:
  explicit Desktop_monitor(QObject* parent = nullptr);

  const QVector< Kwin_desktop>& desktops() const { return _desktops; }
  const QString& current_desktop_name() const { return _current_desktop_name; }

  void switch_to_desktop(int index);
  void switch_to_desktop_by_name(const QString& name);

 signals:
  void desktops_changed();

 private slots:
  void on_desktop_created(const QDBusMessage& message);
  void on_desktop_removed(const QDBusMessage& message);
  void on_desktop_data_changed(const QDBusMessage& message);
  void on_current_desktop_changed(const QDBusMessage& message);
  void on_desktops_fetched(QDBusPendingCallWatcher* watcher);

 private:
  void fetch_desktops();
  void fetch_current_desktop();

  QVector< Kwin_desktop> _desktops;
  QString _current_desktop_name;
};
