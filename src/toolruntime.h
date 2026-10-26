#pragma once
#include "commandrunner.h"
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <memory>

namespace ToolRuntime {
QString executable(const QString &name);
void setExecutable(const QString &name, const QString &path);
} // namespace ToolRuntime

class RuntimeInstaller : public QObject {
    Q_OBJECT
  public:
    explicit RuntimeInstaller(QObject *parent = nullptr);
    void install(const QString &tool);
    bool busy() const {
        return active;
    }
  signals:
    void progress(const QString &message);
    void finished(const QString &path, const QString &error);

  private:
    QNetworkAccessManager network;
    CommandRunner runner;
    std::unique_ptr<QTemporaryDir> temporary;
    bool active = false;
    QString tool;
    void fetch(const QUrl &url, qint64 maximum, std::function<void(QByteArray, QString)> callback);
    void fail(const QString &message);
};
