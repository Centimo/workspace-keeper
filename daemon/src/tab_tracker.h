#pragma once

#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class Workspace_db;

/// Connects to the BroTab event socket and auto-saves tabs for all active
/// workspaces whenever tab changes are detected (debounced).
class Tab_tracker : public QObject {
  Q_OBJECT

 public:
  explicit Tab_tracker(Workspace_db& db, QObject* parent = nullptr);

  /// Try to connect to the BroTab event socket. Retries periodically if unavailable.
  void start();

 private slots:
  void try_connect();
  void on_connected();
  void on_disconnected();
  void on_socket_error(QLocalSocket::LocalSocketError error);
  void on_ready_read();
  void request_tab_list();
  void on_tab_list_reply(QNetworkReply* reply);

 private:
  void save_tabs_from_response(const QByteArray& body);

  Workspace_db& _db;

  QLocalSocket _socket;
  QTimer _debounce_timer;
  QTimer _reconnect_timer;
  QNetworkAccessManager _network;
  QByteArray _read_buffer;
  bool _save_in_progress = false;
  int _current_port = 0;
};
