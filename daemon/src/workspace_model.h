#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

enum class Entry_type {
  SECTION_HEADER,
  WORKSPACE,
  PATH
};

struct Entry {
  QString display_text;
  QString data;
  Entry_type type;
  bool is_active;
};

class Workspace_model : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int selected_index READ selected_index NOTIFY selected_index_changed)

 public:
  enum Roles {
    DISPLAY_TEXT = Qt::UserRole + 1,
    DATA,
    ENTRY_TYPE,
    IS_ACTIVE
  };

  explicit Workspace_model(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash< int, QByteArray> roleNames() const override;

  void rebuild(
    const QString& filter,
    const QVector< QPair< QString, QString>>& active_desktops,
    const QVector< QPair< QString, QString>>& saved_workspaces,
    const QString& path_input
  );

  Q_INVOKABLE void navigate(int direction);

  int selected_index() const;

  const Entry* selected_entry() const;

  static QString compute_tab_completion(const QString& input);

 signals:
  void selected_index_changed();

 private:
  int find_next_selectable(int from, int direction) const;

  QVector< Entry> _entries;
  int _selected_index = -1;
};
