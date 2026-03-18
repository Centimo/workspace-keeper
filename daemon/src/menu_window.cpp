#include "menu_window.h"
#include "claude_status_tracker.h"
#include "desktop_monitor.h"
#include "journal_log.h"
#include "workspace_db.h"

#include <claude_types.h>

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWindow>

#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>

#include <xcb/xcb.h>

static const QString style_sheet = R"(
  QWidget#background {
    background-color: #232627;
    border: 1px solid #3daee9;
    border-radius: 6px;
  }
  QLineEdit {
    background-color: #2a2e32;
    color: #fcfcfc;
    border: 1px solid #3daee9;
    border-radius: 4px;
    padding-left: 8px;
    padding-right: 8px;
    font-family: Hack;
    font-size: 18px;
  }
  QListWidget {
    background-color: transparent;
    border: none;
    outline: none;
  }
  QListWidget::item {
    padding: 0px;
    border: none;
  }
  QListWidget::item:selected {
    background-color: transparent;
  }
)";

// px from left edge of background widget
static constexpr int text_x = 21;     // 12 (input wrapper margin) + 1 (border) + 8 (padding)
static constexpr int header_x = 6;

static QString item_style(const QString& color) {
  return QString(
    "color: %1; background: transparent; font-family: Hack;"
    "font-size: 18px; padding-left: %2px;"
  ).arg(color).arg(text_x - 1);  // -1 for list widget 1px margin
}

static QString selected_item_style() {
  return QString(
    "color: #1a1a1a; background: #1d99f3; font-family: Hack;"
    "font-size: 18px; padding-left: %1px; border-radius: 3px;"
  ).arg(text_x - 1);
}

static QString header_style() {
  return QString(
    "color: #7f8c8d; background: #1e2123; font-family: Hack;"
    "font-size: 20px; font-weight: bold; padding-left: %1px;"
  ).arg(header_x - 1);  // -1 for list widget 1px margin
}

Menu_window::Menu_window(
  Workspace_db& db,
  Desktop_monitor& desktop_monitor,
  Claude_status_tracker& claude_tracker,
  QWidget* parent
)
  : QWidget(parent)
  , _menu(db, desktop_monitor)
  , _db(db)
{
  Q_UNUSED(claude_tracker)
  QCoreApplication::instance()->installNativeEventFilter(this);

  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Popup);
  setAttribute(Qt::WA_TranslucentBackground);
  setFixedWidth(600);

  auto* background = new QWidget(this);
  background->setObjectName("background");
  background->setStyleSheet(style_sheet);

  auto* outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);
  outer_layout->addWidget(background);

  auto* main_layout = new QVBoxLayout(background);
  main_layout->setContentsMargins(0, _padding, 0, _padding);
  main_layout->setSpacing(0);

  // Dashboard: scrollable area with tab status grid
  _dashboard_widget = new QWidget();
  _dashboard_widget->setStyleSheet("background: transparent;");

  _dashboard_scroll = new QScrollArea(background);
  _dashboard_scroll->setWidget(_dashboard_widget);
  _dashboard_scroll->setWidgetResizable(true);
  _dashboard_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _dashboard_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _dashboard_scroll->setFrameShape(QFrame::NoFrame);
  _dashboard_scroll->setStyleSheet("background: transparent;");
  _dashboard_scroll->hide();
  main_layout->addWidget(_dashboard_scroll);

  // Input field with horizontal padding
  _filter_input = new QLineEdit(background);
  _filter_input->setPlaceholderText("type to filter...");
  _filter_input->setFixedHeight(_input_height);
  _filter_input->installEventFilter(this);

  auto* input_wrapper = new QWidget(background);
  auto* input_layout = new QVBoxLayout(input_wrapper);
  input_layout->setContentsMargins(_padding, 0, _padding, 0);
  input_layout->setSpacing(0);
  input_layout->addWidget(_filter_input);
  main_layout->addWidget(input_wrapper);

  // Message bar
  _message_label = new QLabel("Enter: switch/create  Tab: complete path  Alt+Del: close/delete", background);
  _message_label->setFixedHeight(_message_bar_height);
  _message_label->setAlignment(Qt::AlignCenter);
  _message_label->setStyleSheet("color: #7f8c8d; font-family: Hack; font-size: 13px;");
  main_layout->addWidget(_message_label);

  // List — 1px horizontal margin to not cover border
  _list_widget = new QListWidget(background);
  _list_widget->setFocusPolicy(Qt::NoFocus);
  _list_widget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _list_widget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _list_widget->setSelectionMode(QAbstractItemView::SingleSelection);

  auto* list_wrapper = new QWidget(background);
  auto* list_layout = new QVBoxLayout(list_wrapper);
  list_layout->setContentsMargins(1, 0, 1, 0);
  list_layout->setSpacing(0);
  list_layout->addWidget(_list_widget);
  main_layout->addWidget(list_wrapper);

  connect(_filter_input, &QLineEdit::textChanged, this, &Menu_window::on_filter_changed);
  connect(_menu.model(), &QAbstractItemModel::modelReset, this, &Menu_window::rebuild_list);
  connect(_menu.model(), &Workspace_model::selected_index_changed, this, &Menu_window::update_selection);

  _delete_confirm_timer.setSingleShot(true);
  connect(&_delete_confirm_timer, &QTimer::timeout, this, &Menu_window::cancel_delete_confirm);

  _delete_confirm_tick.setInterval(500);
  connect(&_delete_confirm_tick, &QTimer::timeout, this, [this]() {
    _delete_confirm_remaining_ms -= 500;
    auto name = _pending_delete_response.mid(QString("delete_saved ").length());
    int secs = (_delete_confirm_remaining_ms + 999) / 1000;
    _message_label->setText(
      QString("Delete \"%1\"? Alt+Del to confirm (%2s)  Esc/any key to cancel").arg(name).arg(secs)
    );
    _message_label->setStyleSheet("color: #ed1515; font-family: Hack; font-size: 13px;");
  });
}

