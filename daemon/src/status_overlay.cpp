#include "status_overlay.h"
#include "desktop_monitor.h"
#include "enum_strings.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QToolTip>

#include <xcb/xcb.h>

namespace {

/// Set _NET_WM_STATE atoms on an X11 window (sticky, skip-taskbar, skip-pager).
void set_x11_sticky(WId window_id) {
  auto* connection = xcb_connect(nullptr, nullptr);
  if (!connection)
    return;

  // xcb_connect always returns non-null; must always disconnect.
  struct Connection_guard {
    xcb_connection_t* c;
    ~Connection_guard() { xcb_disconnect(c); }
  } guard{connection};

  if (xcb_connection_has_error(connection))
    return;

  auto intern = [&](const char* name) -> xcb_atom_t {
    auto cookie = xcb_intern_atom(connection, 0, static_cast< uint16_t>(strlen(name)), name);
    auto* reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!reply)
      return XCB_ATOM_NONE;
    auto atom = reply->atom;
    free(reply);
    return atom;
  };

  auto net_wm_state = intern("_NET_WM_STATE");
  auto sticky = intern("_NET_WM_STATE_STICKY");
  auto skip_taskbar = intern("_NET_WM_STATE_SKIP_TASKBAR");
  auto skip_pager = intern("_NET_WM_STATE_SKIP_PAGER");

  if (
    net_wm_state == XCB_ATOM_NONE
    || sticky == XCB_ATOM_NONE
    || skip_taskbar == XCB_ATOM_NONE
    || skip_pager == XCB_ATOM_NONE
  ) {
    qCWarning(logServer, "Status_overlay: failed to intern X11 atoms for sticky window");
    return;
  }

  xcb_atom_t atoms[] = {sticky, skip_taskbar, skip_pager};
  xcb_change_property(
    connection,
    XCB_PROP_MODE_REPLACE,
    static_cast< xcb_window_t>(window_id),
    net_wm_state,
    XCB_ATOM_ATOM,
    32,
    3,
    atoms
  );
  xcb_flush(connection);
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

  connect(
    &_desktop_monitor, &Desktop_monitor::desktops_changed,
    this, &Status_overlay::on_desktops_changed
  );

  restore_position();
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
  resize_to_fit();
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

void Status_overlay::resize_to_fit() {
  int count = qMax(_cells.size(), 1);
  int width = _cell_size + 2 * _padding;
  int height = count * _cell_size + (count - 1) * _spacing + 2 * _padding;
  setFixedSize(width, height);
}

void Status_overlay::apply_x11_sticky() {
  set_x11_sticky(winId());
}

void Status_overlay::save_position() {
  _db.set_meta("overlay_x", QString::number(x()));
  _db.set_meta("overlay_y", QString::number(y()));
}

void Status_overlay::restore_position() {
  auto sx = _db.get_meta("overlay_x");
  auto sy = _db.get_meta("overlay_y");

  if (!sx.isEmpty() && !sy.isEmpty()) {
    int px = sx.toInt();
    int py = sy.toInt();

    // Validate that saved position is within any available screen
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
    int default_width = _cell_size + 2 * _padding;
    move(geometry.right() - default_width - 20, geometry.top() + 20);
  }
}

QRect Status_overlay::cell_rect(int index) const {
  return QRect(
    _padding,
    _padding + index * (_cell_size + _spacing),
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
  painter.setBrush(QColor(30, 30, 30, 200));
  painter.drawRoundedRect(rect(), _corner_radius, _corner_radius);

  // Cells
  QFont font("Hack");
  font.setPixelSize(static_cast< int>(_cell_size * 0.5));
  font.setBold(true);
  painter.setFont(font);

  for (int i = 0; i < _cells.size(); ++i) {
    auto rect = cell_rect(i);
    auto color = state_color(_cells[i].state);

    // Cell background
    painter.setPen(QPen(color.lighter(130), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(rect, 3, 3);

    // Cell label
    painter.setPen(state_text_color(_cells[i].state));
    painter.drawText(rect, Qt::AlignCenter, state_label(_cells[i].state));
  }
}

void Status_overlay::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    _dragging = true;
    _drag_offset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    _press_global_pos = event->globalPosition().toPoint();
    event->accept();
  }
}

void Status_overlay::mouseMoveEvent(QMouseEvent* event) {
  if (_dragging) {
    move(event->globalPosition().toPoint() - _drag_offset);
    event->accept();
  }
}

void Status_overlay::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && _dragging) {
    _dragging = false;

    auto release_pos = event->globalPosition().toPoint();
    auto distance = (release_pos - _press_global_pos).manhattanLength();

    if (distance < 5) {
      // Click — switch desktop
      auto cell_index = cell_at(event->pos());
      if (cell_index >= 0)
        _desktop_monitor.switch_to_desktop(cell_index);
    }
    else {
      // Drag — save new position
      save_position();
    }

    event->accept();
  }
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
