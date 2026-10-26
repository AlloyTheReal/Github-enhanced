#pragma once
#include "diagnostics.h"
#include "githubclient.h"
#include "visuals.h"
class QTreeWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class RepositoryCreateForm;

class RepoBrowser : public GlassDialog {
  Q_OBJECT
public:
  explicit RepoBrowser(GitHubClient *client, QWidget *parent = nullptr);
signals:
  void releasesRequested(const QString &repository);
  void cloneRequested(const QString &url, const QString &suggestedName);

private:
  GitHubClient *client;
  QStackedWidget *pages;
  RepositoryCreateForm *createForm = nullptr;

protected:
  void done(int result) override;

private:
  QTreeWidget *list;
  QLineEdit *search;
  DiagnosticLabel *status;
  QPushButton *more;
  QJsonArray repositories;
  int page = 0;
  void refresh();
  void filter();
  void create();
};
