#pragma once
#include <QString>
#include <QStringList>
// Read an existing AppImage manager's launcher; never install desktop files.
QString appImageDesktopIdentity(const QString &image, const QStringList &dataDirectories);
