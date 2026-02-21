#include "daemon_server.h"
#include "menu_window.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QElapsedTimer>

static const QString socket_name = "workspace-menu";

Daemon_server::Daemon_server(Menu_window& window, QObject* parent)
  : QObject(parent)
  , _window(window)
  , _server(new QLocalServer(this))
{
  connect(_server, &QLocalServer::newConnection, this, &Daemon_server::on_new_connection);
  connect(&_window, &Menu_window::session_finished, this, &Daemon_server::on_session_finished);
}

bool Daemon_server::start() {
  QLocalServer::removeServer(socket_name);
  if (!_server->listen(socket_name)) {
    qCritical("workspace-menu: failed to listen on socket '%s': %s",
      qPrintable(socket_name), qPrintable(_server->errorString()));
    return false;
  }
  qInfo("workspace-menu: listening on %s", qPrintable(_server->fullServerName()));
  return true;
}

void Daemon_server::on_new_connection() {
  while (_server->hasPendingConnections()) {
    auto* client = _server->nextPendingConnection();

    // Early disconnect handler: clean up clients that disconnect before
    // being promoted to _active_client
    connect(client, &QLocalSocket::disconnected, this, [client]() {
      client->deleteLater();
    });

    connect(client, &QLocalSocket::readyRead, this, [this, client]() {
      if (!client->canReadLine()) {
        return;
      }

      // Disconnect readyRead so this lambda doesn't fire again
      disconnect(client, &QLocalSocket::readyRead, this, nullptr);

      auto line = QString::fromUtf8(client->readLine()).trimmed();
      auto parts = line.split(' ');

      if (parts.isEmpty() || parts[0] != "show") {
        client->write("error\n");
        client->flush();
        client->disconnectFromServer();
        return;
      }

      qint64 client_timestamp_ms = 0;
      if (parts.size() > 1) {
        client_timestamp_ms = parts[1].toLongLong();
      }

      if (_active_client) {
        client->write("busy\n");
        client->flush();
        client->disconnectFromServer();
        return;
      }

      // Replace early cleanup handler with session-aware one
      disconnect(client, &QLocalSocket::disconnected, nullptr, nullptr);

      _active_client = client;
      connect(_active_client, &QLocalSocket::disconnected,
        this, &Daemon_server::on_client_disconnected);

      _window.activate(client_timestamp_ms);
    });
  }
}

void Daemon_server::on_session_finished(const QString& response) {
  if (!_active_client) {
    return;
  }

  _active_client->write(response.toUtf8() + '\n');
  _active_client->flush();
  _active_client->disconnectFromServer();
  reset_client();
}

void Daemon_server::on_client_disconnected() {
  if (!_active_client) {
    return;
  }

  // Null out _active_client BEFORE cancel_session to prevent re-entrant
  // on_session_finished from writing to the already-disconnected socket
  reset_client();
  _window.cancel_session();
}

void Daemon_server::reset_client() {
  if (_active_client) {
    _active_client->deleteLater();
    _active_client = nullptr;
  }
}
