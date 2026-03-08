#include "status_overlay.h"
#include "desktop_monitor.h"
#include "enum_strings.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QToolTip>

namespace {

/// Use KWin scripting via D-Bus to set window properties that KWin
/// otherwise overrides when set directly through X11.
void kwin_set_on_all_desktops(const QString& window_caption) {
  auto script_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
    + "/workspace-menu";
  auto script_path = script_dir + "/kwin-sticky.js";

  QFile file(script_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qCWarning(logServer, "Status_overlay: failed to write KWin script to '%s'",
      qPrintable(script_path));
    return;
  }
  file.write(
    "var clients = workspace.clientList();\n"
    "for (var i = 0; i < clients.length; i++) {\n"
    "  if (clients[i].caption === \"" + window_caption.toUtf8() + "\") {\n"
    "    clients[i].onAllDesktops = true;\n"
    "    clients[i].skipTaskbar = true;\n"
    "    clients[i].skipPager = true;\n"
    "  }\n"
    "}\n"
  );
  file.close();

  auto bus = QDBusConnection::sessionBus();

  auto load_msg = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/Scripting",
    "org.kde.kwin.Scripting", "loadScript"
  );
  load_msg << script_path << "workspace-menu-sticky";

  auto reply = bus.call(load_msg);
  if (reply.type() == QDBusMessage::ErrorMessage) {
    qCWarning(logServer, "Status_overlay: KWin loadScript failed: %s",
      qPrintable(reply.errorMessage()));
    return;
  }

  auto script_id = reply.arguments().first().toInt();
  if (script_id < 0) {
    // Script name already registered — unload and retry
    auto unload_msg = QDBusMessage::createMethodCall(
      "org.kde.KWin", "/Scripting",
      "org.kde.kwin.Scripting", "unloadScript"
    );
    unload_msg << "workspace-menu-sticky";
    bus.call(unload_msg);

    reply = bus.call(load_msg);
    script_id = reply.arguments().first().toInt();
    if (script_id < 0)
      return;
  }

  // start() runs all loaded-but-not-yet-started scripts
  auto start_msg = QDBusMessage::createMethodCall(
    "org.kde.KWin", "/Scripting",
    "org.kde.kwin.Scripting", "start"
  );
  bus.call(start_msg);

  // Unload after a short delay to not leave scripts registered
  QTimer::singleShot(500, [bus]() mutable {
    auto unload_msg = QDBusMessage::createMethodCall(
      "org.kde.KWin", "/Scripting",
      "org.kde.kwin.Scripting", "unloadScript"
    );
    unload_msg << "workspace-menu-sticky";
    bus.call(unload_msg, QDBus::NoBlock);
  });
}

} // namespace

Status_overlay::Status_overlay(
  Desktop_monitor& desktop_monitor,
  Workspace_db& db,
  QWidget* parent
)
  : QWidget(parent)
  , _desktop_monitor(desktop_monitor)
  , _db(db)
{
  setWindowFlags(
    Qt::FramelessWindowHint
    | Qt::WindowStaysOnTopHint
    | Qt::Tool
  );
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setMouseTracking(true);

  connect(
    &_desktop_monitor, &Desktop_monitor::desktops_changed,
    this, &Status_overlay::on_desktops_changed
  );

  restore_geometry();
}

void Status_overlay::on_status_changed(
  const QString& workspace,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message,
  qint64 state_since_ms
) {
  QVariantMap entry;
  entry["state"] = static_cast< int>(state);
  entry["tool_name"] = tool_name;
  entry["wait_reason"] = wait_reason;
  entry["wait_message"] = wait_message;
  entry["state_since_ms"] = state_since_ms;
  _claude_statuses[workspace] = entry;
  update_cells();
  update();
}

void Status_overlay::on_desktops_changed() {
  update_cells();

  if (!isVisible()) {
    auto target = size_for_columns(_columns);
    setFixedSize(target);
    show();
    apply_x11_sticky();
  }

  update();
}

