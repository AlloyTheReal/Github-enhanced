#pragma once
#include "visuals.h"
#include <QTimer>
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
  bool running() const { return active; }

private:
  QLabel *titleLabel, *detail;
  QProgressBar *progress;
  QPlainTextEdit *details;
  QPushButton *closeButton;
  bool active = false;
  int completionRevision = 0;
  QString pending;
  QTimer reveal;
};
