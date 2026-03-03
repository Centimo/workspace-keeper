#pragma once

#include "claude_types.h"

#include <QPoint>
#include <QVariantList>
#include <QVariantMap>
#include <QWidget>

class Desktop_monitor;
class Workspace_db;

/// Frameless sticky overlay widget showing per-workspace Claude Code status
/// as a vertical column of colored squares. Draggable, always-on-top, visible
/// on all virtual desktops.
class Status_overlay : public QWidget {
  Q_OBJECT

 public:
  Status_overlay(
    Desktop_monitor& desktop_monitor,
    Workspace_db& db,
    QWidget* parent = nullptr
  );

  /// Update status for a single workspace.
  void on_status_changed(
    const QString& workspace,
    Claude_state state,
    const QString& tool_name,
    const QString& wait_reason,
    const QString& wait_message,
    qint64 state_since_ms
  );

 private slots:
  void on_desktops_changed();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  bool event(QEvent* event) override;

 private:
  struct Cell_info {
    QString workspace_name;
    Claude_state state = Claude_state::NOT_RUNNING;
    QString tool_name;
    QString wait_reason;
    QString wait_message;
    qint64 state_since_ms = 0;
  };

  void update_cells();
  void resize_to_fit();
  void apply_x11_sticky();
  void save_position();
  void restore_position();
  QRect cell_rect(int index) const;
  int cell_at(const QPoint& pos) const;
  QString tooltip_text(const Cell_info& cell) const;

  static QColor state_color(Claude_state state);
  static QColor state_text_color(Claude_state state);
  static QString state_label(Claude_state state);

  Desktop_monitor& _desktop_monitor;
  Workspace_db& _db;

  QVector< Cell_info> _cells;
  QVariantMap _claude_statuses;

  // Drag state
  bool _dragging = false;
  QPoint _drag_offset;
  QPoint _press_global_pos;

  void save_cell_size();
  void restore_cell_size();

  int _cell_size = _default_cell_size;
  static constexpr int _default_cell_size = 24;
  static constexpr int _min_cell_size = 10;
  static constexpr int _max_cell_size = 64;
  static constexpr int _spacing = 2;
  static constexpr int _padding = 2;
  static constexpr int _corner_radius = 4;
};
