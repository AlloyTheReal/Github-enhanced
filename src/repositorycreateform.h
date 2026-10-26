#pragma once
#include "githubclient.h"
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
  bool busy() const { return working; }
signals:
  void created(const QJsonObject &repository);
  void cancelled();

protected:
  void showEvent(QShowEvent *event) override;

private:
  GitHubClient *client;
  QLineEdit *name, *description;
  QComboBox *visibility;
  QCheckBox *readme;
  QLabel *account, *status;
  QPushButton *submit, *back, *login;
  QProgressBar *progress;
  bool working = false, authenticated = false;
  void loadAccount();
  void validate();
  void create();
  void setWorking(bool active);
};