void Status_overlay::toggle_edit_mode() {
  _edit_mode = !_edit_mode;

  if (_edit_mode) {
    setMinimumSize(_cell_size + 2 * _padding, _cell_size + 2 * _padding);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  }
  else {
    // Lock to current grid size
    _columns = columns_for_width(width());
    auto target = size_for_columns(_columns);
    setFixedSize(target);
    save_geometry();
    unsetCursor();
  }

  update();
}

void Status_overlay::update_cells() {
  _cells.clear();
  const auto& desktops = _desktop_monitor.desktops();
  for (const auto& desktop : desktops) {
    auto name = desktop.toMap()["name"].toString();
    Cell_info cell;
    cell.workspace_name = name;

    auto it = _claude_statuses.find(name);
    if (it != _claude_statuses.end()) {
      auto entry = it->toMap();
      cell.state = static_cast< Claude_state>(entry["state"].toInt());
      cell.tool_name = entry["tool_name"].toString();
      cell.wait_reason = entry["wait_reason"].toString();
      cell.wait_message = entry["wait_message"].toString();
      cell.state_since_ms = entry["state_since_ms"].toLongLong();
    }

    _cells.append(cell);
  }
}

void Status_overlay::apply_x11_sticky() {
  kwin_set_on_all_desktops(QCoreApplication::applicationName());
}

void Status_overlay::save_geometry() {
  _db.set_meta("overlay_x", QString::number(x()));
  _db.set_meta("overlay_y", QString::number(y()));
  _db.set_meta("overlay_columns", QString::number(_columns));
}

void Status_overlay::restore_geometry() {
  auto sc = _db.get_meta("overlay_columns");
  if (!sc.isEmpty())
    _columns = qMax(1, sc.toInt());

  auto sx = _db.get_meta("overlay_x");
  auto sy = _db.get_meta("overlay_y");

  if (!sx.isEmpty() && !sy.isEmpty()) {
    int px = sx.toInt();
    int py = sy.toInt();

    bool valid = false;
    for (auto* screen : QGuiApplication::screens()) {
      if (screen->geometry().contains(px, py)) {
        valid = true;
        break;
      }
    }

    if (valid) {
      move(px, py);
      return;
    }
  }

  // Default: top-right corner of primary screen
  auto* screen = QGuiApplication::primaryScreen();
  if (screen) {
    auto geometry = screen->availableGeometry();
    auto target = size_for_columns(_columns);
    move(geometry.right() - target.width() - 20, geometry.top() + 20);
  }
}

int Status_overlay::columns_for_width(int width) const {
  int inner = width - 2 * _padding + _spacing;
  int cols = inner / (_cell_size + _spacing);
  return qMax(1, cols);
}

QSize Status_overlay::size_for_columns(int columns) const {
  int count = qMax(_cells.size(), 1);
  int rows = (count + columns - 1) / columns;
  int w = columns * _cell_size + (columns - 1) * _spacing + 2 * _padding;
  int h = rows * _cell_size + (rows - 1) * _spacing + 2 * _padding;
  return {w, h};
}

QRect Status_overlay::cell_rect(int index) const {
  int col = index % _columns;
  int row = index / _columns;
  return QRect(
    _padding + col * (_cell_size + _spacing),
    _padding + row * (_cell_size + _spacing),
    _cell_size,
    _cell_size
  );
}

int Status_overlay::cell_at(const QPoint& pos) const {
  for (int i = 0; i < _cells.size(); ++i) {
    if (cell_rect(i).contains(pos))
      return i;
  }
  return -1;
}

