#pragma once
#include "visuals.h"
#include "githubclient.h"
#include "codexclient.h"
#include "toolruntime.h"
class QLabel;
class QPushButton;
class QPlainTextEdit;

class AccountDialog : public GlassDialog {
    Q_OBJECT
  public:
    AccountDialog(const QString &provider, GitHubClient *github, CodexClient *codex,
                  QWidget *parent = nullptr);

  protected:
    void done(int result) override;

  private:
    QString provider;
    GitHubClient *github;
    CodexClient *codex;
    RuntimeInstaller installer;
    QLabel *status;
    QLabel *code;
    QPlainTextEdit *details;
    QPushButton *connectButton;
    QPushButton *browserButton;
    QPushButton *copyButton;
    QUrl url;
    bool connected = false;
    void start(bool device = false);
    void complete(bool success, const QString &error);
};
