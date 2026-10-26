#pragma once
#include <QLabel>

struct ErrorExplanation {
  QString title, action;
  bool recognized = false;
  QString text() const {
    return action.isEmpty() ? title : title + "\n" + action;
  }
};

// Presentation only: raw service responses remain available to Git/API parsers.
class Diagnostics : public QObject {
  Q_OBJECT
public:
  static Diagnostics &instance();
  static ErrorExplanation explain(const QString &raw);
  static bool advanced();
  void setAdvanced(bool enabled);
  static void showError(QWidget *parent, const QString &title,
                        const QString &raw);
signals:
  void advancedChanged(bool enabled);
};

class DiagnosticLabel : public QLabel {
  Q_OBJECT
public:
  explicit DiagnosticLabel(QWidget *parent = nullptr);
  void setText(const QString &text);
  void clear() { setText({}); }
  void setError(const QString &raw);

private:
  QString error;
  void refresh();
};
