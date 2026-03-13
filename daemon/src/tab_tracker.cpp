#include "tab_tracker.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>

static constexpr int debounce_ms = 2000;
static constexpr int reconnect_ms = 5000;
static constexpr int first_mediator_port = 4625;
static constexpr int last_mediator_port = 4635;
static constexpr char floorp_suffix[] = " \xe2\x80\x94 Ablaze Floorp"; // " — Ablaze Floorp"

static QString find_event_socket() {
  QDir tmp("/tmp");
  auto entries = tmp.entryList({"brotab-events-*.sock"}, QDir::System);
  if (entries.isEmpty())
    return {};
  return "/tmp/" + entries.first();
}

struct Floorp_window {
  int desktop_index;
  QString title_without_suffix;
};

static QVector< Floorp_window> get_floorp_windows() {
  QVector< Floorp_window> result;

  QProcess process;
  process.start("wmctrl", {"-l"});
  if (!process.waitForFinished(3000))
    return result;

  static const QRegularExpression whitespace("\\s+");
  const auto lines = process.readAllStandardOutput().split('\n');
  for (const auto& line : lines) {
    if (!line.contains("Ablaze Floorp"))
      continue;

    // Format: 0xHEXID  desktop_idx  hostname  title...
    auto parts = QString::fromUtf8(line).split(whitespace, Qt::SkipEmptyParts);
    if (parts.size() < 4)
      continue;

    bool ok = false;
    int desktop_index = parts[1].toInt(&ok);
    if (!ok || desktop_index < 0)
      continue;

    QString title = QStringList(parts.mid(3)).join(' ');
    if (title.endsWith(floorp_suffix))
      title.chop(static_cast< int>(strlen(floorp_suffix)));

    result.append({desktop_index, title});
  }
  return result;
}

static QHash< int, QString> get_desktop_names() {
  QHash< int, QString> result;

  QProcess process;
  process.start("wmctrl", {"-d"});
  if (!process.waitForFinished(3000))
    return result;

  static const QRegularExpression whitespace("\\s+");
  const auto lines = process.readAllStandardOutput().split('\n');
  for (const auto& line : lines) {
    if (line.isEmpty())
      continue;
    // Format: 0  * DG: WxH  VP: X,Y  WA: X,Y WxH  name
    auto parts = QString::fromUtf8(line).split(whitespace, Qt::SkipEmptyParts);
    if (parts.size() < 9)
      continue;
    bool ok = false;
    int idx = parts[0].toInt(&ok);
    if (!ok)
      continue;
    result.insert(idx, QStringList(parts.mid(8)).join(' '));
  }
  return result;
}

Tab_tracker::Tab_tracker(Workspace_db& db, QObject* parent)
  : QObject(parent)
  , _db(db)
{
  _debounce_timer.setSingleShot(true);
  _debounce_timer.setInterval(debounce_ms);
  connect(&_debounce_timer, &QTimer::timeout, this, &Tab_tracker::request_tab_list);

  _reconnect_timer.setInterval(reconnect_ms);
  connect(&_reconnect_timer, &QTimer::timeout, this, &Tab_tracker::try_connect);

  connect(&_socket, &QLocalSocket::connected, this, &Tab_tracker::on_connected);
  connect(&_socket, &QLocalSocket::disconnected, this, &Tab_tracker::on_disconnected);
  connect(&_socket, &QLocalSocket::errorOccurred, this, &Tab_tracker::on_socket_error);
  connect(&_socket, &QLocalSocket::readyRead, this, &Tab_tracker::on_ready_read);

  connect(&_network, &QNetworkAccessManager::finished, this, &Tab_tracker::on_tab_list_reply);
}

void Tab_tracker::start() {
  try_connect();
  _reconnect_timer.start();
}

void Tab_tracker::try_connect() {
  if (_socket.state() != QLocalSocket::UnconnectedState)
    return;

  auto path = find_event_socket();
  if (path.isEmpty())
    return;

  qCInfo(logServer, "Tab_tracker: connecting to %s", qPrintable(path));
  _socket.connectToServer(path);
}

void Tab_tracker::on_connected() {
  qCInfo(logServer, "Tab_tracker: connected to BroTab event socket");
  _reconnect_timer.stop();
}

