#include "journal_log.h"

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

#include <cstdio>
#include <unistd.h>

Q_LOGGING_CATEGORY(logServer, "workspace-menu.server")
Q_LOGGING_CATEGORY(logShortcut, "workspace-menu.shortcut")
Q_LOGGING_CATEGORY(logWindow, "workspace-menu.window")
Q_LOGGING_CATEGORY(logClaude, "workspace-menu.claude")

static int qt_to_journal_priority(QtMsgType type) {
  switch (type) {
    case QtDebugMsg:    return LOG_DEBUG;
    case QtInfoMsg:     return LOG_INFO;
    case QtWarningMsg:  return LOG_WARNING;
    case QtCriticalMsg: return LOG_CRIT;
    case QtFatalMsg:    return LOG_EMERG;
  }
  return LOG_INFO;
}

static void journal_message_handler(
  QtMsgType type,
  const QMessageLogContext& context,
  const QString& message
) {
  auto utf8 = message.toUtf8();
  const char* category = context.category ? context.category : "default";

  sd_journal_send(
    "MESSAGE=%s", utf8.constData(),
    "PRIORITY=%d", qt_to_journal_priority(type),
    "SYSLOG_IDENTIFIER=workspace-menu",
    "QT_CATEGORY=%s", category,
    "CODE_FILE=%s", context.file ? context.file : "",
    "CODE_LINE=%d", context.line,
    "CODE_FUNC=%s", context.function ? context.function : "",
    nullptr
  );

  static const bool tty = isatty(STDERR_FILENO);
  if (tty) {
    fprintf(stderr, "[%s] %s\n", category, utf8.constData());
  }
}

void install_journal_handler() {
  qInstallMessageHandler(journal_message_handler);
}
