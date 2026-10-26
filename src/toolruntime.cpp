#include "toolruntime.h"
#include <QtCore>
#include <QNetworkReply>

QString ToolRuntime::executable(const QString &name) {
    const auto configured = QSettings().value("tools/" + name).toString();
    if (QFileInfo(configured).isFile())
        return configured;
    const auto system = QStandardPaths::findExecutable(name);
    if (!system.isEmpty())
        return system;
    const auto local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tools/" +
                       name
#ifdef Q_OS_WIN
                       + ".exe"
#endif
        ;
    return QFileInfo(local).isFile() ? local : QString();
}
void ToolRuntime::setExecutable(const QString &name, const QString &path) {
    QSettings().setValue("tools/" + name, QFileInfo(path).absoluteFilePath());
}

RuntimeInstaller::RuntimeInstaller(QObject *parent) : QObject(parent), network(this), runner(this) {}
void RuntimeInstaller::fail(const QString &error) {
    active = false;
    temporary.reset();
    emit finished({}, error);
}
void RuntimeInstaller::fetch(const QUrl &url, qint64 maximum,
                             std::function<void(QByteArray, QString)> callback) {
    if (url.scheme() != "https" ||
        !QStringList{"api.github.com", "github.com", "release-assets.githubusercontent.com",
                     "objects.githubusercontent.com"}
             .contains(url.host())) {
        callback({}, "Adresse de téléchargement non autorisée.");
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(120000);
    request.setRawHeader("User-Agent", "GitHub-Enhanced");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network.get(request);
    if (maximum > 10 * 1024 * 1024) {
        auto previous = std::make_shared<int>(-2);
        connect(reply, &QNetworkReply::downloadProgress, this, [this, previous](qint64 received, qint64 total) {
            if (total <= 0) return;
            const int percent = int(received * 100 / total);
            if (percent < *previous + 2 && percent != 100) return;
            *previous = percent;
            emit progress(QString("Téléchargement : %1% · %2 / %3 Mio").arg(percent).arg(received / 1048576.0, 0, 'f', 1).arg(total / 1048576.0, 0, 'f', 1));
        });
    }
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, bytes, maximum] {
        bytes->append(reply->readAll());
        if (bytes->size() > maximum)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [reply, bytes, maximum, callback] {
        bytes->append(reply->readAll());
        const auto error = reply->error();
        const auto message = reply->errorString();
        reply->deleteLater();
        callback(error == QNetworkReply::NoError && bytes->size() <= maximum ? *bytes : QByteArray{},
                 error == QNetworkReply::NoError && bytes->size() <= maximum ? QString() : message);
    });
}
void RuntimeInstaller::install(const QString &name) {
    if (active || (name != "gh" && name != "codex" && name != "git-lfs"))
        return;
    active = true;
    tool = name;
    temporary = std::make_unique<QTemporaryDir>();
    if (!temporary->isValid()) {
        fail("Impossible de préparer le téléchargement.");
        return;
    }
    emit progress("Recherche de la version officielle…");
    const auto repository = name == "gh" ? "cli/cli" : name == "git-lfs" ? "git-lfs/git-lfs" : "openai/codex";
    fetch(QUrl("https://api.github.com/repos/" + QString(repository) + "/releases/latest"), 4 * 1024 * 1024,
          [this](QByteArray bytes, QString error) {
              if (!error.isEmpty()) {
                  fail(error);
                  return;
              }
              const bool arm = QSysInfo::currentCpuArchitecture().contains("arm") ||
                               QSysInfo::currentCpuArchitecture().contains("aarch");
#if defined(Q_OS_WIN)
              const QString ghPart = arm ? "windows_arm64.zip" : "windows_amd64.zip";
              const QString codexPart =
                  arm ? "codex-aarch64-pc-windows-msvc.exe.zip" : "codex-x86_64-pc-windows-msvc.exe.zip";
#elif defined(Q_OS_MACOS)
        const QString ghPart = arm ? "macOS_arm64.zip" : "macOS_amd64.zip";
        const QString codexPart = arm ? "codex-aarch64-apple-darwin.tar.gz" : "codex-x86_64-apple-darwin.tar.gz";
#else
        const QString ghPart = arm ? "linux_arm64.tar.gz" : "linux_amd64.tar.gz";
        const QString codexPart = arm ? "codex-aarch64-unknown-linux-musl.tar.gz" : "codex-x86_64-unknown-linux-musl.tar.gz";
#endif
              QString lfsPlatform;
#if defined(Q_OS_WIN)
              lfsPlatform = "windows";
#elif defined(Q_OS_MACOS)
              lfsPlatform = "darwin";
#else
              lfsPlatform = "linux";
#endif
              const QString lfsPrefix = "git-lfs-" + lfsPlatform + (arm ? "-arm64-v" : "-amd64-v");
              QJsonObject asset;
              for (const auto &value : QJsonDocument::fromJson(bytes).object()["assets"].toArray()) {
                  auto object = value.toObject();
                  const auto filename = object["name"].toString();
                  if (tool == "gh" ? filename.endsWith(ghPart) : tool == "git-lfs" ? (filename.startsWith(lfsPrefix) && (filename.endsWith(".tar.gz") || filename.endsWith(".zip"))) : filename == codexPart) {
                      asset = object;
                      break;
                  }
              }
              const auto filename = asset["name"].toString();
              if (filename != QFileInfo(filename).fileName() || filename.contains('\\')) {
                  fail("Nom d’archive invalide.");
                  return;
              }
              const auto digest = asset["digest"].toString();
              if (asset.isEmpty() ||
                  !QRegularExpression("^sha256:[a-fA-F0-9]{64}$").match(digest).hasMatch()) {
                  fail("Aucun téléchargement vérifiable pour cet appareil. Installez l’outil depuis son site "
                       "officiel, puis sélectionnez son exécutable.");
                  return;
              }
              emit progress("Téléchargement de " + asset["name"].toString() + "…");
              fetch(QUrl(asset["browser_download_url"].toString()), 200 * 1024 * 1024,
                    [this, asset, digest](QByteArray archive, QString error) {
                        if (!error.isEmpty()) {
                            fail(error);
                            return;
                        }
                        if (QCryptographicHash::hash(archive, QCryptographicHash::Sha256).toHex() !=
                            digest.mid(7).toLatin1().toLower()) {
                            fail("L’empreinte du téléchargement ne correspond pas.");
                            return;
                        }
                        const auto path = temporary->filePath(asset["name"].toString());
                        QFile file(path);
                        if (!file.open(QIODevice::WriteOnly) || file.write(archive) != archive.size()) {
                            fail("Impossible d’enregistrer l’archive.");
                            return;
                        }
                        file.close();
                        emit progress("Installation dans le profil de l’application…");
                        runner.run(
                            "tar", {"-xf", path, "-C", temporary->path()}, temporary->path(),
                            [this](const CommandResult &result) {
                                if (!result.ok()) {
                                    fail("Extraction impossible. Vérifiez que tar est installé.\n" +
                                         result.message());
                                    return;
                                }
                                QString candidate;
                                QDirIterator files(temporary->path(), QDir::Files,
                                                   QDirIterator::Subdirectories);
                                while (files.hasNext()) {
                                    files.next();
                                    auto name = files.fileName();
                                    if (((tool == "gh" || tool == "git-lfs") && (name == tool || name == tool + ".exe")) ||
                                        (tool == "codex" && name.startsWith("codex-") &&
                                         !name.endsWith(".gz") && !name.endsWith(".zip") &&
                                         !name.endsWith(".json") && !name.endsWith(".sig"))) {
                                        candidate = files.filePath();
                                        break;
                                    }
                                }
                                if (candidate.isEmpty()) {
                                    fail("L’archive ne contient pas l’exécutable attendu.");
                                    return;
                                }
                                auto directory =
                                    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                                    "/tools";
                                QDir().mkpath(directory);
                                const auto destination = directory + '/' + tool
#ifdef Q_OS_WIN
                                                         + ".exe"
#endif
                                    ;
                                QFile input(candidate);
                                QSaveFile output(destination);
                                if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly) ||
                                    output.write(input.readAll()) < 0 || !output.commit()) {
                                    fail("Impossible d’installer l’exécutable.");
                                    return;
                                }
                                QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner |
                                                                       QFile::ExeOwner | QFile::ReadGroup |
                                                                       QFile::ExeGroup | QFile::ReadOther |
                                                                       QFile::ExeOther);
                                ToolRuntime::setExecutable(tool, destination);
                                active = false;
                                temporary.reset();
                                emit finished(destination, {});
                            });
                    });
          });
}
