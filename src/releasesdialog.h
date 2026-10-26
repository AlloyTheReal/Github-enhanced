#pragma once
#include "diagnostics.h"
#include "githubclient.h"
#include "operationdialog.h"
#include "visuals.h"
#include <QSet>
class QLineEdit;
class QPlainTextEdit;
class QComboBox;
class QCheckBox;
class QListWidget;
class QTreeWidget;
class QLabel;
class QPushButton;
class ReleaseEditor : public GlassDialog {
  Q_OBJECT
public:
  ReleaseEditor(GitHubClient *client, const QString &repository,
                const QString &defaultBranch, const QJsonObject &release = {},
                QWidget *parent = nullptr);
signals:
  void saved(const QJsonObject &release);

protected:
  void done(int result) override;

private:
  GitHubClient *client;
  QString repository;
  QJsonObject current;
  QLineEdit *tag, *title, *target;
  QPlainTextEdit *notes;
  QCheckBox *draft, *prerelease;
  QListWidget *attachments;
  DiagnosticLabel *status;
  QPushButton *submit, *choose, *remove;
  OperationDialog *operation;
  QStringList pending;
  QSet<QString> uploaded;
  bool working = false, newRelease = true, desiredDraft = true;
  void save();
  void uploadNext();
  void finalize();
  void fail(const QString &error);
  void setWorking(bool active);
};
class ReleasesDialog : public GlassDialog {
  Q_OBJECT
public:
  ReleasesDialog(const QString &localPath, GitHubClient *client,
                 QWidget *parent = nullptr,
                 const QString &explicitRepository = {});

protected:
  void done(int result) override;

private:
  GitHubClient *client;
  CommandRunner locator;
  QString localPath, repository, defaultBranch;
  QComboBox *remotes;
  QLabel *repositoryLabel, *detail;
  DiagnosticLabel *status;
  QTreeWidget *list, *assets;
  QPlainTextEdit *notes;
  QPushButton *createButton, *editButton, *deleteButton, *addFiles,
      *removeAsset, *publishButton, *more, *reload;
  OperationDialog *operation;
  int revision = 0, page = 0;
  bool canWrite = false, working = false, hasMore = false;
  qint64 preferredRelease = 0;
  QString lastOperationError;
  QPushButton *downloadButton, *viewButton;
  QJsonObject selected() const;
  void discover();
  void selectRepository(const QString &name);
  void refresh(int requestedPage = 1);
  void selectRelease();
  void updateActions();
  void edit(bool create);
  void removeRelease();
  void publish();
  void uploadFiles();
  void uploadNext(QStringList files, const QString &repo, qint64 id);
  void deleteSelectedAsset();
  void setWorking(bool active, const QString &title = {});
  void finish(const QString &error = {});
};
