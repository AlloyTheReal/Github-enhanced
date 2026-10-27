#include "mergedialog.h"
#include <QtWidgets>
MergeDialog::MergeDialog(const QString &repository, const QString &preferred,
                         QWidget *parent)
    : GlassDialog(parent), path(repository), runner(this) {
  setObjectName("mergeDialog");
  setWindowTitle("Fusionner des branches");
  resize(760, 630);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(26, 26, 26, 22);
  root->setSpacing(14);
  heading = new QLabel("Fusionner dans la branche courante");
  heading->setObjectName("pageTitle");
  root->addWidget(heading);
  auto *hint =
      new QLabel("La fusion conserve les commits des deux branches. Elle ne "
                 "fait aucun envoi forcé. En cas de conflit, choisissez une "
                 "version ou modifiez le fichier avant de le marquer résolu.");
  hint->setWordWrap(true);
  hint->setObjectName("muted");
  root->addWidget(hint);
  target = new GlassComboBox;
  target->setObjectName("mergeTarget");
  target->setProperty("preferred", preferred);
  root->addWidget(target);
  status = new DiagnosticLabel;
  status->setObjectName("mergeStatus");
  root->addWidget(status);
  preview = new QPlainTextEdit;
  preview->setReadOnly(true);
  preview->setFrameShape(QFrame::NoFrame);
  preview->setObjectName("mergePreview");
  root->addWidget(preview, 1);
  conflicts = new QListWidget;
  conflicts->setObjectName("mergeConflicts");
  conflicts->setFrameShape(QFrame::NoFrame);
  root->addWidget(conflicts, 1);
  auto *resolution = new QHBoxLayout;
  ours = new QPushButton("Version locale");
  ours->setObjectName("mergeOurs");
  theirs = new QPushButton("Version entrante");
  theirs->setObjectName("mergeTheirs");
  edited = new QPushButton("Marquer résolu");
  edited->setObjectName("mergeResolved");
  open = new QPushButton("Ouvrir le fichier…");
  resolution->addWidget(ours);
  resolution->addWidget(theirs);
  resolution->addWidget(open);
  resolution->addWidget(edited);
  root->addLayout(resolution);
  auto *actions = new QHBoxLayout;
  abortButton = new QPushButton("Annuler la fusion");
  abortButton->setObjectName("abortMerge");
  actions->addWidget(abortButton);
  actions->addStretch();
  mergeButton = new QPushButton("Fusionner");
  mergeButton->setObjectName("startMerge");
  finishButton = new QPushButton("Terminer la fusion");
  finishButton->setObjectName("finishMerge");
  actions->addWidget(mergeButton);
  actions->addWidget(finishButton);
  auto *close = new QPushButton("Fermer");
  actions->addWidget(close);
  root->addLayout(actions);
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  connect(target, &QComboBox::currentIndexChanged, this, [this] {
    if (!loading)
      previewTarget();
    updateActions();
  });
  connect(mergeButton, &QPushButton::clicked, this, &MergeDialog::merge);
  connect(ours, &QPushButton::clicked, this, [this] { resolve("ours"); });
  connect(theirs, &QPushButton::clicked, this, [this] { resolve("theirs"); });
  connect(edited, &QPushButton::clicked, this, [this] { resolve("edited"); });
  connect(conflicts, &QListWidget::currentRowChanged, this,
          [this] { updateActions(); });
  connect(open, &QPushButton::clicked, this, [this] {
    if (auto *item = conflicts->currentItem())
      QDesktopServices::openUrl(
          QUrl::fromLocalFile(QDir(path).filePath(item->text())));
  });
  connect(finishButton, &QPushButton::clicked, this, [this] {
    verify(false, [this] {
      run({"diff", "--name-only", "--diff-filter=U", "-z"},
          [this](const CommandResult &r) {
            if (!r.ok() || !r.output.isEmpty()) {
              load("Des conflits restent à résoudre.");
              return;
            }
            run({"commit", "--no-edit"}, [this](const CommandResult &r) {
              load(r.ok() ? QString() : r.message());
            });
          });
    });
  });
  connect(abortButton, &QPushButton::clicked, this, [this] {
    verify(false, [this] {
      run({"merge", "--abort"}, [this](const CommandResult &r) {
        load(r.ok() ? QString() : r.message());
      });
    });
  });
  load();
}
void MergeDialog::run(const QStringList &args,
                      CommandRunner::Callback callback) {
  runner.run("git", args, path, std::move(callback));
}
void MergeDialog::load(const QString &error) {
  working = true;
  loading = true;
  updateActions();
  conflicts->clear();
  run({"symbolic-ref", "--quiet", "--short", "HEAD"}, [this, error](
                                                          const CommandResult
                                                              &r) {
    branch = r.ok() ? QString::fromUtf8(r.output).trimmed() : QString();
    heading->setText(branch.isEmpty()
                         ? "HEAD détachée · sélectionnez une branche"
                         : "Fusionner dans " + branch);
    run({"rev-parse", "--verify", "HEAD"}, [this,
                                            error](const CommandResult &r) {
      head = r.ok() ? QString::fromUtf8(r.output).trimmed() : QString();
      run({"rev-parse", "--verify", "--quiet", "MERGE_HEAD"},
          [this, error](const CommandResult &r) {
            mergeHead =
                r.ok() ? QString::fromUtf8(r.output).trimmed() : QString();
            run({"diff", "--name-only", "--diff-filter=U", "-z"},
                [this, error](const CommandResult &r) {
                  for (const auto &file : r.output.split('\0'))
                    if (!file.isEmpty())
                      conflicts->addItem(QString::fromUtf8(file));
                  if (conflicts->count())
                    conflicts->setCurrentRow(0);
                  run({"for-each-ref", "--format=%(refname)%00%(objectname)",
                       "refs/heads/", "refs/remotes/"},
                      [this, error](const CommandResult &r) {
                        const auto previous = target->currentData()
                                                  .toJsonObject()["ref"]
                                                  .toString();
                        target->clear();
                        int selected = -1;
                        for (const auto &line : r.output.split('\n')) {
                          auto fields = line.split('\0');
                          auto ref = QString::fromUtf8(fields.value(0));
                          if (fields.size() != 2 ||
                              ref == "refs/heads/" + branch ||
                              ref.endsWith("/HEAD"))
                            continue;
                          auto label = ref;
                          label.remove(
                              QRegularExpression("^refs/(heads|remotes)/"));
                          target->addItem(
                              label, QJsonObject{{"ref", ref},
                                                 {"head", QString::fromUtf8(
                                                              fields[1])}});
                          if (ref == previous ||
                              label == target->property("preferred").toString())
                            selected = target->count() - 1;
                        }
                        if (selected >= 0)
                          target->setCurrentIndex(selected);
                        loading = false;
                        working = false;
                        updateActions();
                        if (!error.isEmpty())
                          status->setError(error);
                        else if (!mergeHead.isEmpty())
                          status->setText(
                              conflicts->count()
                                  ? "Résolvez chaque fichier en conflit, puis "
                                    "terminez la fusion."
                                  : "Tous les conflits sont résolus. Terminez "
                                    "la fusion pour créer le commit.");
                        else
                          status->setText("Choisissez la branche à intégrer. "
                                          "Les modifications locales doivent "
                                          "être commitées ou mises de côté.");
                        previewTarget();
                      });
                });
          });
    });
  });
}
void MergeDialog::updateActions() {
  bool ongoing = !mergeHead.isEmpty();
  target->setEnabled(!working && !ongoing);
  mergeButton->setVisible(!ongoing);
  mergeButton->setEnabled(!working && !branch.isEmpty() && !head.isEmpty() &&
                          target->currentIndex() >= 0);
  finishButton->setVisible(ongoing);
  finishButton->setEnabled(!working && conflicts->count() == 0);
  abortButton->setVisible(ongoing);
  abortButton->setEnabled(!working);
  conflicts->setVisible(ongoing);
  preview->setVisible(!ongoing);
  for (auto *button : {ours, theirs, edited, open}) {
    button->setVisible(ongoing);
    button->setEnabled(!working && conflicts->currentItem());
  }
}
void MergeDialog::verify(bool clean, std::function<void()> callback) {
  if (working)
    return;
  working = true;
  updateActions();
  const auto expectedBranch = branch, expectedHead = head,
             expectedMerge = mergeHead;
  run({"symbolic-ref", "--quiet", "--short", "HEAD"}, [this, clean, callback,
                                                       expectedBranch,
                                                       expectedHead,
                                                       expectedMerge](
                                                          const CommandResult
                                                              &r) {
    if (!r.ok() || QString::fromUtf8(r.output).trimmed() != expectedBranch) {
      load("La branche a changé. Vérifiez la sélection avant de recommencer.");
      return;
    }
    run({"rev-parse", "--verify", "HEAD"}, [this, clean, callback, expectedHead,
                                            expectedMerge](
                                               const CommandResult &r) {
      if (!r.ok() || QString::fromUtf8(r.output).trimmed() != expectedHead) {
        load("Les commits ont changé. Vérifiez la branche avant de "
             "recommencer.");
        return;
      }
      run({"rev-parse", "--verify", "--quiet", "MERGE_HEAD"},
          [this, clean, callback, expectedMerge](const CommandResult &r) {
            auto current =
                r.ok() ? QString::fromUtf8(r.output).trimmed() : QString();
            if (current != expectedMerge) {
              load("L’état de la fusion a changé. Vérifiez-le avant de "
                   "recommencer.");
              return;
            }
            if (!clean) {
              callback();
              return;
            }
            run({"status", "--porcelain=v1", "-z", "--untracked-files=all"},
                [this, callback](const CommandResult &r) {
                  if (!r.ok()) {
                    load(r.message());
                    return;
                  }
                  if (!r.output.isEmpty()) {
                    load("Des modifications locales seraient écrasées. Créez "
                         "un commit ou mettez-les de côté avant de fusionner.");
                    return;
                  }
                  callback();
                });
          });
    });
  });
}
void MergeDialog::merge() {
  auto selected = target->currentData().toJsonObject();
  auto ref = selected["ref"].toString(), sha = selected["head"].toString();
  if (!ref.startsWith("refs/heads/") && !ref.startsWith("refs/remotes/"))
    return;
  verify(true, [this, ref, sha] {
    run({"rev-parse", "--verify", ref + "^{commit}"},
        [this, sha](const CommandResult &r) {
          if (!r.ok() || QString::fromUtf8(r.output).trimmed() != sha) {
            load("La branche à intégrer a changé. Relisez les commits avant de "
                 "fusionner.");
            return;
          }
          status->setText("Fusion des branches…");
          run({"-c", "merge.autoStash=false", "merge", "--no-edit", "--", sha},
              [this](const CommandResult &r) {
                if (r.ok()) {
                  status->setText(
                      "Fusion terminée. Vous pouvez fermer cette fenêtre.");
                  load();
                } else
                  load(r.message());
              });
        });
  });
}
void MergeDialog::resolve(const QString &choice) {
  auto *item = conflicts->currentItem();
  if (!item)
    return;
  const auto file = item->text();
  verify(false, [this, file, choice] {
    run({"diff", "--name-only", "--diff-filter=U", "-z"},
        [this, file, choice](const CommandResult &r) {
          if (!r.ok() || !r.output.split('\0').contains(file.toUtf8())) {
            load("Ce fichier ne présente plus de conflit. Vérifiez son "
                 "contenu.");
            return;
          }
          auto stage = [this, file] {
            run({"add", "--", file}, [this](const CommandResult &r) {
              load(r.ok() ? QString() : r.message());
            });
          };
          if (choice == "edited") {
            stage();
            return;
          }
          const auto index = choice == "ours" ? "2" : "3";
          run({"cat-file", "-e", ":" + QString(index) + ":" + file},
              [this, file, choice, stage](const CommandResult &r) {
                if (!r.ok()) {
                  run({"rm", "--force", "--", file},
                      [this](const CommandResult &r) {
                        load(r.ok() ? QString() : r.message());
                      });
                  return;
                }
                run({"checkout", "--" + choice, "--", file},
                    [this, stage](const CommandResult &r) {
                      if (!r.ok()) {
                        load(r.message());
                        return;
                      }
                      stage();
                    });
              });
        });
  });
}
void MergeDialog::previewTarget() {
  if (loading || !mergeHead.isEmpty() || target->currentIndex() < 0)
    return;
  const auto sha = target->currentData().toJsonObject()["head"].toString();
  run({"log", "--left-right", "--oneline", "-n", "60", head + "..." + sha,
       "--"},
      [this, sha](const CommandResult &r) {
        if (target->currentData().toJsonObject()["head"].toString() != sha)
          return;
        preview->setPlainText(
            r.ok()
                ? "< commit de la branche courante · > commit à intégrer\n\n" +
                      QString::fromUtf8(r.output)
                : Diagnostics::explain(r.message()).text());
      });
}
void MergeDialog::done(int result) {
  if (working)
    return;
  GlassDialog::done(result);
}
