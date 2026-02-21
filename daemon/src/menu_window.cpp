#include "menu_window.h"

#include <QApplication>
#include <QDateTime>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

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

Menu_window::Menu_window(QWidget* parent)
  : QWidget(parent)
{
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

  // Input field with horizontal padding
  _filter_input = new QLineEdit(background);
  _filter_input->setPlaceholderText("type to filter...");
  _filter_input->setFixedHeight(42);
  _filter_input->installEventFilter(this);

  auto* input_wrapper = new QWidget(background);
  auto* input_layout = new QVBoxLayout(input_wrapper);
  input_layout->setContentsMargins(_padding, 0, _padding, 0);
  input_layout->setSpacing(0);
  input_layout->addWidget(_filter_input);
  main_layout->addWidget(input_wrapper);

  // Message bar
  _message_label = new QLabel("Enter: switch/create  Tab: complete path  Alt+Del: close", background);
  _message_label->setFixedHeight(26);
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
}

void Menu_window::activate(qint64 client_timestamp_ms) {
  _shown = false;
  _client_timestamp_ms = client_timestamp_ms;

  _menu.begin_session();
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

void Menu_window::finish_session(const QString& response) {
  _shown = false;
  hide();
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
    if (key_event->key() == Qt::Key_Up) {
      _menu.model()->navigate(-1);
      return true;
    }
    if (key_event->key() == Qt::Key_Down) {
      _menu.model()->navigate(1);
      return true;
    }
    if (key_event->key() == Qt::Key_Delete && (key_event->modifiers() & Qt::AltModifier)) {
      auto response = _menu.close_current();
      if (!response.isEmpty()) {
        finish_session(response);
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

bool Menu_window::nativeEventFilter(
  const QByteArray& event_type, void* message, qintptr*
) {
  if (_client_timestamp_ms <= 0 || event_type != "xcb_generic_event_t") {
    return false;
  }

  auto* xcb_event = static_cast< xcb_generic_event_t*>(message);
  if ((xcb_event->response_type & ~0x80) == XCB_MAP_NOTIFY) {
    auto* map_event = reinterpret_cast< xcb_map_notify_event_t*>(xcb_event);
    if (windowHandle() && map_event->window == static_cast< xcb_window_t>(windowHandle()->winId())) {
      auto elapsed_ms = QDateTime::currentMSecsSinceEpoch() - _client_timestamp_ms;
      qInfo("workspace-menu: startup latency %lld ms", elapsed_ms);
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
  }
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

  int total_height = _padding * 2 + 42 + 26 + list_height + 2;
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
