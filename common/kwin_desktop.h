#pragma once

#include <QVariantList>
#include <QVariantMap>

class QDBusArgument;

/// Parse KWin VirtualDesktop struct (position: uint, id: string, name: string).
QVariantMap parse_desktop(const QDBusArgument& argument);

/// Parse desktops array from a QDBusArgument containing a(uss).
QVariantList parse_desktops(const QDBusArgument& argument);

/// Sort desktops list by position.
void sort_by_position(QVariantList& desktops);
