#include "appupdater.h"
#include <QtCore>
#include <QtNetwork>
namespace {
bool trusted(const QUrl &url, const QString &repository) {
  return url.scheme() == "https" && url.host() == "github.com" &&
         url.userInfo().isEmpty() && url.port(-1) == -1 && !url.hasQuery() &&
         !url.hasFragment() &&
         url.path().startsWith('/' + repository + "/releases/",
                               Qt::CaseInsensitive) &&
         !url.path().contains("/../");
}
QNetworkRequest request(const QUrl &url) {
  QNetworkRequest r(url);
  r.setTransferTimeout(20000);
  r.setRawHeader("User-Agent",
                 "GitHub-Enhanced/" +
                     QCoreApplication::applicationVersion().toUtf8());
  r.setRawHeader("Accept", "application/vnd.github+json");
  r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                 QNetworkRequest::UserVerifiedRedirectPolicy);
  return r;
}
bool assetRedirect(const QUrl &url) {
  return url.scheme() == "https" && url.userInfo().isEmpty() &&
         QStringList{"github.com", "release-assets.githubusercontent.com",
                     "objects.githubusercontent.com"}
             .contains(url.host());
}
} // namespace
AppUpdater::AppUpdater(QObject *parent, QNetworkAccessManager *transport)
    : QObject(parent),
      network(transport ? transport : new QNetworkAccessManager(this)) {
  qRegisterMetaType<AppUpdate>();
}
QString AppUpdater::repository() {
  auto value = QSettings()
                   .value("updates/repository", "AlloyTheReal/Github-enhanced")
                   .toString()
                   .trimmed();
  return QRegularExpression("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
                 .match(value)
                 .hasMatch()
             ? value
             : QString();
}
QString AppUpdater::platform() {
#ifdef Q_OS_WIN
  return "windows";
#elif defined(Q_OS_MACOS)
  return "macos";
#else
  return "linux";
#endif
}
bool AppUpdater::newer(const QString &candidate, const QString &current) {
  const QRegularExpression version("^[vV]?(\\d+)\\.(\\d+)\\.(\\d+)$");
  auto a = version.match(candidate), b = version.match(current);
  if (!a.hasMatch() || !b.hasMatch())
    return false;
  for (int i = 1; i <= 3; ++i) {
    bool okA = false, okB = false;
    auto x = a.captured(i).toULongLong(&okA),
         y = b.captured(i).toULongLong(&okB);
    if (!okA || !okB)
      return false;
    if (x != y)
      return x > y;
  }
  return false;
}
AppUpdate AppUpdater::select(const QJsonObject &release, const QString &current,
                             const QString &os, const QString &arch,
                             const QString &repo, QString *error) {
  if (error)
    error->clear();
  AppUpdate info;
  if (release["draft"].toBool() || release["prerelease"].toBool() ||
      !newer(release["tag_name"].toString(), current))
    return info;
  info.version = release["tag_name"].toString();
  info.notes = release["body"].toString().left(50000);
  info.releaseUrl = QUrl(release["html_url"].toString());
  if (!trusted(info.releaseUrl, repo)) {
    if (error)
      *error = "La release ne provient pas du dépôt de mise à jour configuré.";
    return {};
  }
  int best = -1;
  for (const auto &value : release["assets"].toArray()) {
    auto asset = value.toObject();
    auto name = asset["name"].toString();
    auto lower = name.toLower();
    const bool arm = arch.contains("arm") || arch.contains("aarch");
    const bool namedArm = lower.contains("arm64") || lower.contains("aarch64");
    if (arm && !namedArm && !lower.contains("universal"))
      continue;
    if (!arm && namedArm && !lower.contains("universal"))
      continue;
    int score = -1;
    if (os == "linux" && lower.endsWith(".appimage"))
      score = 10;
    else if (os == "windows" && lower.endsWith(".exe"))
      score = 10;
    else if (os == "windows" && lower.endsWith(".msi"))
      score = 5;
    else if (os == "macos" && lower.endsWith(".dmg"))
      score = 10;
    if (score <= best)
      continue;
    QUrl url(asset["browser_download_url"].toString());
    if (!trusted(url, repo) || !url.path().contains("/releases/download/") ||
        QFileInfo(name).fileName() != name || name.contains('\\'))
      continue;
    auto digest = asset["digest"].toString();
    const qint64 size = asset["size"].toDouble();
    if (!QRegularExpression("^sha256:[0-9a-fA-F]{64}$")
             .match(digest)
             .hasMatch() ||
        size <= 0 || size > 2147483648LL)
      continue;
    best = score;
    info.assetName = name;
    info.assetUrl = url;
    info.sha256 = digest.mid(7).toLower();
    info.bytes = size;
  }
  return info;
}
void AppUpdater::check(bool manual) {
  if (active)
    return;
  const auto repo = repository();
  if (repo.isEmpty()) {
    emit checked({}, "Le dépôt de mise à jour est invalide.", manual);
    return;
  }
  active = true;
  auto *reply = network->get(request(
      QUrl("https://api.github.com/repos/" + repo + "/releases/latest")));
  auto bytes = std::make_shared<QByteArray>();
  connect(reply, &QNetworkReply::readyRead, this, [reply, bytes] {
    bytes->append(reply->readAll());
    if (bytes->size() > 2097152)
      reply->abort();
  });
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, bytes, repo, manual] {
        active = false;
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                .toInt() == 404) {
          update = {};
          emit checked(
              {}, "Aucune release stable n’est encore publiée sur ce dépôt.",
              manual);
          return;
        }
        if (reply->error() != QNetworkReply::NoError ||
            bytes->size() > 2097152) {
          const auto http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
          const auto apiMessage = QJsonDocument::fromJson(*bytes).object()["message"].toString().left(1000);
          QString reason;
          if (bytes->size() > 2097152)
            reason = "La réponse de GitHub est trop volumineuse. Réessayez plus tard.";
          else if (http == 429 || (http == 403 &&
                   (reply->rawHeader("X-RateLimit-Remaining") == "0" ||
                    apiMessage.contains("rate limit", Qt::CaseInsensitive)))) {
            reason = "GitHub limite temporairement les recherches de mises à jour. Réessayez plus tard.";
            bool validReset = false;
            const auto reset = reply->rawHeader("X-RateLimit-Reset").toLongLong(&validReset);
            if (validReset)
              reason += " Nouvelle tentative possible après " +
                  QDateTime::fromSecsSinceEpoch(reset).toLocalTime().toString("dd/MM/yyyy HH:mm") + ".";
          } else if (!QSslSocket::supportsSsl())
            reason = "Le composant HTTPS de l’application ne fonctionne pas. Réinstallez l’application depuis la release GitHub.";
          else if (reply->error() == QNetworkReply::SslHandshakeFailedError)
            reason = "La connexion sécurisée à GitHub a échoué. Vérifiez les certificats, l’heure du système et le proxy.";
          else if (reply->error() == QNetworkReply::TimeoutError ||
                   reply->error() == QNetworkReply::OperationCanceledError)
            reason = "La recherche de mise à jour a expiré. Vérifiez la connexion et réessayez.";
          else if (http >= 500)
            reason = "GitHub est temporairement indisponible. Réessayez plus tard.";
          else if (http == 401 || http == 403)
            reason = "GitHub refuse l’accès aux mises à jour. Vérifiez l’accès au dépôt et la configuration du proxy.";
          else
            reason = "La connexion à GitHub a échoué. Vérifiez votre connexion et votre proxy, puis réessayez.";
          const auto details = QString("URL : %1\nHTTP %2 — %3 (Qt %4)\n%5")
              .arg(reply->url().toString()).arg(http).arg(reply->errorString())
              .arg(int(reply->error())).arg(apiMessage).trimmed();
          emit checked({}, "La recherche de mise à jour a échoué. " + reason + "\n" + details, manual);
          return;
        }
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(*bytes, &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
          emit checked({}, "La réponse des mises à jour est invalide.", manual);
          return;
        }
        QString error;
        update = select(document.object(),
                        QCoreApplication::applicationVersion(), platform(),
                        QSysInfo::currentCpuArchitecture(), repo, &error);
        staged.clear();
        QSettings().setValue("updates/lastChecked",
                             QDateTime::currentDateTimeUtc());
        emit checked(update, error, manual);
      });
}
void AppUpdater::download() {
  if (active || !update.available() || update.assetUrl.isEmpty())
    return;
  active = true;
  staged.clear();
  auto folder =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/updates";
  QDir().mkpath(folder);
  const auto destination = folder + '/' + update.assetName;
  auto file = std::make_shared<QSaveFile>(destination);
  if (!file->open(QIODevice::WriteOnly)) {
    active = false;
    emit downloadFinished({},
                          "Le dossier de téléchargement n’est pas accessible.");
    return;
  }
  auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
  auto received = std::make_shared<qint64>(0);
  auto error = std::make_shared<QString>();
  const auto expected = update;
  auto downloadRequest = request(expected.assetUrl);
  downloadRequest.setRawHeader("Accept", "application/octet-stream");
  auto *reply = network->get(downloadRequest);
  connect(reply, &QNetworkReply::redirected, this, [reply](const QUrl &url) {
    if (assetRedirect(url))
      reply->redirectAllowed();
    else
      reply->abort();
  });
  connect(reply, &QNetworkReply::readyRead, this,
          [this, reply, file, hash, received, error, expected] {
            while (reply->bytesAvailable() > 0) {
              auto chunk = reply->read(65536);
              *received += chunk.size();
              if (*received > expected.bytes ||
                  file->write(chunk) != chunk.size()) {
                *error = "Le téléchargement dépasse la taille prévue ou ne "
                         "peut pas être enregistré.";
                reply->abort();
                return;
              }
              hash->addData(chunk);
            }
            emit progress(*received, expected.bytes);
          });
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, file, hash, received, error, expected, destination] {
            active = false;
            reply->deleteLater();
            if (error->isEmpty() && reply->error() != QNetworkReply::NoError)
              *error =
                  "Le téléchargement a été interrompu. Vous pouvez réessayer.";
            if (error->isEmpty() &&
                (*received != expected.bytes ||
                 hash->result().toHex() != expected.sha256.toLatin1()))
              *error = "L’empreinte du fichier ne correspond pas à la release "
                       "GitHub. Le fichier a été rejeté.";
            if (error->isEmpty() && !file->commit())
              *error =
                  "Impossible de terminer l’enregistrement de la mise à jour.";
            if (!error->isEmpty())
              file->cancelWriting();
            else
              staged = destination;
            emit downloadFinished(staged, *error);
          });
}
bool AppUpdater::replaceAppImage(const QString &target, const QString &source,
                                 const QString &sha256, QString *error) {
  auto fail = [error](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  if (!QRegularExpression("^[0-9a-fA-F]{64}$").match(sha256).hasMatch())
    return fail("L’empreinte de mise à jour est manquante.");
  const QFileInfo info(target);
  if (!info.isFile() ||
      !info.fileName().endsWith(".AppImage", Qt::CaseInsensitive) ||
      info.isSymLink() || !QFileInfo(info.absolutePath()).isWritable() ||
      QFileInfo(source).canonicalFilePath() == info.canonicalFilePath())
    return fail("Cette installation doit être mise à jour avec son "
                "gestionnaire de paquets ou depuis les releases GitHub.");
  QFile input(source);
  if (!input.open(QIODevice::ReadOnly))
    return fail("La mise à jour téléchargée est introuvable.");
  auto magic = input.peek(11);
  if (!magic.startsWith(QByteArray("\x7f"
                                   "ELF",
                                   4)) ||
      magic.mid(8, 3) != QByteArray("AI\x02", 3))
    return fail("Le fichier téléchargé n’est pas une AppImage de type 2.");
  QCryptographicHash initialHash(QCryptographicHash::Sha256);
  if (!initialHash.addData(&input) ||
      initialHash.result().toHex() != sha256.toLower().toLatin1() ||
      !input.seek(0))
    return fail("La mise à jour a changé depuis sa vérification. "
                "Téléchargez-la à nouveau.");
  auto backup = target + ".previous";
  if (QFileInfo::exists(backup) && !QFile::remove(backup))
    return fail(
        "Impossible de remplacer la sauvegarde de la précédente AppImage.");
  if (!QFile::copy(target, backup))
    return fail("La sauvegarde de l’AppImage a échoué.");
  QSaveFile output(target);
  if (!output.open(QIODevice::WriteOnly))
    return fail(
        "Le fichier de l’application n’est pas accessible en écriture.");
  QCryptographicHash verified(QCryptographicHash::Sha256);
  while (!input.atEnd()) {
    auto chunk = input.read(1048576);
    if (chunk.isEmpty() && input.error() != QFile::NoError)
      return fail("La lecture de la mise à jour a échoué.");
    verified.addData(chunk);
    if (output.write(chunk) != chunk.size())
      return fail("Le disque ne peut pas recevoir la mise à jour.");
  }
  if (verified.result().toHex() != sha256.toLower().toLatin1()) {
    output.cancelWriting();
    return fail("La mise à jour a changé depuis sa vérification. "
                "Téléchargez-la à nouveau.");
  }
  output.setPermissions(info.permissions() | QFile::ExeOwner);
  if (!output.commit())
    return fail(
        "Le remplacement de l’AppImage a échoué. La sauvegarde est conservée.");
  return true;
}
