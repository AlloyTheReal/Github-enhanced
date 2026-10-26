#include "releasesdialog.h"
#include "accountdialog.h"
#include <QtWidgets>
namespace {
QString releaseEndpoint(const QString &repo, qint64 id) {
  return "repos/" + repo + "/releases/" + QString::number(id);
}
bool validRepo(const QString &repo) {
  return !repo.isEmpty() && GitHubClient::repositoryFromRemote(
                                "https://github.com/" + repo) == repo;
}
void openGithub(const QString &text) {
  const QUrl url(text);
  if (url.scheme() == "https" && url.host() == "github.com")
    QDesktopServices::openUrl(url);
}
} // namespace
ReleaseEditor::ReleaseEditor(GitHubClient *service, const QString &repo,
                             const QString &branch, const QJsonObject &release,
                             QWidget *parent)
    : GlassDialog(parent), client(service), repository(repo), current(release) {
  setObjectName("releaseEditor");
  newRelease = current["id"].toDouble() <= 0;
  setWindowTitle(newRelease ? "Nouvelle release" : "Modifier la release");
  resize(720, 680);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(12);
  auto *heading = new QLabel(windowTitle());
  heading->setObjectName("pageTitle");
  root->addWidget(heading);
  auto *repoLabel = new QLabel(repo);
  repoLabel->setTextFormat(Qt::PlainText);
  repoLabel->setObjectName("muted");
  root->addWidget(repoLabel);
  auto *form = new QFormLayout;
  tag = new QLineEdit(current["tag_name"].toString());
  tag->setObjectName("releaseTag");
  tag->setPlaceholderText("v1.0.0");
  tag->setReadOnly(!newRelease);
  title = new QLineEdit(current["name"].toString());
  title->setObjectName("releaseTitle");
  target = new QLineEdit(branch);
  target->setObjectName("releaseTarget");
  form->addRow("Tag", tag);
  form->addRow("Titre", title);
  if (newRelease)
    form->addRow("Branche ou SHA distant", target);
  else
    target->hide();
  root->addLayout(form);
  notes = new QPlainTextEdit(current["body"].toString());
  notes->setObjectName("releaseNotes");
  notes->setPlaceholderText("Notes de version (Markdown)");
  notes->setMinimumHeight(130);
  root->addWidget(notes, 1);
  auto *options = new QHBoxLayout;
  draft = new QCheckBox("Brouillon");
  draft->setObjectName("releaseDraft");
  draft->setChecked(newRelease || current["draft"].toBool());
  prerelease = new QCheckBox("Préversion");
  prerelease->setObjectName("releasePrerelease");
  prerelease->setChecked(current["prerelease"].toBool());
  options->addWidget(draft);
  options->addWidget(prerelease);
  options->addStretch();
  root->addLayout(options);
  attachments = new QListWidget;
  attachments->setObjectName("releaseAttachments");
  attachments->setMaximumHeight(100);
  root->addWidget(attachments);
  auto *fileRow = new QHBoxLayout;
  choose = new QPushButton("Ajouter des fichiers…");
  choose->setObjectName("chooseReleaseAssets");
  remove = new QPushButton("Retirer de la sélection");
  fileRow->addWidget(choose);
  fileRow->addWidget(remove);
  fileRow->addStretch();
  root->addLayout(fileRow);
  attachments->setVisible(newRelease);
  choose->setVisible(newRelease);
  remove->setVisible(newRelease);
  connect(choose, &QPushButton::clicked, this, [this] {
    for (const auto &path :
         QFileDialog::getOpenFileNames(this, "Fichiers de la release")) {
      bool exists = false;
      for (int i = 0; i < attachments->count(); ++i)
        if (attachments->item(i)->data(Qt::UserRole).toString() == path)
          exists = true;
      if (!exists) {
        auto *item =
            new QListWidgetItem(QFileInfo(path).fileName(), attachments);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
      }
    }
  });
  connect(remove, &QPushButton::clicked, this,
          [this] { delete attachments->takeItem(attachments->currentRow()); });
  auto *hint = new QLabel(
      "Le tag sera créé sur la cible distante s’il n’existe pas. Les fichiers "
      "sont envoyés comme pièces jointes de release, sans commit Git. Une "
      "nouvelle release reste en brouillon jusqu’à la fin des transferts.");
  hint->setObjectName("muted");
  hint->setWordWrap(true);
  root->addWidget(hint);
  status = new QLabel;
  status->setObjectName("releaseEditorStatus");
  status->setTextFormat(Qt::PlainText);
  status->setWordWrap(true);
  root->addWidget(status);
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  submit = buttons->button(QDialogButtonBox::Save);
  submit->setText(newRelease ? "Créer la release" : "Enregistrer");
  submit->setObjectName("saveRelease");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &ReleaseEditor::save);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  operation = new OperationDialog(this);
  connect(client, &GitHubClient::operationOutput, operation,
          &OperationDialog::append);
}
void ReleaseEditor::done(int result) {
  if (working)
    return;
  GlassDialog::done(result);
}
void ReleaseEditor::setWorking(bool active) {
  working = active;
  submit->setEnabled(!active);
  for (auto *widget : QList<QWidget *>{tag, title, target, notes, draft,
                                       prerelease, choose, remove})
    widget->setEnabled(!active);
}
void ReleaseEditor::fail(const QString &error) {
  setWorking(false);
  const auto message =
      current["id"].toDouble() > 0 && newRelease
          ? "Le brouillon existe sur GitHub ; les fichiers déjà envoyés sont "
            "conservés. Vous pouvez réessayer.\n" +
                error
          : error;
  status->setText(message);
  operation->finish(false, message);
}
void ReleaseEditor::save() {
  if (working)
    return;
  const auto tagName = tag->text().trimmed();
  if (!validRepo(repository) || tagName.isEmpty() || tagName.size() > 255 ||
      tagName.contains(QRegularExpression("[\\x00-\\x20]"))) {
    status->setText(
        "Choisissez un tag valide, sans espace ni caractère de contrôle.");
    return;
  }
  desiredDraft = draft->isChecked();
  setWorking(true);
  status->setText("Enregistrement sur " + repository + "…");
  operation->begin("Enregistrement de la release");
  QJsonObject payload{{"tag_name", tagName},
                      {"name", title->text()},
                      {"body", notes->toPlainText()},
                      {"prerelease", prerelease->isChecked()},
                      {"draft", newRelease ? true : desiredDraft}};
  const auto id = qint64(current["id"].toDouble());
  if (id <= 0) {
    payload["target_commitish"] = target->text().trimmed();
    pending.clear();
    for (int i = 0; i < attachments->count(); ++i)
      pending.append(attachments->item(i)->data(Qt::UserRole).toString());
  } else if (newRelease) {
    for (int i = 0; i < attachments->count(); ++i) {
      auto file = attachments->item(i)->data(Qt::UserRole).toString();
      if (!uploaded.contains(file) && !pending.contains(file))
        pending.append(file);
    }
  }
  QPointer<ReleaseEditor> self(this);
  client->api(id > 0 ? releaseEndpoint(repository, id)
                     : "repos/" + repository + "/releases",
              id > 0 ? "PATCH" : "POST", payload,
              [self](QJsonValue value, QString error) {
                if (!self)
                  return;
                if (!error.isEmpty()) {
                  self->fail(error);
                  return;
                }
                self->current = value.toObject();
                if (self->current["id"].toDouble() <= 0) {
                  self->fail(
                      "Réponse GitHub invalide : release non confirmée.");
                  return;
                }
                self->tag->setReadOnly(true);
                self->uploadNext();
              });
}
void ReleaseEditor::uploadNext() {
  if (pending.isEmpty()) {
    finalize();
    return;
  }
  const auto file = pending.first();
  operation->step("Envoi de " + QFileInfo(file).fileName() + "…");
  status->setText("Envoi de " + QFileInfo(file).fileName() + "…");
  QPointer<ReleaseEditor> self(this);
  client->uploadReleaseAsset(repository, qint64(current["id"].toDouble()), file,
                             [self, file](QJsonValue, QString error) {
                               if (!self)
                                 return;
                               if (!error.isEmpty()) {
                                 self->fail(error);
                                 return;
                               }
                               self->uploaded.insert(file);
                               self->pending.removeFirst();
                               self->uploadNext();
                             });
}
void ReleaseEditor::finalize() {
  if (newRelease && !desiredDraft && current["draft"].toBool()) {
    QPointer<ReleaseEditor> self(this);
    client->api(releaseEndpoint(repository, qint64(current["id"].toDouble())),
                "PATCH", {{"draft", false}},
                [self](QJsonValue value, QString error) {
                  if (!self)
                    return;
                  if (!error.isEmpty()) {
                    self->fail(error);
                    return;
                  }
                  self->current = value.toObject();
                  self->setWorking(false);
                  self->operation->finish(true);
                  emit self->saved(self->current);
                  self->accept();
                });
    return;
  }
  setWorking(false);
  operation->finish(true);
  emit saved(current);
  accept();
}
ReleasesDialog::ReleasesDialog(const QString &path, GitHubClient *service,
                               QWidget *parent, const QString &explicitRepo)
    : GlassDialog(parent), client(service), locator(this), localPath(path) {
  setObjectName("releasesDialog");
  setWindowTitle("Releases du dépôt");
  resize(1060, 720);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(12);
  auto *header = new QHBoxLayout;
  auto *title = new QLabel("Releases");
  title->setObjectName("pageTitle");
  header->addWidget(title, 1);
  auto *account = new QPushButton("Compte GitHub…");
  header->addWidget(account);
  root->addLayout(header);
  connect(account, &QPushButton::clicked, this, [this] {
    if (working)
      return;
    AccountDialog auth("gh", client, nullptr, this);
    auth.exec();
    selectRepository(repository);
  });
  auto *context = new QHBoxLayout;
  remotes = new QComboBox;
  remotes->setObjectName("releaseRemote");
  context->addWidget(remotes);
  repositoryLabel = new QLabel("Recherche du dépôt GitHub…");
  repositoryLabel->setObjectName("releaseRepository");
  repositoryLabel->setTextFormat(Qt::PlainText);
  context->addWidget(repositoryLabel, 1);
  reload = new QPushButton("Actualiser");
  context->addWidget(reload);
  connect(reload, &QPushButton::clicked, this, [this] {
    if (validRepo(repository))
      selectRepository(repository);
    else {
      remotes->clear();
      discover();
    }
  });
  root->addLayout(context);
  connect(remotes, &QComboBox::activated, this, [this](int index) {
    selectRepository(remotes->itemData(index).toString());
  });
  auto *split = new QSplitter;
  list = new QTreeWidget;
  list->setObjectName("repositoryReleases");
  list->setHeaderLabels({"Version", "État"});
  list->setRootIsDecorated(false);
  list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  split->addWidget(list);
  auto *right = new QWidget;
  auto *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(14, 0, 0, 0);
  detail = new QLabel("Sélectionnez une release");
  detail->setTextFormat(Qt::PlainText);
  detail->setWordWrap(true);
  rightLayout->addWidget(detail);
  notes = new QPlainTextEdit;
  notes->setObjectName("releaseBody");
  notes->setReadOnly(true);
  rightLayout->addWidget(notes, 1);
  assets = new QTreeWidget;
  assets->setObjectName("releaseAssets");
  assets->setHeaderLabels({"Fichier", "Taille", "Téléchargements"});
  assets->setRootIsDecorated(false);
  assets->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  assets->setMaximumHeight(200);
  rightLayout->addWidget(assets);
  split->addWidget(right);
  split->setStretchFactor(1, 1);
  split->setSizes({330, 670});
  root->addWidget(split, 1);
  connect(list, &QTreeWidget::itemSelectionChanged, this,
          &ReleasesDialog::selectRelease);
  auto *commands = new QHBoxLayout;
  createButton = new QPushButton("Nouvelle release…");
  createButton->setObjectName("newRelease");
  editButton = new QPushButton("Modifier…");
  editButton->setObjectName("editRelease");
  publishButton = new QPushButton("Publier le brouillon");
  publishButton->setObjectName("publishRelease");
  deleteButton = new QPushButton("Supprimer…");
  deleteButton->setObjectName("deleteRelease");
  more = new QPushButton("Charger la suite");
  for (auto *button :
       {createButton, editButton, publishButton, deleteButton, more})
    commands->addWidget(button);
  commands->addStretch();
  root->addLayout(commands);
  connect(createButton, &QPushButton::clicked, this, [this] { edit(true); });
  connect(editButton, &QPushButton::clicked, this, [this] { edit(false); });
  connect(deleteButton, &QPushButton::clicked, this,
          &ReleasesDialog::removeRelease);
  connect(publishButton, &QPushButton::clicked, this, &ReleasesDialog::publish);
  connect(more, &QPushButton::clicked, this, [this] { refresh(page + 1); });
  auto *fileCommands = new QHBoxLayout;
  addFiles = new QPushButton("Ajouter des fichiers…");
  addFiles->setObjectName("uploadReleaseAssets");
  removeAsset = new QPushButton("Supprimer le fichier…");
  removeAsset->setObjectName("deleteReleaseAsset");
  auto *download = new QPushButton("Télécharger (navigateur)");
  downloadButton = download;
  download->setObjectName("downloadReleaseAsset");
  auto *web = new QPushButton("Voir sur GitHub");
  viewButton = web;
  web->setObjectName("viewReleaseOnGithub");
  for (auto *button : {addFiles, removeAsset, download, web})
    fileCommands->addWidget(button);
  fileCommands->addStretch();
  root->addLayout(fileCommands);
  connect(addFiles, &QPushButton::clicked, this, &ReleasesDialog::uploadFiles);
  connect(removeAsset, &QPushButton::clicked, this,
          &ReleasesDialog::deleteSelectedAsset);
  connect(download, &QPushButton::clicked, this, [this] {
    if (auto *item = assets->currentItem())
      openGithub(item->data(0, Qt::UserRole)
                     .toJsonObject()["browser_download_url"]
                     .toString());
  });
  connect(web, &QPushButton::clicked, this,
          [this] { openGithub(selected()["html_url"].toString()); });
  connect(assets, &QTreeWidget::itemSelectionChanged, this,
          &ReleasesDialog::updateActions);
  status = new QLabel;
  status->setObjectName("releasesStatus");
  status->setTextFormat(Qt::PlainText);
  status->setWordWrap(true);
  root->addWidget(status);
  auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
  close->button(QDialogButtonBox::Close)->setText("Fermer");
  root->addWidget(close);
  connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
  operation = new OperationDialog(this);
  connect(client, &GitHubClient::operationOutput, operation,
          &OperationDialog::append);
  connect(&locator, &CommandRunner::busyChanged, this, [this](bool busy) {
    if (!busy && repository.isEmpty()) {
      repositoryLabel->setText("Aucun distant GitHub reconnu");
      status->setText("Configurez une URL github.com dans les réglages du "
                      "dépôt, puis actualisez.");
    }
  });
  updateActions();
  if (validRepo(explicitRepo)) {
    remotes->hide();
    QTimer::singleShot(
        0, this, [this, explicitRepo] { selectRepository(explicitRepo); });
  } else
    QTimer::singleShot(0, this, &ReleasesDialog::discover);
}
void ReleasesDialog::done(int result) {
  if (working)
    return;
  GlassDialog::done(result);
}
void ReleasesDialog::discover() {
  if (localPath.isEmpty()) {
    status->setText("Choisissez un dépôt GitHub dans Mes dépôts.");
    return;
  }
  locator.run("git", {"remote"}, localPath, [this](const CommandResult &r) {
    if (!r.ok()) {
      status->setText(r.message());
      return;
    }
    auto names = QString::fromUtf8(r.output).split('\n', Qt::SkipEmptyParts);
    names.removeAll("origin");
    if (QString::fromUtf8(r.output).split('\n').contains("origin"))
      names.prepend("origin");
    for (const auto &name : names)
      locator.run("git", {"remote", "get-url", name}, localPath,
                  [this, name](const CommandResult &url) {
                    const auto repo = GitHubClient::repositoryFromRemote(
                        QString::fromUtf8(url.output).trimmed());
                    if (!url.ok() || repo.isEmpty())
                      return;
                    remotes->addItem(name + " · " + repo, repo);
                    if (repository.isEmpty())
                      selectRepository(repo);
                  });
    if (names.isEmpty()) {
      repositoryLabel->setText("Aucun distant GitHub");
      status->setText(
          "Configurez un distant GitHub dans les réglages du dépôt.");
    }
  });
}
void ReleasesDialog::selectRepository(const QString &name) {
  if (working)
    return;
  ++revision;
  repository = name;
  canWrite = false;
  page = 0;
  hasMore = false;
  lastOperationError.clear();
  preferredRelease = 0;
  list->clear();
  assets->clear();
  notes->clear();
  detail->setText("Sélectionnez une release");
  repositoryLabel->setText(validRepo(name) ? name
                                           : "Aucun dépôt GitHub identifié");
  updateActions();
  if (!validRepo(name)) {
    status->setText("Ce distant n’est pas une URL github.com reconnue.");
    return;
  }
  const auto expected = revision;
  QPointer<ReleasesDialog> self(this);
  status->setText("Vérification du dépôt et des droits…");
  client->api("repos/" + name, "GET", {},
              [self, name, expected](QJsonValue value, QString error) {
                if (!self || self->revision != expected)
                  return;
                if (!error.isEmpty()) {
                  self->status->setText(error);
                  return;
                }
                auto repo = value.toObject();
                self->defaultBranch = repo["default_branch"].toString("main");
                self->canWrite =
                    repo["permissions"].toObject()["push"].toBool();
                self->refresh();
              });
}
void ReleasesDialog::refresh(int requestedPage) {
  if (!validRepo(repository) || working)
    return;
  const auto expected = revision;
  const auto selection = preferredRelease > 0
                             ? preferredRelease
                             : qint64(selected()["id"].toDouble());
  preferredRelease = 0;
  more->setEnabled(false);
  status->setText("Chargement des releases de " + repository + "…");
  QPointer<ReleasesDialog> self(this);
  client->api(
      "repos/" + repository +
          "/releases?per_page=30&page=" + QString::number(requestedPage),
      "GET", {},
      [self, expected, requestedPage, selection](QJsonValue value,
                                                 QString error) {
        if (!self || self->revision != expected)
          return;
        if (!error.isEmpty()) {
          self->status->setText(error);
          return;
        }
        auto entries = value.toArray();
        {
          QSignalBlocker block(self->list);
          if (requestedPage == 1)
            self->list->clear();
          for (const auto &value : entries) {
            auto release = value.toObject();
            const auto state = release["draft"].toBool()        ? "Brouillon"
                               : release["prerelease"].toBool() ? "Préversion"
                                                                : "Publiée";
            const auto caption = release["name"].toString();
            auto *item = new QTreeWidgetItem(
                self->list,
                {caption.isEmpty() ? release["tag_name"].toString() : caption,
                 state});
            item->setData(0, Qt::UserRole, release);
            item->setToolTip(0, release["tag_name"].toString());
          }
        }
        self->page = requestedPage;
        self->hasMore = entries.size() == 30;
        QTreeWidgetItem *chosen = nullptr;
        for (int i = 0; i < self->list->topLevelItemCount(); ++i) {
          auto *item = self->list->topLevelItem(i);
          if (qint64(item->data(0, Qt::UserRole)
                         .toJsonObject()["id"]
                         .toDouble()) == selection)
            chosen = item;
        }
        if (!chosen && self->list->topLevelItemCount())
          chosen = self->list->topLevelItem(0);
        if (chosen)
          self->list->setCurrentItem(chosen);
        self->selectRelease();
        self->status->setText(
            QString("%1 release(s) · ").arg(self->list->topLevelItemCount()) +
            (self->canWrite ? "Accès en écriture" : "Lecture seule") +
            (self->lastOperationError.isEmpty()
                 ? QString()
                 : "\n" + self->lastOperationError));
        self->updateActions();
      });
}
QJsonObject ReleasesDialog::selected() const {
  return list->currentItem()
             ? list->currentItem()->data(0, Qt::UserRole).toJsonObject()
             : QJsonObject{};
}
void ReleasesDialog::selectRelease() {
  auto release = selected();
  assets->clear();
  notes->setPlainText(release["body"].toString());
  detail->setText(
      release.isEmpty()
          ? "Sélectionnez une release"
          : release["tag_name"].toString() + " · " +
                release["target_commitish"].toString() + "\n" +
                (release["immutable"].toBool() ? "Release immuable" : ""));
  for (const auto &value : release["assets"].toArray()) {
    auto asset = value.toObject();
    auto *item = new QTreeWidgetItem(
        assets,
        {asset["name"].toString(),
         QString::number(asset["size"].toDouble() / 1048576.0, 'f', 1) + " Mio",
         QString::number(asset["download_count"].toInt())});
    item->setData(0, Qt::UserRole, asset);
  }
  if (assets->topLevelItemCount())
    assets->setCurrentItem(assets->topLevelItem(0));
  updateActions();
}
void ReleasesDialog::updateActions() {
  const auto release = selected();
  const bool allowed = canWrite && !working;
  const bool mutableRelease =
      !release.isEmpty() && !release["immutable"].toBool();
  createButton->setEnabled(allowed);
  editButton->setEnabled(allowed && mutableRelease);
  deleteButton->setEnabled(allowed && mutableRelease);
  publishButton->setEnabled(allowed && mutableRelease &&
                            release["draft"].toBool());
  addFiles->setEnabled(allowed && mutableRelease);
  removeAsset->setEnabled(allowed && mutableRelease && assets->currentItem());
  remotes->setEnabled(!working);
  reload->setEnabled(!working);
  more->setEnabled(!working && hasMore);
  downloadButton->setEnabled(assets->currentItem() != nullptr);
  viewButton->setEnabled(!release["html_url"].toString().isEmpty());
  list->setEnabled(!working);
}
void ReleasesDialog::edit(bool create) {
  if (!canWrite || working || !validRepo(repository))
    return;
  ReleaseEditor editor(client, repository, defaultBranch,
                       create ? QJsonObject{} : selected(), this);
  connect(&editor, &ReleaseEditor::saved, this,
          [this](const QJsonObject &release) {
            preferredRelease = qint64(release["id"].toDouble());
          });
  editor.exec();
  refresh();
}
void ReleasesDialog::setWorking(bool active, const QString &title) {
  working = active;
  if (active) {
    lastOperationError.clear();
    operation->begin(title);
    status->setText(title + "…");
  }
  updateActions();
}
void ReleasesDialog::finish(const QString &error) {
  working = false;
  operation->finish(error.isEmpty(), error);
  lastOperationError = error;
  if (!error.isEmpty())
    status->setText(error);
  updateActions();
  refresh();
}
void ReleasesDialog::removeRelease() {
  auto release = selected();
  if (!canWrite || working || release.isEmpty())
    return;
  if (QMessageBox::question(this, "Supprimer la release",
                            repository + " · " +
                                release["tag_name"].toString() +
                                "\nSupprimer cette release et ses fichiers ? "
                                "Le tag Git est conservé.",
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes)
    return;
  setWorking(true, "Suppression de la release");
  QPointer<ReleasesDialog> self(this);
  client->api(releaseEndpoint(repository, qint64(release["id"].toDouble())),
              "DELETE", {}, [self](QJsonValue, QString error) {
                if (self)
                  self->finish(error);
              });
}
void ReleasesDialog::publish() {
  auto release = selected();
  if (!canWrite || working || !release["draft"].toBool())
    return;
  setWorking(true, "Publication du brouillon");
  QPointer<ReleasesDialog> self(this);
  client->api(releaseEndpoint(repository, qint64(release["id"].toDouble())),
              "PATCH", {{"draft", false}}, [self](QJsonValue, QString error) {
                if (self)
                  self->finish(error);
              });
}
void ReleasesDialog::uploadFiles() {
  auto release = selected();
  if (!canWrite || working || release.isEmpty())
    return;
  auto files =
      QFileDialog::getOpenFileNames(this, "Fichiers à joindre à la release");
  if (files.isEmpty())
    return;
  setWorking(true, "Envoi des fichiers de release");
  uploadNext(files, repository, qint64(release["id"].toDouble()));
}
void ReleasesDialog::uploadNext(QStringList files, const QString &repo,
                                qint64 id) {
  if (files.isEmpty()) {
    finish();
    return;
  }
  auto file = files.takeFirst();
  operation->step("Envoi de " + QFileInfo(file).fileName() + "…");
  QPointer<ReleasesDialog> self(this);
  client->uploadReleaseAsset(
      repo, id, file, [self, files, repo, id](QJsonValue, QString error) {
        if (!self)
          return;
        if (!error.isEmpty()) {
          self->finish(error);
          return;
        }
        self->uploadNext(files, repo, id);
      });
}
void ReleasesDialog::deleteSelectedAsset() {
  auto *item = assets->currentItem();
  if (!canWrite || working || !item)
    return;
  auto asset = item->data(0, Qt::UserRole).toJsonObject();
  if (QMessageBox::question(this, "Supprimer le fichier",
                            repository + "\nSupprimer " +
                                asset["name"].toString() + " de la release ?",
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes)
    return;
  setWorking(true, "Suppression du fichier joint");
  QPointer<ReleasesDialog> self(this);
  client->api("repos/" + repository + "/releases/assets/" +
                  QString::number(qint64(asset["id"].toDouble())),
              "DELETE", {}, [self](QJsonValue, QString error) {
                if (self)
                  self->finish(error);
              });
}
