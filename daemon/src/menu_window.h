#pragma once

#include "workspace_menu.h"

#include <QAbstractNativeEventFilter>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

class Menu_window
  : public QWidget
  , public QAbstractNativeEventFilter
{
  Q_OBJECT

 public:
  explicit Menu_window(QWidget* parent = nullptr);

  void activate(qint64 client_timestamp_ms = 0);
  void cancel_session();

 signals:
  void session_finished(const QString& response);

 protected:
  bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) override;
  bool eventFilter(QObject* obj, QEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  void finish_session(const QString& response);
  void rebuild_list();
  void update_selection();
  void on_filter_changed(const QString& text);

  Workspace_menu _menu;

  QLineEdit* _filter_input;
  QLabel* _message_label;
  QListWidget* _list_widget;
  bool _shown = false;
  qint64 _client_timestamp_ms = 0;

  static constexpr int _padding = 12;
  static constexpr int _max_visible_items = 15;
  static constexpr int _item_height = 34;
  static constexpr int _header_height = 38;
};
