#include "pushpreflight.h"
#include <QtCore>
namespace {
constexpr qint64 threshold = 50LL * 1024 * 1024;
}
PushPreflight::PushPreflight(QObject *parent)
    : QObject(parent), runner(this), installer(this) {
  runner.setTimeoutMilliseconds(120000);
  runner.setEnvironmentRemovals({"GH_TOKEN", "GITHUB_TOKEN",
                                 "GH_ENTERPRISE_TOKEN",
                                 "GITHUB_ENTERPRISE_TOKEN", "GIT_LITERAL_PATHSPECS"});
  connect(&runner, &CommandRunner::outputReceived, this,
          &PushPreflight::output);
  connect(&installer, &RuntimeInstaller::progress, this,
          &PushPreflight::detail);
  connect(&installer, &RuntimeInstaller::finished, this,
          [this](const QString &, const QString &error) {
            if (error.isEmpty())
              startConversion();
            else
              finishConversion({}, error);
          });
}
void PushPreflight::inspect(const QString &repository, Checked callback) {
  if (active) {
    callback({}, "Une vérification d’envoi est déjà en cours.");
    return;
  }
  active = true;
  inspection = {};
  inspection.repository = repository;
  checked = std::move(callback);
  emit busyChanged(true);
  emit detail("Vérification des références distantes avant l’envoi…");
  runner.run(
      "git", {"fetch", "--all", "--prune", "--no-recurse-submodules"},
      repository, [this](const CommandResult &r) {
        if (!r.ok()) {
          finishInspection(
              "Impossible de vérifier les dépôts distants. Vérifiez le réseau "
              "et l’authentification avant l’envoi.");
          return;
        }
        runner.run(
            "git", {"symbolic-ref", "--quiet", "HEAD"}, inspection.repository,
            [this](const CommandResult &branch) {
              if (!branch.ok()) {
                finishInspection("Sélectionnez une branche avant l’envoi.");
                return;
              }
              inspection.branchRef = QString::fromUtf8(branch.output).trimmed();
              runner.run(
                  "git", {"rev-parse", "--verify", "HEAD"},
                  inspection.repository, [this](const CommandResult &head) {
                    if (!head.ok()) {
                      finishInspection(
                          "Créez un premier commit avant l’envoi.");
                      return;
                    }
                    inspection.head = QString::fromUtf8(head.output).trimmed();
                    collectRefs();
                  });
            });
      });
}
void PushPreflight::collectRefs() {
  runner.run("git", {"for-each-ref", "--format=%(refname)", "refs/remotes/"},
             inspection.repository, [this](const CommandResult &r) {
               if (!r.ok()) {
                 finishInspection(r.message());
                 return;
               }
               inspection.remoteRefs =
                   QString::fromUtf8(r.output).split('\n', Qt::SkipEmptyParts);
               scan();
             });
}
void PushPreflight::scan() {
  emit detail("Analyse des fichiers présents dans les commits non publiés…");
  runner.run(
      "git",
      {"rev-list", "--objects", inspection.head, "--not", "--remotes", "--"},
      inspection.repository, [this](const CommandResult &r) {
        if (!r.ok()) {
          finishInspection("Analyse des objets Git impossible : " +
                           r.message());
          return;
        }
        QByteArray ids;
        QHash<QString, QString> paths;
        for (const auto &line : r.output.split('\n')) {
          int separator = line.indexOf(' ');
          const auto id = separator < 0 ? line : line.left(separator);
          if (id.isEmpty())
            continue;
          if (!QRegularExpression("^[a-fA-F0-9]{40,64}$")
                   .match(QString::fromLatin1(id))
                   .hasMatch()) {
            finishInspection("Liste d’objets Git invalide.");
            return;
          }
          ids += id + '\n';
          if (separator >= 0)
            paths.insert(QString::fromLatin1(id),
                         QString::fromUtf8(line.mid(separator + 1)));
        }
        if (ids.isEmpty()) {
          finishInspection();
          return;
        }
        runner.run(
            "git",
            {"cat-file",
             "--batch-check=%(objectname) %(objecttype) %(objectsize)"},
            inspection.repository,
            [this, paths](const CommandResult &sizes) {
              if (!sizes.ok()) {
                finishInspection(sizes.message());
                return;
              }
              for (const auto &line : sizes.output.split('\n')) {
                if (line.isEmpty())
                  continue;
                auto parts = line.split(' ');
                bool ok = false;
                auto bytes = parts.value(2).toLongLong(&ok);
                if (parts.size() != 3 || !ok) {
                  finishInspection("Taille d’objet Git inconnue. L’envoi n’a "
                                   "pas été lancé.");
                  return;
                }
                if (parts[1] == "blob" && bytes >= threshold) {
                  const auto id = QString::fromLatin1(parts[0]);
                  inspection.largeObjects.append(
                      {id, paths.value(id, id), bytes});
                }
              }
              finishInspection();
            },
            ids);
      });
}
void PushPreflight::finishInspection(const QString &error) {
  active = false;
  auto callback = std::move(checked);
  checked = {};
  emit busyChanged(false);
  if (callback)
    callback(inspection, error);
}
void PushPreflight::convert(const PushInspection &reviewed,
                            Converted callback) {
  if (active) {
    callback({}, {}, "Une opération LFS est déjà en cours.");
    return;
  }
  if (reviewed.largeObjects.isEmpty() || reviewed.head.isEmpty() ||
      !reviewed.branchRef.startsWith("refs/heads/")) {
    callback({}, {}, "Aucun commit à convertir.");
    return;
  }
  // Re-fetch/re-scan: a commit may have been published after the review dialog
  // opened.
  inspect(reviewed.repository, [this, reviewed, callback](PushInspection fresh,
                                                          QString error) {
    if (!error.isEmpty()) {
      callback({}, {}, error);
      return;
    }
    if (fresh.head != reviewed.head || fresh.branchRef != reviewed.branchRef) {
      callback({}, {},
               "La branche ou les commits ont changé. Relancez l’envoi.");
      return;
    }
    if (fresh.largeObjects.isEmpty()) {
      callback({}, {},
               "Ces gros objets ne sont plus dans les commits non publiés. "
               "Relancez l’envoi.");
      return;
    }
    inspection = fresh;
    converted = callback;
    backup.clear();
    active = true;
    emit busyChanged(true);
    runner.run("git",
               {"status", "--porcelain=v1", "-z", "--untracked-files=all"},
               inspection.repository, [this](const CommandResult &status) {
                 if (!status.ok() || !status.output.isEmpty()) {
                   finishConversion(
                       {}, "Commitez ou mettez de côté les modifications "
                           "locales avant de convertir les commits en LFS.");
                   return;
                 }
                 if (ToolRuntime::executable("git-lfs").isEmpty())
                   installer.install("git-lfs");
                 else
                   startConversion();
               });
  });
}
void PushPreflight::startConversion() {
  emit detail("Préparation de Git LFS et de la sauvegarde locale…");
  if (QFileInfo(QDir(inspection.repository).filePath(".gitattributes"))
          .isSymLink()) {
    finishConversion(
        {},
        ".gitattributes doit être un fichier local, pas un lien symbolique.");
    return;
  }
  runner.run(
      ToolRuntime::executable("git-lfs"), {"install", "--local"},
      inspection.repository, [this](const CommandResult &installed) {
        if (!installed.ok()) {
          finishConversion({}, "Configuration LFS impossible : " +
                                   installed.message());
          return;
        }
        runner.run(
            "git", {"status", "--porcelain=v1", "-z", "--untracked-files=all"},
            inspection.repository, [this](const CommandResult &status) {
              if (!status.ok() || !status.output.isEmpty()) {
                finishConversion({},
                                 "Les fichiers ont changé. Conversion annulée "
                                 "pour préserver les modifications locales.");
                return;
              }
              runner.run(
                  "git", {"rev-parse", "--verify", inspection.branchRef},
                  inspection.repository, [this](const CommandResult &head) {
                    if (!head.ok() ||
                        QString::fromUtf8(head.output).trimmed() !=
                            inspection.head) {
                      finishConversion(
                          {}, "La branche a changé. Relancez l’envoi.");
                      return;
                    }
                    backup =
                        "refs/github-enhanced/lfs-backups/" +
                        QString::number(QDateTime::currentMSecsSinceEpoch()) +
                        "-" + inspection.head.left(12);
                    runner.run(
                        "git",
                        {"update-ref", "--create-reflog", backup,
                         inspection.head, QString(inspection.head.size(), '0')},
                        inspection.repository,
                        [this](const CommandResult &saved) {
                          if (!saved.ok())
                            finishConversion({}, "Sauvegarde impossible : " +
                                                     saved.message());
                          else
                            snapshotReferences();
                        });
                  });
            });
      });
}
void PushPreflight::migrate() {
  runner.run("git", {"symbolic-ref", "--quiet", "HEAD"}, inspection.repository,
             [this](const CommandResult &branch) {
               if (!branch.ok() || QString::fromUtf8(branch.output).trimmed() !=
                                       inspection.branchRef) {
                 finishConversion(
                     {}, "La branche active a changé. Conversion annulée.");
                 return;
               }
               performMigration();
             });
}
void PushPreflight::snapshotReferences() {
  runner.run(
      "git",
      {"for-each-ref", "--format=%(refname)%00%(objectname)%00%(symref)"},
      inspection.repository, [this](const CommandResult &refs) {
        if (!refs.ok()) {
          finishConversion(
              {}, "Lecture des références avant conversion impossible.");
          return;
        }
        originalRefs.clear();
        for (const auto &line : refs.output.split('\n')) {
          auto fields = line.split('\0');
          if (fields.size() == 3 && fields[2].isEmpty())
            originalRefs.insert(QString::fromUtf8(fields[0]),
                                QString::fromLatin1(fields[1]));
        }
        runner.run(
            "git", {"rev-parse", "--absolute-git-dir"}, inspection.repository,
            [this](const CommandResult &dir) {
              if (!dir.ok()) {
                finishConversion({}, "Dossier de sauvegarde Git introuvable.");
                return;
              }
              recoveryDirectory = std::make_unique<QTemporaryDir>(
                  QString::fromUtf8(dir.output).trimmed() +
                  "/github-enhanced-lfs-XXXXXX");
              if (!recoveryDirectory->isValid()) {
                finishConversion(
                    {}, "Impossible de créer la sauvegarde des références.");
                return;
              }
              recoveryDirectory->setAutoRemove(false);
              QJsonObject refsJson;
              for (auto it = originalRefs.begin(); it != originalRefs.end();
                   ++it)
                refsJson[it.key()] = it.value();
              QSaveFile saved(recoveryDirectory->filePath("recovery.json"));
              auto bytes =
                  QJsonDocument(QJsonObject{{"branch", inspection.branchRef},
                                            {"head", inspection.head},
                                            {"backup", backup},
                                            {"refs", refsJson}})
                      .toJson();
              if (!saved.open(QIODevice::WriteOnly) ||
                  saved.write(bytes) != bytes.size() || !saved.commit()) {
                finishConversion(
                    {}, "Écriture de la sauvegarde des références impossible.");
                return;
              }
              migrate();
            });
      });
}
void PushPreflight::performMigration() {
  emit detail("Conversion des gros fichiers dans les commits non publiés…");
  QStringList args{"migrate", "import", "--above=52428799b",
                   "--include-ref=" + inspection.branchRef,
                   "--object-map=" +
                       recoveryDirectory->filePath("objects.csv")};
  for (const auto &ref : inspection.remoteRefs)
    args << "--exclude-ref=" + ref;
  // No --everything, --yes, --no-rewrite or force push. A clean worktree is
  // required.
  runner.setTimeoutMilliseconds(1800000);
  runner.run(ToolRuntime::executable("git-lfs"), args, inspection.repository,
             [this](const CommandResult &migrated) {
               migrationError =
                   migrated.ok()
                       ? QString()
                       : "Conversion LFS interrompue : " + migrated.message();
               restoreReferences();
             });
}
void PushPreflight::restoreReferences() {
  // git-lfs migrate may also remap refs pointing to a rewritten commit.
  // Preserve unrelated refs and the backup with its object map and
  // compare-and-swap updates.
  rewritten.clear();
  QFile map(recoveryDirectory->filePath("objects.csv"));
  if (map.open(QIODevice::ReadOnly))
    for (const auto &line : map.readAll().split('\n')) {
      auto fields = line.trimmed().split(',');
      if (fields.size() == 2)
        rewritten.insert(QString::fromLatin1(fields[0]),
                         QString::fromLatin1(fields[1]));
    }
  restorationError.clear();
  restoreJobs.clear();
  runner.run(
      "git",
      {"for-each-ref", "--format=%(refname)%00%(objectname)%00%(symref)"},
      inspection.repository, [this](const CommandResult &refs) {
        if (!refs.ok()) {
          finishConversion(
              {}, "Vérification des références impossible. Sauvegarde : " +
                      recoveryDirectory->path());
          return;
        }
        QHash<QString, QString> current;
        QSet<QString> aliases;
        for (const auto &line : refs.output.split('\n')) {
          auto fields = line.split('\0');
          if (fields.size() == 3) {
            auto name = QString::fromUtf8(fields[0]);
            current.insert(name, QString::fromLatin1(fields[1]));
            if (!fields[2].isEmpty())
              aliases.insert(name);
          }
        }
        for (auto it = originalRefs.begin(); it != originalRefs.end(); ++it) {
          if (it.key() == inspection.branchRef ||
              current.value(it.key()) == it.value())
            continue;
          if (aliases.contains(it.key()) ||
              (it.key() != backup &&
               rewritten.value(it.value()) != current.value(it.key()))) {
            restorationError =
                "Une référence a changé pendant la conversion : " + it.key() +
                ". Aucune référence concurrente n’a été écrasée.";
            continue;
          }
          restoreJobs.append(
              {it.key(), it.value(),
               current.value(it.key(), QString(inspection.head.size(), '0'))});
        }
        restoreNext();
      });
}
void PushPreflight::restoreNext() {
  if (restoreJobs.isEmpty()) {
    if (!migrationError.isEmpty() || !restorationError.isEmpty()) {
      finishConversion(
          {}, migrationError + '\n' + restorationError +
                  "\nSauvegarde des références : " + recoveryDirectory->path());
      return;
    }
    hydrateAndVerify();
    return;
  }
  const auto job = restoreJobs.takeFirst();
  runner.run("git", {"update-ref", "--no-deref", job[0], job[1], job[2]},
             inspection.repository, [this, job](const CommandResult &restored) {
               if (!restored.ok())
                 restorationError =
                     "Restauration de référence impossible : " + job[0] +
                     ". Sauvegarde conservée dans " + recoveryDirectory->path();
               restoreNext();
             });
}
void PushPreflight::hydrateAndVerify() {
  runner.run(
      ToolRuntime::executable("git-lfs"), {"checkout"}, inspection.repository,
      [this](const CommandResult &checkout) {
        if (!checkout.ok()) {
          finishConversion({}, "Conversion effectuée, mais les fichiers de "
                               "travail n’ont pas été restaurés : " +
                                   checkout.message());
          return;
        }
        runner.run(
            ToolRuntime::executable("git-lfs"), {"fsck", "--objects", "HEAD"},
            inspection.repository, [this](const CommandResult &check) {
              if (!check.ok()) {
                finishConversion({}, "Vérification des objets LFS échouée : " +
                                         check.message());
                return;
              }
              runner.run("git", {"rev-parse", "--verify", "HEAD"},
                         inspection.repository,
                         [this](const CommandResult &head) {
                           if (!head.ok())
                             finishConversion({}, head.message());
                           else
                             finishConversion(
                                 QString::fromUtf8(head.output).trimmed());
                         });
            });
      });
}
void PushPreflight::finishConversion(const QString &newHead,
                                     const QString &error) {
  active = false;
  runner.setTimeoutMilliseconds(120000);
  auto callback = std::move(converted);
  converted = {};
  emit busyChanged(false);
  if (callback)
    callback(newHead, backup, error);
}
