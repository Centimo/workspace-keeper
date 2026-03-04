#pragma once

#include "claude_types.h"

#include <QPoint>
#include <QSize>
#include <QVariantList>
#include <QVariantMap>
#include <QWidget>

class Desktop_monitor;
class Workspace_db;

/// Frameless sticky overlay widget showing per-workspace Claude Code status
/// as a grid of colored squares. Has two modes:
/// - Normal: click to switch desktop, tooltip on hover. No drag/resize.
/// - Edit: draggable, resizable by edges. Right-click context menu to toggle.
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
  void toggle_edit_mode();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
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

  enum class Resize_edge {
    NONE,
    RIGHT,
    BOTTOM,
    BOTTOM_RIGHT
  };

  void update_cells();
  void apply_x11_sticky();
  void save_geometry();
  void restore_geometry();
  int columns_for_width(int width) const;
  QSize size_for_columns(int columns) const;
  QRect cell_rect(int index) const;
  int cell_at(const QPoint& pos) const;
  QString tooltip_text(const Cell_info& cell) const;
  Resize_edge edge_at(const QPoint& pos) const;
  Qt::CursorShape cursor_for_edge(Resize_edge edge) const;

  static QColor state_color(Claude_state state);
  static QColor state_text_color(Claude_state state);
  static QString state_label(Claude_state state);

  Desktop_monitor& _desktop_monitor;
  Workspace_db& _db;

  QVector< Cell_info> _cells;
  QVariantMap _claude_statuses;

  bool _edit_mode = false;

  // Drag state (edit mode only)
  bool _dragging = false;
  QPoint _drag_offset;

  // Resize state (edit mode only)
  bool _resizing = false;
  Resize_edge _resize_edge = Resize_edge::NONE;
  QPoint _resize_origin;
  QSize _resize_start_size;

  // Click detection (normal mode)
  QPoint _press_global_pos;

  int _columns = 1;
  static constexpr int _cell_size = 24;
  static constexpr int _spacing = 2;
  static constexpr int _padding = 2;
  static constexpr int _corner_radius = 4;
  static constexpr int _edge_margin = 6;
};
