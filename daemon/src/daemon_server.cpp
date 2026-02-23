#include "daemon_server.h"
#include "journal_log.h"
#include "menu_window.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QDateTime>
#include <QDir>
#include <QProcess>

static const QString socket_name = "workspace-menu";

static const QString& workspace_bin() {
  static const QString path = QDir::homePath() + "/.local/bin/workspace";
  return path;
}

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
    qCCritical(logServer, "failed to listen on socket '%s': %s",
      qPrintable(socket_name), qPrintable(_server->errorString()));
    return false;
  }
  qCInfo(logServer, "listening on %s", qPrintable(_server->fullServerName()));
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
      disconnect(client, &QLocalSocket::disconnected, this, nullptr);

      _active_client = client;
      connect(_active_client, &QLocalSocket::disconnected,
        this, &Daemon_server::on_client_disconnected);

      _window.activate(client_timestamp_ms);
    });
  }
}

void Daemon_server::on_session_finished(const QString& response) {
  if (_shortcut_session) {
    _shortcut_session = false;

    qCInfo(logServer, "shortcut session response: '%s'", qPrintable(response));

    if (!response.isEmpty()
      && !response.startsWith("cancelled")
      && !response.startsWith("error"))
    {
      auto* process = new QProcess(this);
      connect(process, &QProcess::finished, this,
        [process](int exit_code, QProcess::ExitStatus status) {
          if (exit_code != 0 || status != QProcess::NormalExit) {
            qCWarning(logServer, "handle-response: exit code %d, stderr: %s",
              exit_code, process->readAllStandardError().constData());
          }
          process->deleteLater();
        });
      connect(process, &QProcess::errorOccurred, this, [process](QProcess::ProcessError err) {
        qCWarning(logServer, "handle-response: process error %d: %s",
          static_cast<int>(err), qPrintable(process->errorString()));
        // Only delete here for FailedToStart — finished() won't be emitted in that case.
        // For all other errors, finished() follows and handles cleanup.
        if (err == QProcess::FailedToStart) {
          process->deleteLater();
        }
      });
      process->start(workspace_bin(), {"handle-response", response});
    }
    return;
  }

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

void Daemon_server::trigger_from_shortcut() {
  if (_active_client || _shortcut_session || _window.isVisible()) {
    return;
  }

  _shortcut_session = true;
  _window.activate(QDateTime::currentMSecsSinceEpoch());
}

void Daemon_server::reset_client() {
  if (_active_client) {
    _active_client->deleteLater();
    _active_client = nullptr;
  }
}
