#include "desktopintegration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {
QString desktopString(QString value) {
    return value.replace('\\', "\\\\").replace('\n', "\\n").replace('\r', "\\r").replace('\t', "\\t");
}
QString executableArgument(QString value) {
    // Exec quoting is decoded after desktop-entry string escaping.
    value.replace('\\', "\\\\").replace('"', "\\\"").replace('`', "\\`").replace('$', "\\$");
    value.replace('%', "%%");
    return desktopString('"' + value + '"');
}
bool save(const QString &path, const QByteArray &bytes, QString *error) {
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes)
        return true;
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        return true;
    if (error)
        *error = "Impossible d’installer le lanceur : " + path + " : " + file.errorString();
    return false;
}
}

bool DesktopIntegration::installAppImageLauncher(const QString &image, const QString &dataDirectory,
                                                const QString &iconSource, QString *error) {
    if (error)
        error->clear();
    QFileInfo executable(image);
    if (image.isEmpty() || !executable.isFile() || !executable.isExecutable() ||
        image.contains('=') || image.contains('\n') || image.contains('\r') || dataDirectory.isEmpty()) {
        if (error)
            *error = "Le chemin de l’AppImage n’est pas utilisable pour son lanceur.";
        return false;
    }
    const QString applications = dataDirectory + "/applications";
    const QString icons = dataDirectory + "/icons/hicolor/1024x1024/apps";
    if (!QDir().mkpath(applications) || !QDir().mkpath(icons)) {
        if (error)
            *error = "Le dossier des lanceurs ou des icônes n’est pas accessible.";
        return false;
    }
    QFile source(iconSource);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "Le logo de l’application est introuvable.";
        return false;
    }
    const auto icon = icons + "/io.githubenhanced.GitHubEnhanced.png";
    const auto bytes = source.readAll();
    if (bytes.isEmpty() || !save(icon, bytes, error))
        return false;
    // Keep a stable executable for desktop parsers that probe it before
    // expanding field codes (notably %% in an AppImage filename).
    const auto entry = QString(
        "[Desktop Entry]\nType=Application\nName=GitHub Enhanced\n"
        "Comment=Client Git natif et personnalisable\nExec=/usr/bin/env -- %1 %f\nIcon=%2\n"
        "Terminal=false\nCategories=Development;RevisionControl;\n"
        "StartupNotify=true\nStartupWMClass=github-enhanced\nX-GitHubEnhanced-AppImage=true\n")
                           .arg(executableArgument(executable.absoluteFilePath()), desktopString(icon));
    return save(applications + "/io.githubenhanced.GitHubEnhanced.desktop", entry.toUtf8(), error);
}
