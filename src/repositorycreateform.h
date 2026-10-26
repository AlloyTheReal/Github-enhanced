#pragma once
#include "githubclient.h"
#include "visuals.h"
#include <QSet>
#include <QWidget>
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QProgressBar;
class RepositoryCreateForm : public QWidget {
  Q_OBJECT
public:
  explicit RepositoryCreateForm(GitHubClient *client,
                                QWidget *parent = nullptr);
  void focusName();
  void configurePublication(const QString &name, bool hasCommits);
  bool sendExistingCommits() const;
  bool busy() const { return working; }
signals:
  void created(const QJsonObject &repository);
  void cancelled();

protected:
  void showEvent(QShowEvent *event) override;

private:
  GitHubClient *client;
  QLineEdit *name, *description;
  QComboBox *visibility, *owner;
  QCheckBox *readme, *pushCommits;
  QLabel *account, *status, *heading, *hint, *ownerHint;
  QPushButton *submit, *back, *login;
  QProgressBar *progress;
  bool working = false, authenticated = false, publishing = false;
  int accountRevision = 0;
  QSet<QString> organizations;
  void loadAccount();
  void validate();
  void loadOrganizations(int page, int revision);
  void updateOwner();
  void create();
  void setWorking(bool active);
};

class RepositoryPublishDialog : public GlassDialog {
  Q_OBJECT
public:
  RepositoryPublishDialog(GitHubClient *client, const QString &name,
                          bool hasCommits, QWidget *parent = nullptr);
  QJsonObject repository() const { return result; }
  bool sendExistingCommits() const;

protected:
  void done(int result) override;

private:
  RepositoryCreateForm *form;
  QJsonObject result;
};
