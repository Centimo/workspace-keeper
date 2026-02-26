#include "claude_status_server.h"
#include "claude_status_tracker.h"
#include "journal_log.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <cstdlib>

Claude_status_server::Claude_status_server(Claude_status_tracker& tracker, QObject* parent)
  : QObject(parent)
  , _tracker(tracker)
  , _server(new QLocalServer(this))
{
  connect(_server, &QLocalServer::newConnection, this, &Claude_status_server::on_new_connection);
}

bool Claude_status_server::start() {
  auto path = socket_path();
  QLocalServer::removeServer(path);
  if (!_server->listen(path)) {
    qCCritical(logClaude, "failed to listen on socket '%s': %s",
      qPrintable(path), qPrintable(_server->errorString()));
    return false;
  }
  qCInfo(logClaude, "listening on %s", qPrintable(_server->fullServerName()));
  return true;
}

void Claude_status_server::process_buffered_lines(QLocalSocket* client) {
  while (client->canReadLine()) {
    auto line = QString::fromUtf8(client->readLine()).trimmed();
    if (line.isEmpty()) {
      continue;
    }

    // Protocol: "status\t<workspace>\t<event>\t[args...]" (tab-delimited)
    auto parts = line.split('\t');
    if (parts.size() < 3 || parts[0] != "status") {
      qCWarning(logClaude, "malformed protocol line: '%s'", qPrintable(line));
      continue;
    }

    const auto& workspace = parts[1];
    const auto& event_type = parts[2];
    auto args = parts.mid(3);

    _tracker.process_event(workspace, event_type, args);
  }
}

void Claude_status_server::on_new_connection() {
  while (_server->hasPendingConnections()) {
    auto* client = _server->nextPendingConnection();

    connect(client, &QLocalSocket::readyRead, this, [this, client]() {
      process_buffered_lines(client);
    });

    connect(client, &QLocalSocket::disconnected, this, [this, client]() {
      // Disconnect readyRead to prevent use-after-free if both signals
      // are queued in the same event loop iteration
      disconnect(client, &QLocalSocket::readyRead, this, nullptr);
      process_buffered_lines(client);
      client->deleteLater();
    });
  }
}

QString Claude_status_server::socket_path() {
  auto runtime_dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (runtime_dir.isEmpty()) {
    runtime_dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  }
  if (runtime_dir.isEmpty()) {
    runtime_dir = "/tmp";
  }
  return runtime_dir + "/workspace-claude-status";
}
