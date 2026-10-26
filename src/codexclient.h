#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QHash>
#include <QTimer>
#include <QTemporaryDir>
#include <QUrl>
#include <functional>

class CodexClient : public QObject {
    Q_OBJECT
  public:
    explicit CodexClient(QObject *parent = nullptr, const QString &program = {});
    ~CodexClient() override;
    void checkAccount();
    void login(bool deviceCode = false);
    void cancelLogin();
    void logout();
    void generateCommit(const QString &diff, const QString &language = "français");
    void cancelGeneration();
    void resetTransport();
    bool generating() const {
        return inGeneration;
    }
  signals:
    void accountChanged(const QJsonObject &account);
    void loginStarted(const QUrl &url, const QString &code);
    void loginFinished(bool success, const QString &error);
    void messageGenerated(const QString &summary, const QString &description);
    void generationChanged(bool active);
    void progressDetail(const QString &text);
    void errorOccurred(const QString &error);

  private:
    using Reply = std::function<void(QJsonObject, QString)>;
    QProcess process;
    QTemporaryDir workspace;
    QTimer deadline;
    QHash<int, Reply> replies;
    QList<std::function<void()>> waiting;
    QByteArray buffer;
    QString program;
    QString loginId;
    QString threadId;
    QString turnId;
    QString finalText;
    int nextId = 0;
    int generation = 0;
    int authentication = 0;
    bool initialized = false;
    bool inGeneration = false;
    bool closing = false;
    bool transportFailed = false;
    void ready(std::function<void()> callback);
    void request(const QString &method, const QJsonObject &params, Reply callback);
    void send(const QJsonObject &object);
    void receive(const QJsonObject &object);
    void failGeneration(const QString &message);
    void failed(const QString &message);
};
