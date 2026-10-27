#pragma once
#include <QString>

namespace DesktopIntegration {
// Install the AppImage launcher and icon where the Wayland compositor can find them.
bool installAppImageLauncher(const QString &image, const QString &dataDirectory,
                             const QString &iconSource, QString *error = nullptr);
}
