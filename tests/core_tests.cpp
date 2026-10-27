#include "commandrunner.h"
#include "gitstatus.h"
#include "theme.h"
#include <QApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

static void check(bool condition, const QString &message) {
    if (!condition) { QTextStream(stderr) << "FAIL: " << message << '\n'; std::exit(1); }
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("github-enhanced-tests");
    CommandRunner runner;
    auto execute = [&](const QString &program, const QStringList &args, const QString &cwd) {
        QEventLoop loop; CommandResult result;
        bool returned = false;
        QTimer watchdog; watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, [&] {
            check(false, "Command callback did not arrive: " + program + " " + args.join(' '));
        });
        watchdog.start(30000);
        runner.run(program, args, cwd, [&](const CommandResult &r) {
            check(returned, "Command callback must be asynchronous: " + program);
            result = r; loop.quit();
        });
        check(runner.busy(), "Runner is busy immediately after enqueueing");
        returned = true;
        loop.exec(); return result;
    };
    QTemporaryDir temporary; check(temporary.isValid(), "Temporary directory");
    const auto root = temporary.path();
    auto git = [&](const QStringList &args) {
        auto r = execute("git", args, root); check(r.ok(), args.join(' ') + ": " + r.message()); return r;
    };
    auto write = [&](const QString &name, const QByteArray &content) {
        QFile f(QDir(root).filePath(name)); check(f.open(QIODevice::WriteOnly), name); check(f.write(content) == content.size(), "write " + name);
    };
    git({"init", "--initial-branch=main"});
    git({"config", "user.name", "Integration Test"}); git({"config", "user.email", "test@example.invalid"});
    git({"config", "commit.gpgsign", "false"}); git({"config", "core.hooksPath", root + "/no-hooks"});
#ifdef Q_OS_WIN
    const QString odd = "- [é] fichier avec espace.txt";
#else
    const QString odd = "- [é] fichier\navec tab\t.txt";
#endif
    write(odd, "first\n"); write("ordinary.txt", "hello\n");
    auto status = parseStatus(git({"status", "--porcelain=v1", "-z", "--untracked-files=all"}).output);
    check(status.size() == 2, "Two untracked files");
    check(status[0].path == odd && status[0].untracked(), "Preserve newlines, Unicode, tabs, leading dash and glob characters");
    git({"add", "--", odd});
    auto indexed = parseStatus(git({"status", "--porcelain=v1", "-z"}).output);
    check(indexed[0].staged(), "Staged status");
    git({"rm", "--cached", "--ignore-unmatch", "--", odd});
    check(QFile::exists(QDir(root).filePath(odd)), "Unstage unborn repository preserves file");
    git({"add", "--all", "--", odd, "ordinary.txt"});
    git({"commit", "-m", "Initial commit"});
    git({"mv", "--", "ordinary.txt", "renamed.txt"});
    auto renamed = parseStatus(git({"status", "--porcelain=v1", "-z"}).output);
    check(renamed.size() == 1 && renamed[0].path == "renamed.txt" && renamed[0].originalPath == "ordinary.txt", "Rename destination/source order");
    git({"restore", "--staged", "--", "renamed.txt", "ordinary.txt"});
    check(QFile::exists(root + "/renamed.txt"), "Unstage rename preserves file");
    git({"add", "--all", "--", "renamed.txt", "ordinary.txt"}); git({"commit", "-m", "Rename"});
    write(odd, "first\nsecond\n");
    auto diff = git({"diff", "--no-ext-diff", "--no-textconv", "--", odd});
    check(diff.output.contains("+second"), "Literal pathspec diff");
    git({"add", "--", odd});
    check(git({"diff", "--cached", "--", odd}).output.contains("+second"), "Staged diff");
    git({"commit", "-m", "Change"});
    git({"switch", "-c", "feature/test", "--"}); git({"switch", "--", "main"});
    git({"checkout", "--detach"});
    auto detached = execute("git", {"symbolic-ref", "--quiet", "--short", "HEAD"}, root);
    check(!detached.ok(), "Detached HEAD handled as nonfatal");
    git({"switch", "--", "main"});
    const QString remote = root + "/remote.git";
    git({"init", "--bare", remote}); git({"remote", "add", "origin", remote});
    git({"--git-dir", remote, "symbolic-ref", "HEAD", "refs/heads/main"});
    git({"push", "--set-upstream", "origin", "HEAD"}); git({"fetch", "--prune"}); git({"pull", "--ff-only"});
    git({"clone", "--", remote, root + "/clone"});
    auto failed = execute("executable-that-does-not-exist-ge", {}, root);
    check(!failed.ok() && !failed.message().isEmpty(), "Missing executable reports error without deadlock");
    QStringList order; QEventLoop loop;
    runner.run("git", {"--version"}, root, [&](const CommandResult &r) {
        check(r.ok(), "Queue job one"); order << "one";
        runner.run("git", {"--version"}, root, [&](const CommandResult &nested) { check(nested.ok(), "Nested queue job"); order << "three"; loop.quit(); });
    });
    runner.run("git", {"--version"}, root, [&](const CommandResult &r) { check(r.ok(), "Queue job two"); order << "two"; });
    loop.exec(); check(order == QStringList{"one", "two", "three"}, "FIFO with callback-enqueued command");
    QJsonObject theme{{"version", 1}, {"id", "custom"}, {"name", "Custom"}, {"author", "Test"}};
    for (const auto &key : {"background", "surface", "text", "muted", "accent", "border", "added", "removed"}) theme[key] = "#aabbcc";
    check(Theme::validate(theme), "Valid theme accepted");
    theme["id"] = "../../escape"; check(!Theme::validate(theme), "Path traversal rejected");
    theme["id"] = "custom"; theme["accent"] = "red; image: url(secret)";
    check(!Theme::validate(theme), "Stylesheet injection rejected");
    QTextStream(stdout) << "PASS: Git operations, literal paths, rename, unborn index, detached HEAD, local remotes, command queue, theme validation\n";
    return 0;
}
