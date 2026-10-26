#pragma once
#include "diagnostics.h"
#include "visuals.h"
#include <QTimer>
#include <functional>
class QLabel;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class OperationDialog : public GlassDialog {
  Q_OBJECT
public:
  explicit OperationDialog(QWidget *parent = nullptr);
  void begin(const QString &title);
  void step(const QString &text);
  void append(const QString &text);
  void finish(bool success, const QString &message = {});
  void setRecoveryAction(const QString &title, std::function<void()> action);
  bool running() const { return active; }

private:
  QLabel *titleLabel, *detail, *hint;
  QCheckBox *advancedToggle;
  QProgressBar *progress;
  QPlainTextEdit *details;
  QPushButton *closeButton, *recoveryButton;
  std::function<void()> recovery;
  bool active = false;
  int completionRevision = 0;
  QString pending;
  QTimer reveal;
};
