#include "repositorysync.h"
#include <QtCore>
RepositorySync::RepositorySync(QObject *parent)
    : QObject(parent), runner(this) {
  qRegisterMetaType<RepositorySyncState>();
  runner.setTimeoutMilliseconds(30000);
  runner.setEnvironmentRemovals({"GH_TOKEN", "GITHUB_TOKEN",
                                 "GH_ENTERPRISE_TOKEN",
                                 "GITHUB_ENTERPRISE_TOKEN"});
}
void RepositorySync::inspect(const QString &repository, bool fetch) {
  if (active) {
    pendingPath = repository;
    pendingFetch = pendingFetch || fetch;
    return;
  }
  if (repository.isEmpty())
    return;
  active = true;
  state = {};
  state.repository = repository;
  emit busyChanged(true);
  runner.run("git", {"symbolic-ref", "--quiet", "--short", "HEAD"}, repository,
             [this, fetch](const CommandResult &r) {
               if (!r.ok()) {
                 state.kind = "detached";
                 finish();
                 return;
               }
               state.branch = QString::fromUtf8(r.output).trimmed();
               runner.run(
                   "git", {"rev-parse", "--verify", "HEAD"}, state.repository,
                   [this, fetch](const CommandResult &head) {
                     if (!head.ok()) {
                       state.kind = "unborn";
                       finish();
                       return;
                     }
                     state.head = QString::fromUtf8(head.output).trimmed();
                     runner.run(
                         "git",
                         {"for-each-ref",
                          "--format=%(upstream:short)%00%(upstream:remotename)%"
                          "00%(upstream:remoteref)",
                          "refs/heads/" + state.branch},
                         state.repository,
                         [this, fetch](const CommandResult &refs) {
                           if (!refs.ok()) {
                             state.error =
                                 "Lecture de la branche distante impossible.";
                             finish();
                             return;
                           }
                           auto fields = refs.output.trimmed().split('\0');
                           state.upstream = QString::fromUtf8(fields.value(0));
                           state.remote = QString::fromUtf8(fields.value(1));
                           state.remoteRef = QString::fromUtf8(fields.value(2));
                           findRemote(fetch);
                         });
                   });
             });
}
void RepositorySync::findRemote(bool fetch) {
  runner.run(
      "git", {"remote"}, state.repository,
      [this, fetch](const CommandResult &r) {
        if (!r.ok()) {
          state.error = "Lecture des dépôts distants impossible.";
          finish();
          return;
        }
        const auto remotes =
            QString::fromUtf8(r.output).split('\n', Qt::SkipEmptyParts);
        if (state.remote.isEmpty())
          state.remote =
              remotes.contains("origin") ? "origin" : remotes.value(0);
        if (state.remote.isEmpty() || state.remote == ".") {
          state.kind = "localOnly";
          finish();
          return;
        }
        state.checkedAt = verified.value(key());
        if (!fetch) {
          state.error = failures.value(key());
          compare();
          return;
        }
        runner.run(
            "git",
            {"fetch", "--prune", "--no-recurse-submodules", "--", state.remote},
            state.repository, [this](const CommandResult &fetched) {
              if (!fetched.ok()) {
                // Raw stderr can contain credential-bearing URLs. Keep it out
                // of labels and logs.
                state.error =
                    "Impossible de vérifier le dépôt distant (réseau, accès ou "
                    "authentification). Réessayez avec Vérifier le distant.";
                failures.insert(key(), state.error);
                compare();
                return;
              }
              state.checkedAt = QDateTime::currentDateTime();
              verified.insert(key(), state.checkedAt);
              failures.remove(key());
              compare();
            });
      });
}
void RepositorySync::compare() {
  if (state.upstream.isEmpty()) {
    state.kind = "noUpstream";
    finish();
    return;
  }
  runner.run(
      "git",
      {"rev-list", "--left-right", "--count", "HEAD...@{upstream}", "--"},
      state.repository, [this](const CommandResult &r) {
        if (!r.ok()) {
          state.kind = "missingUpstream";
          if (state.error.isEmpty())
            state.error = "La branche distante suivie n’existe plus ou n’a pas "
                          "encore été récupérée.";
          finish();
          return;
        }
        auto values = QString::fromUtf8(r.output).simplified().split(' ');
        bool a = false, b = false;
        state.ahead = values.value(0).toInt(&a);
        state.behind = values.value(1).toInt(&b);
        if (!a || !b) {
          state.error = "Comparaison des commits impossible.";
          finish();
          return;
        }
        state.kind = state.ahead && state.behind ? "diverged"
                     : state.ahead               ? "ahead"
                     : state.behind              ? "behind"
                                                 : "synced";
        finish();
      });
}
void RepositorySync::finish() {
  active = false;
  emit updated(state);
  emit busyChanged(false);
  if (!pendingPath.isEmpty()) {
    const auto path = pendingPath;
    const bool fetch = pendingFetch;
    pendingPath.clear();
    pendingFetch = false;
    QTimer::singleShot(0, this, [this, path, fetch] { inspect(path, fetch); });
  }
}