void Menu_window::activate(qint64 client_timestamp_ms) {
  if (isVisible()) {
    qCWarning(logWindow, "activate() called while already visible — ignoring");
    return;
  }

  _shown = false;
  _client_timestamp_ms = client_timestamp_ms;
  _saved_keyboard_layout = -1;

  {
    QElapsedTimer timer;
    timer.start();
    QDBusInterface keyboard("org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts");
    QDBusReply< uint> current_layout = keyboard.call("getLayout");
    if (current_layout.isValid() && current_layout.value() != 0) {
      _saved_keyboard_layout = current_layout.value();
      keyboard.call("setLayout", 0u);
    }
    auto elapsed = timer.elapsed();
    if (elapsed > 50) {
      qCWarning(logWindow, "keyboard layout switch took %lld ms (blocking event loop)", elapsed);
    }
  }

  _menu.begin_session();
  rebuild_dashboard();
  rebuild_list();

  if (auto* screen = QApplication::primaryScreen()) {
    auto geometry = screen->geometry();
    int x = (geometry.width() - width()) / 2 + geometry.x();
    int y = (geometry.height() - height()) / 3 + geometry.y();
    move(x, y);
  }

  _filter_input->clear();
  _filter_input->setFocus();

  show();
  raise();
  activateWindow();
}

void Menu_window::show_delete_confirm(const QString& response) {
  static constexpr int _confirm_timeout_ms = 3000;
  _pending_delete_response = response;
  _delete_confirm_remaining_ms = _confirm_timeout_ms;
  _delete_confirm_timer.start(_confirm_timeout_ms);
  _delete_confirm_tick.start();

  auto name = response.mid(QString("delete_saved ").length());
  int secs = _confirm_timeout_ms / 1000;
  _message_label->setText(
    QString("Delete \"%1\"? Alt+Del to confirm (%2s)  Esc/any key to cancel").arg(name).arg(secs)
  );
  _message_label->setStyleSheet("color: #ed1515; font-family: Hack; font-size: 13px;");
}

void Menu_window::cancel_delete_confirm() {
  if (_pending_delete_response.isEmpty()) {
    return;
  }
  _pending_delete_response.clear();
  _delete_confirm_timer.stop();
  _delete_confirm_tick.stop();
  _message_label->setText("Enter: switch/create  Tab: complete path  Alt+Del: close/delete");
  _message_label->setStyleSheet("color: #7f8c8d; font-family: Hack; font-size: 13px;");
}

