#pragma once
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <functional>

struct CommandResult {
    int code = -1;
    QByteArray output;
    QByteArray error;
    bool truncated = false;
    bool ok() const { return code == 0; }
    QString message() const { return QString::fromUtf8(error.isEmpty() ? output : error).trimmed(); }
};

// Each queued job captures its own working directory. No command goes through a shell.
class CommandRunner : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const CommandResult &)>;
    explicit CommandRunner(QObject *parent = nullptr);
    ~CommandRunner() override;
    void run(const QString &program, const QStringList &args, const QString &directory,
             Callback callback, const QByteArray &input = {}, qsizetype outputLimit = 16 * 1024 * 1024, bool truncateOutput = false);
    bool busy() const { return active || !jobs.isEmpty(); }
    void setTimeoutMilliseconds(int milliseconds) { timeoutOverride = milliseconds; }
    void setEnvironmentRemovals(const QStringList &names) { removedEnvironment = names; }
signals:
    void busyChanged(bool busy);
    void commandStarted(const QString &program, const QStringList &arguments);
    void outputReceived(const QString &text);
private:
    struct Job { QString program; QStringList args; QString directory; Callback callback; QByteArray input; qsizetype outputLimit; bool truncateOutput; int timeoutMilliseconds; };
    QQueue<Job> jobs;
    QProcess process;
    QTimer timeout;
    Job current;
    CommandResult result;
    bool active = false;
    bool stopping = false;
    int timeoutOverride = 0;
    QStringList removedEnvironment;
    void next();
    void drain();
    void finish(int code);
};
