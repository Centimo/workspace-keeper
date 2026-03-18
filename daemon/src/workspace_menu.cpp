#include "workspace_menu.h"
#include "desktop_monitor.h"
#include "journal_log.h"
#include "workspace_db.h"

Workspace_menu::Workspace_menu(Workspace_db& db, Desktop_monitor& desktop_monitor, QObject* parent)
  : QObject(parent)
  , _db(db)
  , _desktop_monitor(desktop_monitor)
{}

void Workspace_menu::begin_session() {
  _filter_text.clear();
  load_data();
  rebuild_model();
}

QString Workspace_menu::filter_text() const {
  return _filter_text;
}

void Workspace_menu::set_filter_text(const QString& text) {
  if (_filter_text == text) {
    return;
  }
  _filter_text = text;
  emit filter_text_changed();
  rebuild_model();
}

Workspace_model* Workspace_menu::model() {
  return &_model;
}

QString Workspace_menu::select_current() {
  const auto* entry = _model.selected_entry();
  if (entry) {
    return "select " + entry->data;
  }
  if (!_filter_text.isEmpty()) {
    return "custom_input " + _filter_text;
  }
  return {};
}

QString Workspace_menu::close_current() {
  const auto* entry = _model.selected_entry();
  if (entry && entry->type == Entry_type::WORKSPACE) {
    if (entry->is_active) {
      return "close " + entry->data;
    }
    else {
      return "delete_saved " + entry->data;
    }
  }
  return {};
}

void Workspace_menu::move_current(int direction) {
  auto [name_a, name_b] = _model.move_selected(direction);
  if (name_a.isEmpty()) {
    return;
  }

  // Persist new order to database
  _db.swap_desktop_order(name_a, name_b);

  // Swap positions in _active_desktops to keep in sync
  int idx_a = -1, idx_b = -1;
  for (int i = 0; i < _active_desktops.size(); ++i) {
    if (_active_desktops[i].first == name_a) {
      idx_a = i;
    }
    if (_active_desktops[i].first == name_b) {
      idx_b = i;
    }
  }
  if (idx_a >= 0 && idx_b >= 0) {
    std::swap(_active_desktops[idx_a], _active_desktops[idx_b]);
  }
}

QString Workspace_menu::tab_complete() {
  const auto* entry = _model.selected_entry();

  // If selected entry has a path as data, insert it into input
  if (entry && !entry->data.isEmpty() && entry->data.startsWith('/')) {
    return entry->data;
  }

  // Otherwise try filesystem completion on current input
  if (_filter_text.startsWith('/')) {
    return Workspace_model::compute_tab_completion(_filter_text);
  }

  return _filter_text;
}

void Workspace_menu::load_data() {
  _active_desktops.clear();
  _saved_workspaces.clear();

  auto active = _db.active_desktops();
  for (const auto& ws : active) {
    _active_desktops.append({ws.name, ws.project_dir});
  }

  auto saved = _db.saved_workspaces();
  for (const auto& ws : saved) {
    _saved_workspaces.append({ws.name, ws.project_dir});
  }
}

void Workspace_menu::rebuild_model() {
  _model.rebuild(_filter_text, _active_desktops, _saved_workspaces, _filter_text, _desktop_monitor.current_desktop_name());
}
