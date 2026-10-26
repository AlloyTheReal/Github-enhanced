#include "githubclient.h"
#include "toolruntime.h"
#include <QtCore>

namespace {
QString readable(QByteArray bytes) {
  auto value = QString::fromUtf8(bytes);
  value.remove(QRegularExpression("\x1b\\[[0-?]*[ -/]*[@-~]"));
  return value;
}
} // namespace
GitHubClient::GitHubClient(QObject *parent, const QString &path)
    : QObject(parent), runner(this), program(path) {
  connect(&runner, &CommandRunner::busyChanged, this,
          &GitHubClient::busyChanged);
  connect(&runner, &CommandRunner::outputReceived, this,
          &GitHubClient::operationOutput);
  runner.setEnvironmentRemovals({"GH_TOKEN", "GITHUB_TOKEN",
                                 "GH_ENTERPRISE_TOKEN",
                                 "GITHUB_ENTERPRISE_TOKEN"});
  authDeadline.setSingleShot(true);
  connect(&authDeadline, &QTimer::timeout, this, [this] {
    cancelLogin();
    emit loginFinished(false, "La connexion GitHub a expiré.");
  });
  connect(&auth, &QProcess::started, this, [this] {
    auth.write("\n");
    auth.closeWriteChannel();
  });
  connect(&auth, &QProcess::readyReadStandardOutput, this,
          [this] { emit authOutput(readable(auth.readAllStandardOutput())); });
  connect(&auth, &QProcess::readyReadStandardError, this,
          [this] { emit authOutput(readable(auth.readAllStandardError())); });
  connect(&auth, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              authDeadline.stop();
              emit loginFinished(
                  false,
                  "GitHub CLI est introuvable. Installez-le depuis cet écran.");
            }
          });
  connect(&auth, &QProcess::finished, this,
          [this](int code, QProcess::ExitStatus status) {
            authDeadline.stop();
            if (closing)
              return;
            if (code != 0 || status != QProcess::NormalExit) {
              emit loginFinished(
                  false,
                  "La connexion GitHub n’a pas abouti. Vous pouvez réessayer.");
              return;
            }
            runner.run(executable(),
                       {"auth", "setup-git", "--hostname", "github.com"}, {},
                       [this](const CommandResult &r) {
                         if (!r.ok())
                           emit errorOccurred("Compte connecté, mais l’accès "
                                              "Git n’a pas été configuré : " +
                                              r.message());
                         checkAccount();
                         emit loginFinished(true, {});
                       });
          });
}
GitHubClient::~GitHubClient() {
  closing = true;
  auth.disconnect(this);
  if (auth.state() != QProcess::NotRunning) {
    auth.kill();
    auth.waitForFinished(1500);
  }
}
QString GitHubClient::executable() const {
  return program.isEmpty() ? ToolRuntime::executable("gh") : program;
}
void GitHubClient::api(const QString &endpoint, const QString &method,
                       const QJsonObject &payload, Reply callback) {
  const auto path = executable();
  if (path.isEmpty()) {
    callback({}, "Installez GitHub CLI dans les réglages du compte.");
    return;
  }
  QStringList args{"api", "--hostname", "github.com", endpoint};
  QByteArray input;
  if (method != "GET") {
    args << "--method" << method << "--input" << "-";
    input = QJsonDocument(payload).toJson(QJsonDocument::Compact);
  }
  runner.run(
      path, args, {},
      [callback](const CommandResult &r) {
        if (!r.ok()) {
          const auto response = QJsonDocument::fromJson(r.output).object();
          QStringList errors;
          for (const auto &entry : response["errors"].toArray()) {
            auto object = entry.toObject();
            if (object["code"] == "already_exists")
              errors << "Ce nom existe déjà. Choisissez un autre nom.";
            else if (entry.isString())
              errors << entry.toString();
            else if (!object["message"].toString().isEmpty())
              errors << object["message"].toString();
          }
          const auto message = response["message"].toString();
          const auto explanation = !errors.isEmpty() ? errors.join("\n") : message;
          const auto original = r.message().right(6000);
          callback({}, explanation.isEmpty() || original.contains(explanation) ? original : explanation + "\n" + original);
          return;
        }
        if (r.output.trimmed().isEmpty()) {
          callback({}, {});
          return;
        }
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(r.output, &error);
        if (error.error != QJsonParseError::NoError) {
          callback({}, "Réponse GitHub invalide.");
          return;
        }
        callback(doc.isArray() ? QJsonValue(doc.array())
                               : QJsonValue(doc.object()),
                 {});
      },
      input);
}
void GitHubClient::checkAccount() {
  api("user", "GET", {}, [this](QJsonValue result, QString error) {
    if (!error.isEmpty()) {
      currentAccount = {};
      emit accountChanged({});
      emit errorOccurred(error);
      return;
    }
    currentAccount = result.toObject();
    emit accountChanged(currentAccount);
  });
}
bool GitHubClient::workflowPermissionMissing(const QString &message) {
  const auto text = message.toLower();
  return text.contains("workflow") && text.contains("scope") &&
         (text.contains("without") || text.contains("missing") ||
          text.contains("insufficient") || text.contains("requires"));
}
void GitHubClient::login() { startAuthentication(false); }
void GitHubClient::authorizeWorkflows() { startAuthentication(true); }
void GitHubClient::startAuthentication(bool workflowsOnly) {
  if (auth.state() != QProcess::NotRunning)
    return;
  const auto path = executable();
  if (path.isEmpty()) {
    emit loginFinished(false, "Installez GitHub CLI pour vous connecter.");
    return;
  }
  auto env = QProcessEnvironment::systemEnvironment();
  for (const auto &key : {"GH_TOKEN", "GITHUB_TOKEN", "GH_PROMPT_DISABLED",
                          "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN"})
    env.remove(key);
  env.insert("GH_HOST", "github.com");
  env.insert("NO_COLOR", "1");
  auth.setProcessEnvironment(env);
  const QStringList arguments =
      workflowsOnly
          ? QStringList{"auth",       "refresh",  "--hostname",
                        "github.com", "--scopes", "workflow"}
          : QStringList{"auth",       "login",          "--hostname",
                        "github.com", "--git-protocol", "https",
                        "--web",      "--scopes",       "gist,workflow"};
  auth.start(path, arguments);
  authDeadline.start(600000);
}
void GitHubClient::cancelLogin() {
  authDeadline.stop();
  if (auth.state() != QProcess::NotRunning)
    auth.kill();
}
void GitHubClient::logout() {
  const auto login = currentAccount["login"].toString();
  if (login.isEmpty())
    return;
  runner.run(executable(),
             {"auth", "logout", "--hostname", "github.com", "--user", login},
             {}, [this](const CommandResult &r) {
               if (!r.ok())
                 emit errorOccurred(r.message());
               else {
                 currentAccount = {};
                 emit accountChanged({});
               }
             });
}
void GitHubClient::repositories(int page) {
  page = qMax(1, page);
  api(QString("user/"
              "repos?per_page=100&page=%1&sort=updated&affiliation=owner,"
              "collaborator,organization_member")
          .arg(page),
      "GET", {}, [this, page](QJsonValue value, QString error) {
        if (!error.isEmpty()) {
          emit errorOccurred(error);
          return;
        }
        auto array = value.toArray();
        emit repositoriesLoaded(array, page, array.size() == 100);
      });
}
void GitHubClient::createRepository(const QString &name,
                                    const QString &description,
                                    bool privateRepository, Reply callback,
                                    bool initializeReadme,
                                    const QString &organization) {
  const auto normalized = normalizedRepositoryName(name.trimmed());
  if (!QRegularExpression("^[A-Za-z0-9_.-]{1,100}$")
           .match(normalized)
           .hasMatch() ||
      normalized == "." || normalized == "..") {
    callback({}, "Le nom du dépôt est invalide.");
    return;
  }
  if (!organization.isEmpty() &&
      !QRegularExpression("^[A-Za-z0-9-]+$").match(organization).hasMatch()) {
    callback({}, "Le nom de l’organisation est invalide.");
    return;
  }
  api(organization.isEmpty() ? "user/repos" : "orgs/" + organization + "/repos",
      "POST",
      {{"name", normalized},
       {"description", description},
       {"private", privateRepository},
       {"auto_init", initializeReadme}},
      std::move(callback));
}

