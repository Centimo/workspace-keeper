#pragma once

#include "workspace_model.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QPair>

class Workspace_menu : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString filter_text READ filter_text WRITE set_filter_text NOTIFY filter_text_changed)
  Q_PROPERTY(Workspace_model* model READ model CONSTANT)

 public:
  explicit Workspace_menu(QObject* parent = nullptr);

  void begin_session();

  QString filter_text() const;
  void set_filter_text(const QString& text);

  Workspace_model* model();

  /// @return response string ("select <data>" or "custom_input <data>"), empty if nothing selected
  Q_INVOKABLE QString select_current();
  /// @return response string ("close <data>"), empty if not a workspace
  Q_INVOKABLE QString close_current();
  Q_INVOKABLE QString tab_complete();

 signals:
  void filter_text_changed();

 private:
  void load_data();
  void rebuild_model();

  QString _filter_text;
  QString _workspace_dir;

  QVector< QPair< QString, QString>> _active_desktops;
  QVector< QPair< QString, QString>> _saved_workspaces;
  QString _current_desktop_name;

  Workspace_model _model;
};