QString Status_overlay::tooltip_text(const Cell_info& cell) const {
  QString text = cell.workspace_name;

  if (cell.state == Claude_state::NOT_RUNNING) {
    text += "\nClaude not running";
    return text;
  }

  text += "\nState: " + to_wire_string(cell.state);
  if (!cell.tool_name.isEmpty())
    text += "\nTool: " + cell.tool_name;
  if (!cell.wait_reason.isEmpty())
    text += "\nWaiting for: " + cell.wait_reason;
  if (!cell.wait_message.isEmpty())
    text += "\nMessage: " + cell.wait_message;

  if (cell.state_since_ms > 0) {
    auto elapsed = (QDateTime::currentMSecsSinceEpoch() - cell.state_since_ms) / 1000;
    if (elapsed < 60)
      text += "\nDuration: " + QString::number(elapsed) + "s";
    else
      text += "\nDuration: " + QString::number(elapsed / 60) + "m " + QString::number(elapsed % 60) + "s";
  }

  return text;
}

unsigned Status_overlay::edges_at(const QPoint& pos) const {
  unsigned edges = EDGE_NONE;
  if (pos.x() < _edge_margin)              edges |= EDGE_LEFT;
  if (pos.x() >= width() - _edge_margin)   edges |= EDGE_RIGHT;
  if (pos.y() < _edge_margin)              edges |= EDGE_TOP;
  if (pos.y() >= height() - _edge_margin)  edges |= EDGE_BOTTOM;
  return edges;
}

Qt::CursorShape Status_overlay::cursor_for_edges(unsigned edges) const {
  if ((edges & (EDGE_TOP | EDGE_LEFT)) == (EDGE_TOP | EDGE_LEFT))       return Qt::SizeFDiagCursor;
  if ((edges & (EDGE_BOTTOM | EDGE_RIGHT)) == (EDGE_BOTTOM | EDGE_RIGHT)) return Qt::SizeFDiagCursor;
  if ((edges & (EDGE_TOP | EDGE_RIGHT)) == (EDGE_TOP | EDGE_RIGHT))     return Qt::SizeBDiagCursor;
  if ((edges & (EDGE_BOTTOM | EDGE_LEFT)) == (EDGE_BOTTOM | EDGE_LEFT)) return Qt::SizeBDiagCursor;
  if (edges & (EDGE_LEFT | EDGE_RIGHT))  return Qt::SizeHorCursor;
  if (edges & (EDGE_TOP | EDGE_BOTTOM))  return Qt::SizeVerCursor;
  return Qt::ArrowCursor;
}

QColor Status_overlay::state_color(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return QColor(0x1d, 0x5f, 0xa0);
    case Claude_state::WORKING:     return QColor(0x2d, 0x8a, 0x4e);
    case Claude_state::WAITING:     return QColor(0xb0, 0x80, 0x20);
    case Claude_state::IDLE:        return QColor(0x55, 0x55, 0x55);
    case Claude_state::NOT_RUNNING: return QColor(0x3a, 0x3a, 0x3a);
  }
  return QColor(0x3a, 0x3a, 0x3a);
}

QColor Status_overlay::state_text_color(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return QColor(0xff, 0xff, 0xff);
    case Claude_state::WORKING:     return QColor(0xff, 0xff, 0xff);
    case Claude_state::WAITING:     return QColor(0xff, 0xff, 0xff);
    case Claude_state::IDLE:        return QColor(0xaa, 0xaa, 0xaa);
    case Claude_state::NOT_RUNNING: return QColor(0x66, 0x66, 0x66);
  }
  return QColor(0x66, 0x66, 0x66);
}

QString Status_overlay::state_label(Claude_state state) {
  switch (state) {
    case Claude_state::REQUESTING:  return "R";
    case Claude_state::WORKING:     return "W";
    case Claude_state::WAITING:     return "?";
    case Claude_state::IDLE:        return "_";
    case Claude_state::NOT_RUNNING: return "-";
  }
  return "-";
}

// --- Event handling ---