QString GitHubClient::normalizedRepositoryName(const QString &name) {
  auto normalized = name;
  return normalized.replace(
      QRegularExpression("\\s+",
                         QRegularExpression::UseUnicodePropertiesOption),
      "-");
}
QString GitHubClient::repositoryFromRemote(const QString &value) {
  QString path;
  const auto text = value.trimmed();
  const auto scp = QRegularExpression("^(?:[^/@:]+@)?github\\.com:([^?#]+)$",
                                      QRegularExpression::CaseInsensitiveOption)
                       .match(text);
  if (scp.hasMatch())
    path = scp.captured(1);
  else {
    const QUrl url(text);
    if (!QStringList{"https", "ssh", "git"}.contains(url.scheme()) ||
        url.host().compare("github.com", Qt::CaseInsensitive) != 0 ||
        url.hasQuery() || url.hasFragment())
      return {};
    path = url.path();
  }
  while (path.startsWith('/'))
    path.remove(0, 1);
  while (path.endsWith('/'))
    path.chop(1);
  if (path.endsWith(".git", Qt::CaseInsensitive))
    path.chop(4);
  const auto parts = path.split('/');
  if (parts.size() != 2)
    return {};
  for (const auto &part : parts)
    if (!QRegularExpression("^[A-Za-z0-9_.-]+$").match(part).hasMatch() ||
        part == "." || part == "..")
      return {};
  return path;
}
void GitHubClient::uploadReleaseAsset(const QString &repository,
                                      qint64 releaseId, const QString &file,
                                      Reply callback) {
  if (repositoryFromRemote("https://github.com/" + repository) != repository ||
      releaseId <= 0 || !QFileInfo(file).isFile()) {
    callback({}, "Dépôt, release ou fichier invalide.");
    return;
  }
  const auto program = executable();
  if (program.isEmpty()) {
    callback({}, "Connectez le compte GitHub avant d’envoyer un fichier.");
    return;
  }
  const auto endpoint =
      "https://uploads.github.com/repos/" + repository + "/releases/" +
      QString::number(releaseId) + "/assets?name=" +
      QString::fromLatin1(QUrl::toPercentEncoding(QFileInfo(file).fileName()));
  runner.setTimeoutMilliseconds(1800000);
  runner.run(
      program,
      {"api", "--hostname", "github.com", endpoint, "--method", "POST",
       "--header", "Content-Type: application/octet-stream", "--header",
       "Content-Length: " + QString::number(QFileInfo(file).size()), "--input",
       QFileInfo(file).absoluteFilePath()},
      {}, [callback](const CommandResult &r) {
        if (!r.ok()) {
          auto error =
              QJsonDocument::fromJson(r.output).object()["message"].toString();
          const auto original = r.message().right(6000);
          callback({}, error.isEmpty() || original.contains(error) ? original : error + "\n" + original);
          return;
        }
        auto object = QJsonDocument::fromJson(r.output).object();
        if (object["id"].toDouble() <= 0) {
          callback({}, "GitHub n’a pas confirmé le transfert du fichier.");
          return;
        }
        callback(object, {});
      });
  runner.setTimeoutMilliseconds(0);
}
