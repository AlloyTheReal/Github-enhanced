#pragma once
#include "visuals.h"
#include "commandrunner.h"
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;

class RepoSettings : public GlassDialog {
    Q_OBJECT
  public:
    explicit RepoSettings(const QString &repository, const QJsonObject &githubAccount = {},
                          QWidget *parent = nullptr);
    static QString ignoreTemplate(const QString &name);

  private:
    QString path;
    CommandRunner runner;
    QLineEdit *name;
    QLineEdit *email;
    QLineEdit *origin;
    QPlainTextEdit *ignore;
    QLabel *status;
    QPushButton *save;
    QByteArray initialIgnore;
    bool hadOrigin = false;
    bool ignoreWritable = true;
    QStringList commands;
    QList<QStringList> jobs;
    void apply();
    void next();
};
