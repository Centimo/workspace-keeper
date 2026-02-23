#include "workspace_model.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

Workspace_model::Workspace_model(QObject* parent)
  : QAbstractListModel(parent)
{}

int Workspace_model::rowCount(const QModelIndex&) const {
  return _entries.size();
}

QVariant Workspace_model::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= _entries.size()) {
    return {};
  }

  const auto& entry = _entries[index.row()];

  switch (role) {
    case DISPLAY_TEXT: return entry.display_text;
    case DATA: return entry.data;
    case ENTRY_TYPE: return static_cast< int>(entry.type);
    case IS_ACTIVE: return entry.is_active;
    default: return {};
  }
}

QHash< int, QByteArray> Workspace_model::roleNames() const {
  return {
    {DISPLAY_TEXT, "display_text"},
    {DATA, "data"},
    {ENTRY_TYPE, "entry_type"},
    {IS_ACTIVE, "is_active"},
  };
}

void Workspace_model::rebuild(
  const QString& filter,
  const QVector< QPair< QString, QString>>& active_desktops,
  const QVector< QPair< QString, QString>>& saved_workspaces,
  const QString& path_input,
  const QString& current_desktop
) {
  beginResetModel();
  _entries.clear();

  auto matches_filter = [&](const QString& text) {
    return filter.isEmpty() || text.contains(filter, Qt::CaseInsensitive);
  };

  // Section: active desktops
  QVector< Entry> active_entries;
  for (const auto& [name, project_dir] : active_desktops) {
    QString display = project_dir.isEmpty() ? name : name + "  " + project_dir;
    QString data_value = project_dir.isEmpty() ? name : project_dir;
    if (!matches_filter(display)) {
      continue;
    }
    active_entries.append({display, data_value, Entry_type::WORKSPACE, true});
  }

  if (!active_entries.isEmpty()) {
    _entries.append({"active", {}, Entry_type::SECTION_HEADER, false});
    _entries.append(active_entries);
  }

  // Section: saved (inactive) workspaces
  QVector< Entry> saved_entries;
  for (const auto& [name, project_dir] : saved_workspaces) {
    QString display = project_dir.isEmpty() ? name : name + "  " + project_dir;
    QString data_value = project_dir.isEmpty() ? name : project_dir;
    if (!matches_filter(display)) {
      continue;
    }
    saved_entries.append({display, data_value, Entry_type::WORKSPACE, false});
  }

  if (!saved_entries.isEmpty()) {
    _entries.append({"saved", {}, Entry_type::SECTION_HEADER, false});
    _entries.append(saved_entries);
  }

  // Section: path browsing
  if (path_input.startsWith('/')) {
    QString dir_part;
    QString prefix;

    if (path_input.endsWith('/')) {
      dir_part = path_input;
      prefix = {};
    }
    else {
      QFileInfo fi(path_input);
      dir_part = fi.path() + '/';
      prefix = fi.fileName();
    }

    QDir dir(dir_part);
    if (dir.exists()) {
      bool show_hidden = prefix.startsWith('.');

      auto filters = QDir::Dirs | QDir::NoDotAndDotDot;
      if (show_hidden) {
        filters |= QDir::Hidden;
      }

      auto entries = dir.entryInfoList(
        prefix.isEmpty() ? QStringList{} : QStringList{prefix + "*"},
        filters,
        QDir::Name
      );

      QVector< Entry> path_entries;
      int count = 0;
      for (const auto& fi : entries) {
        if (!show_hidden && fi.fileName().startsWith('.')) {
          continue;
        }
        QString path = fi.filePath() + '/';
        path_entries.append({path, path, Entry_type::PATH, false});
        if (++count >= 50) {
          break;
        }
      }

      if (!path_entries.isEmpty()) {
        _entries.append({"paths", {}, Entry_type::SECTION_HEADER, false});
        _entries.append(path_entries);
      }
    }
  }

  // Select current desktop entry, or first selectable as fallback
  _selected_index = -1;
  if (!current_desktop.isEmpty()) {
    for (int i = 0; i < _entries.size(); ++i) {
      if (_entries[i].type == Entry_type::WORKSPACE
        && _entries[i].display_text.startsWith(current_desktop))
      {
        _selected_index = i;
        break;
      }
    }
  }
  if (_selected_index < 0) {
    _selected_index = find_next_selectable(-1, 1);
  }

  endResetModel();
  emit selected_index_changed();
}

void Workspace_model::navigate(int direction) {
  if (_entries.isEmpty()) {
    return;
  }

  int next = find_next_selectable(_selected_index, direction);
  if (next != _selected_index) {
    _selected_index = next;
    emit selected_index_changed();
  }
}

int Workspace_model::selected_index() const {
  return _selected_index;
}

const Entry* Workspace_model::selected_entry() const {
  if (_selected_index >= 0 && _selected_index < _entries.size()) {
    return &_entries[_selected_index];
  }
  return nullptr;
}

int Workspace_model::find_next_selectable(int from, int direction) const {
  const int count = _entries.size();
  if (count == 0) {
    return -1;
  }

  int pos = from < 0
    ? (direction > 0 ? 0 : count - 1)
    : (from + direction + count) % count;
  for (int i = 0; i < count; ++i) {
    if (_entries[pos].type != Entry_type::SECTION_HEADER) {
      return pos;
    }
    pos = (pos + direction + count) % count;
  }

  return -1;
}

QString Workspace_model::compute_tab_completion(const QString& input) {
  if (!input.startsWith('/')) {
    return input;
  }

  QString dir_part;
  QString prefix;

  if (input.endsWith('/')) {
    dir_part = input;
    prefix = {};
  }
  else {
    QFileInfo fi(input);
    dir_part = fi.path() + '/';
    prefix = fi.fileName();
  }

  QDir dir(dir_part);
  if (!dir.exists()) {
    return input;
  }

  bool show_hidden = prefix.startsWith('.');
  auto filters = QDir::Dirs | QDir::NoDotAndDotDot;
  if (show_hidden) {
    filters |= QDir::Hidden;
  }

  auto entries = dir.entryInfoList(
    prefix.isEmpty() ? QStringList{} : QStringList{prefix + "*"},
    filters,
    QDir::Name
  );

  QStringList names;
  for (const auto& fi : entries) {
    if (!show_hidden && fi.fileName().startsWith('.')) {
      continue;
    }
    names.append(fi.fileName());
  }

  if (names.isEmpty()) {
    return input;
  }

  if (names.size() == 1) {
    return dir_part + names[0] + '/';
  }

  // Longest common prefix
  QString common = names[0];
  for (int i = 1; i < names.size(); ++i) {
    while (!names[i].startsWith(common)) {
      common.chop(1);
    }
  }

  return dir_part + common;
}