void Status_overlay::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Background
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(30, 30, 30, _edit_mode ? 230 : 200));
  painter.drawRoundedRect(rect(), _corner_radius, _corner_radius);

  // Cells
  QFont font("Hack");
  font.setPixelSize(static_cast< int>(_cell_size * 0.5));
  font.setBold(true);
  painter.setFont(font);

  for (int i = 0; i < _cells.size(); ++i) {
    auto rect = cell_rect(i);
    auto color = state_color(_cells[i].state);

    painter.setPen(QPen(color.lighter(130), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(rect, 3, 3);

    painter.setPen(state_text_color(_cells[i].state));
    painter.drawText(rect, Qt::AlignCenter, state_label(_cells[i].state));

    if (i == _hovered_cell && !_edit_mode) {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(QColor(255, 255, 255, 160), 2));
      painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 2, 2);
    }
  }

  // Edit mode border
  if (_edit_mode) {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(100, 180, 255, 180), 2));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), _corner_radius, _corner_radius);
  }
}

void Status_overlay::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (_edit_mode) {
    auto edges = edges_at(event->pos());
    if (edges != EDGE_NONE) {
      _resizing = true;
      _resize_edges = edges;
      _resize_origin = event->globalPos();
      _resize_start_geometry = geometry();
    }
    else {
      _dragging = true;
      _drag_offset = event->globalPos() - frameGeometry().topLeft();
    }
  }
  else {
    _press_global_pos = event->globalPos();
  }

  event->accept();
}

void Status_overlay::mouseMoveEvent(QMouseEvent* event) {
  if (_edit_mode) {
    if (_resizing) {
      auto delta = event->globalPos() - _resize_origin;
      auto geo = _resize_start_geometry;
      int min_w = _cell_size + 2 * _padding;
      int min_h = _cell_size + 2 * _padding;

      if (_resize_edges & EDGE_RIGHT)
        geo.setRight(qMax(geo.right() + delta.x(), geo.left() + min_w - 1));
      if (_resize_edges & EDGE_BOTTOM)
        geo.setBottom(qMax(geo.bottom() + delta.y(), geo.top() + min_h - 1));
      if (_resize_edges & EDGE_LEFT)
        geo.setLeft(qMin(geo.left() + delta.x(), geo.right() - min_w + 1));
      if (_resize_edges & EDGE_TOP)
        geo.setTop(qMin(geo.top() + delta.y(), geo.bottom() - min_h + 1));

      setGeometry(geo);
      _columns = columns_for_width(geo.width());
      event->accept();
      update();
    }
    else if (_dragging) {
      move(event->globalPos() - _drag_offset);
      event->accept();
    }
    else {
      setCursor(cursor_for_edges(edges_at(event->pos())));
    }
  }
  else {
    auto cell = cell_at(event->pos());
    if (cell != _hovered_cell) {
      _hovered_cell = cell;
      update();
    }
  }
}

void Status_overlay::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (_edit_mode) {
    if (_resizing) {
      _resizing = false;
      _columns = columns_for_width(width());
    }
    else if (_dragging) {
      _dragging = false;
    }
  }
  else {
    // Normal mode — click to switch desktop
    auto release_pos = event->globalPos();
    if ((release_pos - _press_global_pos).manhattanLength() < 5) {
      auto cell_index = cell_at(event->pos());
      if (cell_index >= 0)
        _desktop_monitor.switch_to_desktop(cell_index);
    }
  }

  event->accept();
}

void Status_overlay::leaveEvent(QEvent*) {
  if (_hovered_cell >= 0) {
    _hovered_cell = -1;
    update();
  }
}

void Status_overlay::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu;
  auto* action = menu.addAction(_edit_mode ? "Lock layout" : "Edit layout");
  connect(action, &QAction::triggered, this, &Status_overlay::toggle_edit_mode);
  menu.exec(event->globalPos());
}

bool Status_overlay::event(QEvent* event) {
  if (event->type() == QEvent::ToolTip) {
    auto* help_event = static_cast< QHelpEvent*>(event);
    auto index = cell_at(help_event->pos());
    if (index >= 0) {
      QToolTip::showText(help_event->globalPos(), tooltip_text(_cells[index]), this);
    }
    else {
      QToolTip::hideText();
    }
    return true;
  }
  return QWidget::event(event);
}
