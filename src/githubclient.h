#pragma once
#include "commandrunner.h"
#include <QJsonArray>
#include <QJsonObject>

class GitHubClient : public QObject {
  Q_OBJECT
public:
  using Reply = std::function<void(QJsonValue, QString)>;
  explicit GitHubClient(QObject *parent = nullptr, const QString &program = {});
  ~GitHubClient() override;
  bool busy() const {
    return runner.busy() || auth.state() != QProcess::NotRunning;
  }
  void checkAccount();
  void login();
  void authorizeWorkflows();
  static bool workflowPermissionMissing(const QString &message);
  void cancelLogin();
  void logout();
  void repositories(int page = 1);
  void createRepository(const QString &name, const QString &description,
                        bool privateRepository, Reply callback,
                        bool initializeReadme = false,
                        const QString &organization = {});
  static QString normalizedRepositoryName(const QString &name);
  static QString repositoryFromRemote(const QString &url);
  void uploadReleaseAsset(const QString &repository, qint64 releaseId,
                          const QString &file, Reply callback);
  void api(const QString &endpoint, const QString &method,
           const QJsonObject &payload, Reply callback);
  QJsonObject account() const { return currentAccount; }
signals:
  void busyChanged(bool busy);
  void operationOutput(const QString &text);
  void accountChanged(const QJsonObject &account);
  void authOutput(const QString &text);
  void loginFinished(bool success, const QString &error);
  void repositoriesLoaded(const QJsonArray &repositories, int page,
                          bool hasMore);
  void errorOccurred(const QString &message);

private:
  CommandRunner runner;
  QProcess auth;
  QTimer authDeadline;
  QString program;
  QJsonObject currentAccount;
  bool closing = false;
  QString executable() const;
  void startAuthentication(bool workflowsOnly);
};
