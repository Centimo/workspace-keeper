#pragma once

#include <QObject>

class QLocalServer;
class QLocalSocket;
class Menu_window;

class Daemon_server : public QObject {
  Q_OBJECT

 public:
  explicit Daemon_server(Menu_window& window, QObject* parent = nullptr);

  bool start();

 private:
  void on_new_connection();
  void on_session_finished(const QString& response);
  void on_client_disconnected();
  void reset_client();

  Menu_window& _window;
  QLocalServer* _server;
  QLocalSocket* _active_client = nullptr;
};