void Menu_window::finish_session(const QString& response) {
  cancel_delete_confirm();
  _shown = false;
  hide();

  if (_saved_keyboard_layout >= 0) {
    QElapsedTimer timer;
    timer.start();
    QDBusInterface keyboard("org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts");
    keyboard.call("setLayout", static_cast< uint>(_saved_keyboard_layout));
    _saved_keyboard_layout = -1;
    auto elapsed = timer.elapsed();
    if (elapsed > 50) {
      qCWarning(logWindow, "keyboard layout restore took %lld ms (blocking event loop)", elapsed);
    }
  }

  emit session_finished(response);
}

void Menu_window::cancel_session() {
  if (isVisible()) {
    finish_session("cancelled");
  }
}

bool Menu_window::eventFilter(QObject* obj, QEvent* event) {
  if (obj == _filter_input && event->type() == QEvent::KeyPress) {
    auto* key_event = static_cast< QKeyEvent*>(event);

    // Any key except Alt+Del cancels a pending delete confirmation
    bool is_alt_del = key_event->key() == Qt::Key_Delete
      && (key_event->modifiers() & Qt::AltModifier);
    if (!is_alt_del && !_pending_delete_response.isEmpty()) {
      cancel_delete_confirm();
    }

    if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
      auto response = _menu.select_current();
      if (!response.isEmpty()) {
        finish_session(response);
      }
      return true;
    }
    if (key_event->key() == Qt::Key_Tab) {
      auto completed = _menu.tab_complete();
      _filter_input->setText(completed);
      return true;
    }
    if (key_event->key() == Qt::Key_Escape) {
      finish_session("cancelled");
      return true;
    }
    if (key_event->key() == Qt::Key_Up && (key_event->modifiers() & Qt::ShiftModifier)) {
      _menu.move_current(-1);
      return true;
    }
    if (key_event->key() == Qt::Key_Down && (key_event->modifiers() & Qt::ShiftModifier)) {
      _menu.move_current(1);
      return true;
    }
    if (key_event->key() == Qt::Key_Up) {
      _menu.model()->navigate(-1);
      return true;
    }
    if (key_event->key() == Qt::Key_Down) {
      _menu.model()->navigate(1);
      return true;
    }
    if (key_event->key() == Qt::Key_Delete && (key_event->modifiers() & Qt::AltModifier)) {
      if (!_pending_delete_response.isEmpty()) {
        // Second Alt+Del within confirm window — execute
        auto response = _pending_delete_response;
        cancel_delete_confirm();
        finish_session(response);
      }
      else {
        auto response = _menu.close_current();
        if (response.startsWith("delete_saved ")) {
          show_delete_confirm(response);
        }
        else if (!response.isEmpty()) {
          finish_session(response);
        }
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

bool Menu_window::nativeEventFilter(
  const QByteArray& event_type, void* message, long*
) {
  if (_client_timestamp_ms <= 0 || event_type != "xcb_generic_event_t") {
    return false;
  }

  auto* xcb_event = static_cast< xcb_generic_event_t*>(message);
  if ((xcb_event->response_type & ~0x80) == XCB_MAP_NOTIFY) {
    auto* map_event = reinterpret_cast< xcb_map_notify_event_t*>(xcb_event);
    if (windowHandle() && map_event->window == static_cast< xcb_window_t>(windowHandle()->winId())) {
      auto elapsed_ms = QDateTime::currentMSecsSinceEpoch() - _client_timestamp_ms;
      qCInfo(logWindow, "startup latency %lld ms", elapsed_ms);
      _client_timestamp_ms = 0;
    }
  }

  return false;
}

void Menu_window::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::ActivationChange) {
    if (isActiveWindow()) {
      _shown = true;
    }
    else if (_shown) {
      finish_session("cancelled");
    }
    else {
      qCInfo(logWindow, "ActivationChange: not active, _shown=false — ignoring");
    }
  }
}

bool Menu_window::event(QEvent* event) {
  // Guard: if window is hidden without ever becoming active (e.g. WM refused focus),
  // finish the session so _shortcut_session doesn't get stuck.
  if (event->type() == QEvent::Hide && !_shown) {
    qCWarning(logWindow, "Hide event while _shown=false — finishing session via queued call");
    QMetaObject::invokeMethod(this, [this]() {
      if (!isVisible() && !_shown) {
        finish_session("cancelled");
      }
    }, Qt::QueuedConnection);
  }
  return QWidget::event(event);
}

void Menu_window::rebuild_list() {
  _list_widget->clear();

  auto* model = _menu.model();
  int count = model->rowCount();

  for (int i = 0; i < count; ++i) {
    auto index = model->index(i);
    auto display_text = model->data(index, Workspace_model::DISPLAY_TEXT).toString();
    auto entry_type = static_cast< Entry_type>(model->data(index, Workspace_model::ENTRY_TYPE).toInt());
    auto is_active = model->data(index, Workspace_model::IS_ACTIVE).toBool();

    auto* item = new QListWidgetItem(_list_widget);
    int height = (entry_type == Entry_type::SECTION_HEADER) ? _header_height : _item_height;
    item->setSizeHint(QSize(0, height));

    auto* label = new QLabel(display_text, _list_widget);
    label->setFixedHeight(height);

    if (entry_type == Entry_type::SECTION_HEADER) {
      item->setFlags(Qt::NoItemFlags);
      label->setStyleSheet(header_style());
    }
    else {
      QString color = is_active ? "#1d99f3" : "#fcfcfc";
      label->setStyleSheet(item_style(color));
      label->setProperty("base_text", display_text);
      label->setProperty("base_color", color);
      label->setProperty("is_selectable", true);
    }

    _list_widget->setItemWidget(item, label);
  }

  update_selection();

  int list_height = 0;
  int visible_count = 0;
  for (int i = 0; i < _list_widget->count() && visible_count < _max_visible_items; ++i) {
    list_height += _list_widget->sizeHintForRow(i);
    ++visible_count;
  }
  _list_widget->setFixedHeight(list_height);

  int dashboard_height = _dashboard_scroll->isVisible() ? _dashboard_scroll->height() : 0;
  int total_height = _padding * 2 + dashboard_height + _input_height + _message_bar_height + list_height + _border_width * 2;
  setFixedHeight(total_height);
}

void Menu_window::update_selection() {
  int selected = _menu.model()->selected_index();

  for (int i = 0; i < _list_widget->count(); ++i) {
    auto* label = qobject_cast< QLabel*>(_list_widget->itemWidget(_list_widget->item(i)));
    if (!label || !label->property("is_selectable").toBool()) {
      continue;
    }

    auto base_text = label->property("base_text").toString();
    if (i == selected) {
      label->setText(" " + base_text);
      label->setStyleSheet(selected_item_style());
    }
    else {
      label->setText(base_text);
      QString color = label->property("base_color").toString();
      label->setStyleSheet(item_style(color));
    }
  }

  if (selected >= 0 && selected < _list_widget->count()) {
    _list_widget->setCurrentRow(selected);
    _list_widget->scrollToItem(_list_widget->item(selected), QAbstractItemView::EnsureVisible);
  }
  else {
    _list_widget->setCurrentRow(-1);
  }
}

void Menu_window::on_filter_changed(const QString& text) {
  _menu.set_filter_text(text);
}

void Menu_window::on_tab_status_changed(
  const QString& workspace,
  int pane_id,
  Claude_state state,
  const QString& tool_name,
  const QString& wait_reason,
  const QString& wait_message,
  qint64 state_since_ms
) {
  Q_UNUSED(pane_id) Q_UNUSED(tool_name) Q_UNUSED(wait_reason)
  Q_UNUSED(wait_message) Q_UNUSED(state_since_ms) Q_UNUSED(state)
  Q_UNUSED(workspace)
  if (isVisible()) {
    rebuild_dashboard();
  }
}

void Menu_window::rebuild_dashboard() {
  // Load data from DB
  auto wezterm_tabs_json = _db.all_wezterm_tabs();
  auto claude_statuses = _db.all_claude_tab_statuses();

  // Build lookup: (workspace_name, pane_id) -> Claude_tab_status
  QHash< QString, QHash< int, Claude_tab_status>> status_map;
  for (const auto& s : claude_statuses) {
    status_map[s.workspace_name][s.pane_id] = s;
  }

  // Build lookup: workspace_name -> [tab_info]
  struct Tab_info {
    int pane_id;
    int tab_index;
    QString cwd;
  };
  QHash< QString, QVector< Tab_info>> tabs_map;
  for (int i = 0; i < wezterm_tabs_json.size(); ++i) {
    auto obj = wezterm_tabs_json[i].toObject();
    auto ws = obj["workspace_name"].toString();
    tabs_map[ws].append({
      obj["pane_id"].toInt(),
      obj["tab_index"].toInt(),
      obj["cwd"].toString()
    });
  }

  // Sort each workspace's tabs by tab_index
  for (auto& tabs : tabs_map) {
    std::sort(tabs.begin(), tabs.end(), [](const Tab_info& a, const Tab_info& b) {
      return a.tab_index < b.tab_index;
    });
  }

  // Collect active workspaces in order from DB
  auto active_workspaces = _db.active_desktops();

  // Rebuild dashboard widget
  delete _dashboard_widget->layout();
  // Remove all child widgets
  for (auto* child : _dashboard_widget->findChildren< QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
    delete child;
  }

  if (active_workspaces.isEmpty() || tabs_map.isEmpty()) {
    _dashboard_scroll->hide();
    return;
  }

  auto* row_layout = new QHBoxLayout(_dashboard_widget);
  row_layout->setContentsMargins(_padding, _padding / 2, _padding, _padding / 2);
  row_layout->setSpacing(12);

  for (const auto& ws_info : active_workspaces) {
    auto tabs_it = tabs_map.find(ws_info.name);
    if (tabs_it == tabs_map.end() || tabs_it->isEmpty()) {
      continue;
    }

    auto* col = new QWidget(_dashboard_widget);
    auto* col_layout = new QVBoxLayout(col);
    col_layout->setContentsMargins(0, 0, 0, 0);
    col_layout->setSpacing(2);

    // Workspace name header
    auto* header = new QLabel(ws_info.name, col);
    header->setStyleSheet("color: #7f8c8d; font-family: Hack; font-size: 12px; font-weight: bold;");
    col_layout->addWidget(header);

    // One row per tab
    for (const auto& tab : *tabs_it) {
      auto* tab_row = new QWidget(col);
      auto* tab_layout = new QHBoxLayout(tab_row);
      tab_layout->setContentsMargins(0, 0, 0, 0);
      tab_layout->setSpacing(6);

      // CWD: last path component
      QString cwd_display = tab.cwd.isEmpty() ? "~" : QFileInfo(tab.cwd).fileName();
      if (cwd_display.isEmpty()) {
        cwd_display = tab.cwd;
      }

      auto* cwd_label = new QLabel(cwd_display, tab_row);
      cwd_label->setStyleSheet("color: #fcfcfc; font-family: Hack; font-size: 13px;");
      cwd_label->setMinimumWidth(80);
      tab_layout->addWidget(cwd_label);

      // Status indicator
      Claude_state tab_state = Claude_state::NOT_RUNNING;
      auto ws_status_it = status_map.find(ws_info.name);
      if (ws_status_it != status_map.end()) {
        auto pane_it = ws_status_it->find(tab.pane_id);
        if (pane_it != ws_status_it->end()) {
          tab_state = pane_it->state;
        }
      }

      auto* state_indicator = new QLabel(QString(state_label(tab_state)), tab_row);
      state_indicator->setFixedSize(18, 18);
      state_indicator->setAlignment(Qt::AlignCenter);
      state_indicator->setStyleSheet(QString(
        "background: %1; color: %2; font-family: Hack; font-size: 11px;"
        "font-weight: bold; border-radius: 3px;"
      ).arg(state_color_hex(tab_state)).arg(state_text_color_hex(tab_state)));
      tab_layout->addWidget(state_indicator);

      col_layout->addWidget(tab_row);
    }

    col_layout->addStretch();
    row_layout->addWidget(col);
  }

  row_layout->addStretch();

  // Compute height from content without triggering event processing:
  // header row (font ~14px + spacing) + each tab row (font ~15px + spacing) + padding
  static constexpr int _row_height = 20;
  static constexpr int _header_row_height = 18;
  int max_tabs = 0;
  for (const auto& ws_info : active_workspaces) {
    auto it = tabs_map.find(ws_info.name);
    if (it != tabs_map.end()) {
      max_tabs = std::max(max_tabs, static_cast< int>(it->size()));
    }
  }
  int dashboard_height = _padding + _header_row_height + max_tabs * _row_height + _padding / 2;
  _dashboard_scroll->setFixedHeight(dashboard_height);
  _dashboard_scroll->show();
}
