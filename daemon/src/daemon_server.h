#pragma once

#include <QObject>

class QLocalServer;
class QLocalSocket;
class Menu_window;

/// Unix socket server for the popup menu (show command only).
/// Claude status and workspace management go through D-Bus.
class Daemon_server : public QObject {
  Q_OBJECT

 public:
  explicit Daemon_server(Menu_window& window, QObject* parent = nullptr);

  bool start();

 public slots:
  void trigger_from_shortcut();

 private:
  void on_new_connection();
  void on_session_finished(const QString& response);
  void on_client_disconnected();
  void reset_client();

  Menu_window& _window;
  QLocalServer* _server;
  QLocalSocket* _active_client = nullptr;
  bool _shortcut_session = false;
};
