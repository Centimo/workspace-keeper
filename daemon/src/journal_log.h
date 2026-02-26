#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logServer)
Q_DECLARE_LOGGING_CATEGORY(logShortcut)
Q_DECLARE_LOGGING_CATEGORY(logWindow)
Q_DECLARE_LOGGING_CATEGORY(logClaude)

/// Installs a custom Qt message handler that sends log messages to systemd journal
/// via sd_journal_send and duplicates them to stderr for terminal debugging.
/// Must be called before QApplication construction.
void install_journal_handler();