void Tab_tracker::on_disconnected() {
  qCInfo(logServer, "Tab_tracker: disconnected from BroTab event socket");
  _read_buffer.clear();
  _reconnect_timer.start();
}

void Tab_tracker::on_socket_error(QLocalSocket::LocalSocketError error) {
  qCInfo(logServer, "Tab_tracker: socket error %d", static_cast< int>(error));
}

void Tab_tracker::on_ready_read() {
  _read_buffer.append(_socket.readAll());

  while (_read_buffer.contains('\n')) {
    int newline_pos = _read_buffer.indexOf('\n');
    _read_buffer.remove(0, newline_pos + 1);
    _debounce_timer.start();
  }
}

void Tab_tracker::request_tab_list() {
  if (_save_in_progress)
    return;

  _save_in_progress = true;
  _current_port = first_mediator_port;

  QNetworkRequest request(QUrl(QString("http://localhost:%1/list_tabs").arg(_current_port)));
  request.setTransferTimeout(3000);
  _network.get(request);
}

void Tab_tracker::on_tab_list_reply(QNetworkReply* reply) {
  reply->deleteLater();

  if (reply->error() == QNetworkReply::NoError) {
    save_tabs_from_response(reply->readAll());
    _save_in_progress = false;
    return;
  }

  // Try next port
  ++_current_port;
  if (_current_port < last_mediator_port) {
    QNetworkRequest request(QUrl(QString("http://localhost:%1/list_tabs").arg(_current_port)));
    request.setTransferTimeout(3000);
    _network.get(request);
    return;
  }

  qCInfo(logServer, "Tab_tracker: no BroTab mediator responding");
  _save_in_progress = false;
}

void Tab_tracker::save_tabs_from_response(const QByteArray& body) {
  qCInfo(logServer, "Tab_tracker: saving tabs for all active workspaces");

  // 1. Parse BroTab tab list
  struct Brotab_tab {
    int window_id;
    QString title;
    QString url;
  };

  QVector< Brotab_tab> all_tabs;
  const auto lines = body.split('\n');
  for (const auto& line : lines) {
    if (line.isEmpty())
      continue;
    // Format: prefix.windowId.tabId\ttitle\turl
    auto cols = QString::fromUtf8(line).split('\t');
    if (cols.size() < 3)
      continue;
    auto id_parts = cols[0].split('.');
    if (id_parts.size() < 3)
      continue;
    bool ok = false;
    int window_id = id_parts[1].toInt(&ok);
    if (!ok)
      continue;
    all_tabs.append({window_id, cols[1], cols[2]});
  }

  if (all_tabs.isEmpty())
    return;

  // 2. Get Floorp X11 windows with their desktop indices
  auto floorp_windows = get_floorp_windows();
  if (floorp_windows.isEmpty()) {
    qCInfo(logServer, "Tab_tracker: no Floorp windows found");
    return;
  }

  // 3. Map BroTab windowId → desktop_index by matching tab titles to X11 window titles
  QHash< int, int> window_to_desktop;
  for (const auto& floorp : floorp_windows) {
    for (const auto& tab : all_tabs) {
      if (tab.title == floorp.title_without_suffix) {
        window_to_desktop.insert(tab.window_id, floorp.desktop_index);
        break;
      }
    }
  }

  // 4. Map desktop_index → workspace_name
  auto desktop_names = get_desktop_names();

  // 5. Group tabs by window and save per workspace
  QHash< int, QStringList> window_tabs;
  for (const auto& tab : all_tabs) {
    if (!tab.url.startsWith("about:"))
      window_tabs[tab.window_id].append(tab.url);
  }

  int saved_count = 0;
  for (auto it = window_to_desktop.constBegin(); it != window_to_desktop.constEnd(); ++it) {
    auto workspace_name = desktop_names.value(it.value());
    if (workspace_name.isEmpty())
      continue;

    auto urls = window_tabs.value(it.key());
    _db.set_tabs(workspace_name, urls);
    ++saved_count;
    qCInfo(logServer, "Tab_tracker: saved %d tabs for workspace '%s'",
      urls.size(), qPrintable(workspace_name));
  }

  qCInfo(logServer, "Tab_tracker: done, saved tabs for %d workspaces", saved_count);
}
