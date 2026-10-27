#pragma once
#include "commandrunner.h"
#include "diagnostics.h"
#include "visuals.h"
class QComboBox;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class MergeDialog : public GlassDialog {
  Q_OBJECT
public:
  MergeDialog(const QString &repository, const QString &preferredTarget = {},
              QWidget *parent = nullptr);

protected:
  void done(int result) override;

private:
  QString path, branch, head, mergeHead;
  CommandRunner runner;
  bool working = false, loading = false;
  QComboBox *target;
  QLabel *heading;
  DiagnosticLabel *status;
  QListWidget *conflicts;
  QPlainTextEdit *preview;
  QPushButton *mergeButton, *finishButton, *abortButton, *ours, *theirs,
      *edited, *open;
  void run(const QStringList &args, CommandRunner::Callback callback);
  void load(const QString &error = {});
  void updateActions();
  void verify(bool clean, std::function<void()> callback);
  void merge();
  void resolve(const QString &choice);
  void previewTarget();
};
