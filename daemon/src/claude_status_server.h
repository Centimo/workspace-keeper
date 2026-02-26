#pragma once

#include <QObject>

class QLocalServer;
class QLocalSocket;
class Claude_status_tracker;

/// Accepts fire-and-forget connections on the Claude status socket.
/// Parses tab-delimited lines of form "status\t<workspace>\t<event>\t[args...]"
/// and forwards them to the Claude_status_tracker.
///
/// Socket path: $XDG_RUNTIME_DIR/workspace-claude-status (or /tmp/ fallback).
class Claude_status_server : public QObject {
  Q_OBJECT

 public:
  explicit Claude_status_server(Claude_status_tracker& tracker, QObject* parent = nullptr);

  bool start();

 private:
  void on_new_connection();
  void process_buffered_lines(QLocalSocket* client);

  Claude_status_tracker& _tracker;
  QLocalServer* _server;

  static QString socket_path();
};
