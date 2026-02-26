#include "workspace_menu.h"
#include "workspace_db.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>

Workspace_menu::Workspace_menu(Workspace_db& db, QObject* parent)
  : QObject(parent)
  , _db(db)
{
  _workspace_dir = qEnvironmentVariable("WORKSPACE_DIR");

  if (_workspace_dir.isEmpty()) {
    _workspace_dir = QDir::homePath() + "/.config/workspaces";
  }
}

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
    return "close " + entry->data;
  }
  return {};
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
  _current_desktop_name.clear();

  // Read active desktops from wmctrl -d
  QProcess wmctrl;
  wmctrl.start("wmctrl", {"-d"});

  QVector< Desktop_info> desktops;

  if (wmctrl.waitForFinished(2000) && wmctrl.exitCode() == 0) {
    auto output = QString::fromUtf8(wmctrl.readAllStandardOutput());
    auto lines = output.split('\n', Qt::SkipEmptyParts);

    static const QRegularExpression whitespace_re("\\s+");
    static const QRegularExpression geometry_re("^\\d+x\\d+$");

    for (const auto& line : lines) {
      auto parts = line.split(whitespace_re, Qt::SkipEmptyParts);
      bool is_current = parts.size() > 1 && parts[1] == "*";

      int geo_idx = -1;
      for (int i = 0; i < parts.size(); ++i) {
        if (parts[i].contains(geometry_re)) {
          geo_idx = i;
        }
      }
      if (geo_idx >= 0 && geo_idx + 1 < parts.size()) {
        QStringList name_parts;
        for (int i = geo_idx + 1; i < parts.size(); ++i) {
          name_parts.append(parts[i]);
        }
        QString name = name_parts.join(' ');
        int index = parts[0].toInt();
        desktops.append({index, name, is_current});

        if (is_current) {
          _current_desktop_name = name;
        }
      }
    }

    std::sort(desktops.begin(), desktops.end(), [](const auto& a, const auto& b) {
      return a.index < b.index;
    });
  }

  // Sync to database
  _db.sync_active_desktops(desktops, _workspace_dir);

  // Read back from database
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
  _model.rebuild(_filter_text, _active_desktops, _saved_workspaces, _filter_text, _current_desktop_name);
}
