#include "kwin_desktop.h"

#include <QDBusArgument>

#include <algorithm>

QVariantMap parse_desktop(const QDBusArgument& argument) {
  uint position = 0;
  QString id;
  QString name;
  argument.beginStructure();
  argument >> position >> id >> name;
  argument.endStructure();
  return {{"id", id}, {"name", name}, {"position", position}};
}

QVariantList parse_desktops(const QDBusArgument& argument) {
  QVariantList result;
  argument.beginArray();
  while (!argument.atEnd())
    result.append(parse_desktop(argument));
  argument.endArray();
  return result;
}

void sort_by_position(QVariantList& desktops) {
  std::sort(desktops.begin(), desktops.end(), [](const QVariant& a, const QVariant& b) {
    return a.toMap()["position"].toUInt() < b.toMap()["position"].toUInt();
  });
}
