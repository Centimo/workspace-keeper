#include "plugin.h"
#include "workspace_monitor.h"

#include <QtQml>

void Plugin::registerTypes(const char* uri) {
  Q_ASSERT(QString(uri) == "org.workspace.monitor");
  qmlRegisterType< Workspace_monitor>(uri, 1, 0, "WorkspaceMonitor");
}
