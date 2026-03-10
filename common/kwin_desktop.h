#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

class QDBusArgument;

/// KWin virtual desktop metadata.
struct Kwin_desktop {
  QString id;
  QString name;
  uint position = 0;

  /// Convert to QVariantMap for QML property access.
  QVariantMap to_variant_map() const;
};

/// Parse KWin VirtualDesktop struct (position: uint, id: string, name: string).
Kwin_desktop parse_desktop(const QDBusArgument& argument);

/// Parse desktops array from a QDBusArgument containing a(uss).
QVector< Kwin_desktop> parse_desktops(const QDBusArgument& argument);

/// Sort desktops list by position.
void sort_by_position(QVector< Kwin_desktop>& desktops);
