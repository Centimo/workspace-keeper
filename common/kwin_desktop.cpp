#include "kwin_desktop.h"

#include <QDBusArgument>

#include <algorithm>

QVariantMap Kwin_desktop::to_variant_map() const {
  return {{"id", id}, {"name", name}, {"position", position}};
}

Kwin_desktop parse_desktop(const QDBusArgument& argument) {
  uint position = 0;
  QString id;
  QString name;
  argument.beginStructure();
  argument >> position >> id >> name;
  argument.endStructure();
  return {.id = id, .name = name, .position = position};
}

QVector< Kwin_desktop> parse_desktops(const QDBusArgument& argument) {
  QVector< Kwin_desktop> result;
  argument.beginArray();
  while (!argument.atEnd())
    result.append(parse_desktop(argument));
  argument.endArray();
  return result;
}

void sort_by_position(QVector< Kwin_desktop>& desktops) {
  std::sort(desktops.begin(), desktops.end(), [](const Kwin_desktop& a, const Kwin_desktop& b) {
    return a.position < b.position;
  });
}
