#pragma once

#include <QObject>

class QLocalServer;
class QLocalSocket;
class Claude_status_tracker;
class Menu_window;

class Daemon_server : public QObject {
  Q_OBJECT

 public:
  explicit Daemon_server(
    Menu_window& window,
    Claude_status_tracker* claude_tracker = nullptr,
    QObject* parent = nullptr
  );

  bool start();

 public slots:
  void trigger_from_shortcut();

 private:
  void on_new_connection();
  void on_session_finished(const QString& response);
  void on_client_disconnected();
  void reset_client();
  void handle_status_line(const QString& line);

  Menu_window& _window;
  Claude_status_tracker* _claude_tracker;
  QLocalServer* _server;
  QLocalSocket* _active_client = nullptr;
  bool _shortcut_session = false;
};
