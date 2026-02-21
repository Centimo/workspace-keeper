#include "workspace_menu.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

Workspace_menu::Workspace_menu(QObject* parent)
  : QObject(parent)
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

  // Read active desktops from wmctrl -d
  QProcess wmctrl;
  wmctrl.start("wmctrl", {"-d"});

  QSet< QString> active_names;
  if (wmctrl.waitForFinished(2000) && wmctrl.exitCode() == 0) {
    auto output = QString::fromUtf8(wmctrl.readAllStandardOutput());
    auto lines = output.split('\n', Qt::SkipEmptyParts);

    // Parse: extract desktop name (everything after geometry like "1920x1080")
    struct Desktop_info {
      int index;
      QString name;
    };
    QVector< Desktop_info> desktops;

    static const QRegularExpression whitespace_re("\\s+");
    static const QRegularExpression geometry_re("^\\d+x\\d+$");

    for (const auto& line : lines) {
      auto parts = line.split(whitespace_re, Qt::SkipEmptyParts);
      // Find the geometry field (NxN pattern), name is everything after it
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
        desktops.append({index, name});
      }
    }

    // Sort by index
    std::sort(desktops.begin(), desktops.end(), [](const auto& a, const auto& b) {
      return a.index < b.index;
    });

    for (const auto& d : desktops) {
      active_names.insert(d.name);

      // Look up project_dir from workspace config
      QString project_dir;
      QString ws_config = _workspace_dir + '/' + d.name + "/project_dir";
      QFile file(ws_config);
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        project_dir = QTextStream(&file).readLine().trimmed();
      }

      _active_desktops.append({d.name, project_dir});
    }
  }

  // Read saved workspaces from filesystem
  QDir ws_dir(_workspace_dir);
  if (ws_dir.exists()) {
    auto entries = ws_dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& entry : entries) {
      QString name = entry.fileName();
      if (active_names.contains(name)) {
        continue;
      }

      QString project_dir;
      QFile file(entry.filePath() + "/project_dir");
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        project_dir = QTextStream(&file).readLine().trimmed();
      }

      _saved_workspaces.append({name, project_dir});
    }
  }
}

void Workspace_menu::rebuild_model() {
  _model.rebuild(_filter_text, _active_desktops, _saved_workspaces, _filter_text);
}

