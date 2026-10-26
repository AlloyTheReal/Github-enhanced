#include "commandrunner.h"
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

CommandRunner::CommandRunner(QObject *parent) : QObject(parent) {
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] {
        result.error += "\nOperation interrupted: time limit exceeded.";
        process.kill();
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, &CommandRunner::drain);
    connect(&process, &QProcess::readyReadStandardError, this, &CommandRunner::drain);
    connect(&process, &QProcess::started, this, [this] {
        if (!current.input.isEmpty()) process.write(current.input);
        process.closeWriteChannel();
    });
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        drain();
        finish(status == QProcess::NormalExit ? code : -1);
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            result.error += process.errorString().toUtf8();
            finish(-1);
        }
    });
}

CommandRunner::~CommandRunner() {
    stopping = true;
    process.disconnect(this);
    if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(2000); }
}

void CommandRunner::run(const QString &program, const QStringList &args, const QString &directory,
                        Callback callback, const QByteArray &input, qsizetype outputLimit, bool truncateOutput) {
    jobs.enqueue({program, args, directory, std::move(callback), input, outputLimit, truncateOutput, timeoutOverride});
    if (!active) next();
}

void CommandRunner::next() {
    if (stopping || active) return;
    if (jobs.isEmpty()) { emit busyChanged(false); return; }
    active = true;
    current = jobs.dequeue();
    result = {};
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &name : removedEnvironment) env.remove(name);
    const auto lfsPath = QSettings().value("tools/git-lfs").toString();
    if (!lfsPath.isEmpty()) env.insert("PATH", QFileInfo(lfsPath).absolutePath() + QDir::listSeparator() + env.value("PATH"));
    env.insert("PATH", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tools" + QDir::listSeparator() + env.value("PATH"));
    env.insert("GIT_TERMINAL_PROMPT", "0");
    env.insert("GCM_INTERACTIVE", "Never");
    env.insert("GIT_PAGER", "cat");
    env.insert("GH_PROMPT_DISABLED", "1");
    // App-issued Git pathspecs are literal. Git LFS needs its own internal glob
    // semantics for checkout, so this override must not leak into that component.
    if (removedEnvironment.contains("GIT_LITERAL_PATHSPECS") || QFileInfo(current.program).baseName() == "git-lfs")
        env.remove("GIT_LITERAL_PATHSPECS");
    else
        env.insert("GIT_LITERAL_PATHSPECS", "1");
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(current.directory);
    emit busyChanged(true);
    emit commandStarted(current.program, current.args);
    process.start(current.program, current.args);
    timeout.start(current.timeoutMilliseconds > 0 ? current.timeoutMilliseconds : current.args.contains("clone") || current.args.contains("push") || current.args.contains("fetch") || current.args.contains("pull") || current.args.contains("add") ? 1800000 : 120000);
}

void CommandRunner::drain() {
    const qsizetype limit = current.outputLimit;
    auto out = process.readAllStandardOutput();
    auto err = process.readAllStandardError();
    if (!err.isEmpty()) emit outputReceived(QString::fromUtf8(err));
    if (current.truncateOutput) {
        const auto available = qMax(qsizetype(0), limit - result.output.size());
        result.output += out.left(available);
        result.truncated = result.truncated || out.size() > available;
        result.error += err.left(qMax(qsizetype(0), qsizetype(65536) - result.error.size()));
        return;
    }
    if (result.output.size() + result.error.size() + out.size() + err.size() > limit) {
        result.error += "\nOutput limit exceeded (16 MiB).";
        process.kill();
        return;
    }
    result.output += out;
    result.error += err;
}

void CommandRunner::finish(int code) {
    if (!active || stopping) return;
    timeout.stop();
    result.code = code;
    auto callback = std::move(current.callback);
    // Keep active true while the callback queues follow-up work.
    if (callback) callback(result);
    active = false;
    QTimer::singleShot(0, this, &CommandRunner::next);
}
