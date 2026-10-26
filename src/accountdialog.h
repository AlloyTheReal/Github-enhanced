#pragma once
#include "diagnostics.h"
#include "codexclient.h"
#include "githubclient.h"
#include "toolruntime.h"
#include "visuals.h"
class QLabel;
class QPushButton;
class QPlainTextEdit;

class AccountDialog : public GlassDialog {
  Q_OBJECT
public:
  AccountDialog(const QString &provider, GitHubClient *github,
                CodexClient *codex, QWidget *parent = nullptr,
                bool workflowsOnly = false);
  bool succeeded() const { return connected; }

protected:
  void done(int result) override;

private:
  QString provider;
  GitHubClient *github;
  CodexClient *codex;
  RuntimeInstaller installer;
  DiagnosticLabel *status;
  QLabel *code;
  QPlainTextEdit *details;
  QPushButton *connectButton;
  QPushButton *browserButton;
  QPushButton *copyButton;
  QUrl url;
  bool connected = false;
  bool workflowsOnly = false;
  void start(bool device = false);
  void complete(bool success, const QString &error);
};
