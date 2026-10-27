#pragma once
#include "appupdater.h"
#include "diagnostics.h"
#include "visuals.h"
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class UpdateDialog : public GlassDialog {
  Q_OBJECT
public:
  UpdateDialog(AppUpdater *service, QWidget *parent = nullptr);
signals:
  void installRequested(const QString &file);

private:
  AppUpdater *service;
  DiagnosticLabel *status;
  QPlainTextEdit *notes;
  QProgressBar *progress;
  QPushButton *checkButton, *downloadButton, *installButton;
  void display(const AppUpdate &update, const QString &error);
};
