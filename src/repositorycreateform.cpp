#include "repositorycreateform.h"
#include "accountdialog.h"
#include <QtWidgets>
RepositoryCreateForm::RepositoryCreateForm(GitHubClient *service,
                                           QWidget *parent)
    : QWidget(parent), client(service) {
  setObjectName("repositoryCreateForm");
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(16);
  heading = new QLabel("Nouveau dépôt GitHub");
  heading->setObjectName("pageTitle");
  root->addWidget(heading);
  auto *accountRow = new QHBoxLayout;
  account = new QLabel("Vérification du compte GitHub…");
  account->setObjectName("createRepositoryAccount");
  account->setTextFormat(Qt::PlainText);
  accountRow->addWidget(account, 1);
  login = new QPushButton("Se connecter à GitHub…");
  login->setObjectName("repositoryLogin");
  login->hide();
  accountRow->addWidget(login);
  root->addLayout(accountRow);
  connect(login, &QPushButton::clicked, this, [this] {
    AccountDialog auth("gh", client, nullptr, this);
    auth.exec();
    loadAccount();
  });
  auto *form = new QFormLayout;
  form->setSpacing(14);
  name = new QLineEdit;
  name->setObjectName("newRepositoryName");
  name->setPlaceholderText("mon-projet");
  name->setMaxLength(100);
  description = new QLineEdit;
  description->setObjectName("newRepositoryDescription");
  visibility = new QComboBox;
  visibility->addItems({"Privé", "Public"});
  visibility->setObjectName("newRepositoryVisibility");
  owner = new QComboBox;
  owner->setObjectName("repositoryOwner");
  form->addRow("Propriétaire", owner);
  form->addRow("Nom du dépôt", name);
  form->addRow("Description", description);
  form->addRow("Visibilité", visibility);
  root->addLayout(form);
  readme = new QCheckBox(
      "Créer un README pour pouvoir cloner le dépôt immédiatement");
  readme->setObjectName("initializeRepositoryReadme");
  readme->setChecked(true);
  root->addWidget(readme);
  ownerHint = new QLabel;
  ownerHint->setObjectName("repositoryOwnerHint");
  ownerHint->setTextFormat(Qt::PlainText);
  ownerHint->setWordWrap(true);
  root->addWidget(ownerHint);
  pushCommits = new QCheckBox("Envoyer aussi les commits existants");
  pushCommits->setObjectName("publishExistingCommits");
  pushCommits->hide();
  root->addWidget(pushCommits);
  hint = new QLabel(
      "Le dépôt sera créé sur le compte affiché ci-dessus. Saisissez "
      "uniquement son nom, sans URL ni nom de propriétaire.");
  hint->setWordWrap(true);
  hint->setObjectName("muted");
  root->addWidget(hint);
  status = new QLabel;
  status->setObjectName("repositoryCreationStatus");
  status->setTextFormat(Qt::PlainText);
  status->setWordWrap(true);
  root->addWidget(status);
  progress = new QProgressBar;
  progress->setRange(0, 0);
  progress->setTextVisible(false);
  progress->hide();
  root->addWidget(progress);
  root->addStretch();
  auto *actions = new QHBoxLayout;
  back = new QPushButton("Retour aux dépôts");
  actions->addWidget(back);
  actions->addStretch();
  submit = new QPushButton("Créer le dépôt");
  submit->setObjectName("createRepositorySubmit");
  submit->setEnabled(false);
  actions->addWidget(submit);
  root->addLayout(actions);
  connect(back, &QPushButton::clicked, this, &RepositoryCreateForm::cancelled);
  connect(submit, &QPushButton::clicked, this, &RepositoryCreateForm::create);
  connect(name, &QLineEdit::textChanged, this, [this](const QString &text) {
    auto normalized = GitHubClient::normalizedRepositoryName(text);
    if (normalized != text) {
      const int cursor = GitHubClient::normalizedRepositoryName(
                             text.left(name->cursorPosition()))
                             .size();
      QSignalBlocker block(name);
      name->setText(normalized);
      name->setCursorPosition(cursor);
    }
    updateOwner();
  });
  connect(owner, &QComboBox::currentIndexChanged, this,
          [this] { updateOwner(); });
  connect(visibility, &QComboBox::currentIndexChanged, this,
          [this] { updateOwner(); });
  connect(name, &QLineEdit::returnPressed, this, [this] {
    if (submit->isEnabled())
      create();
  });
}
void RepositoryCreateForm::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (!working)
    loadAccount();
}
void RepositoryCreateForm::focusName() {
  name->setFocus();
  name->selectAll();
}
void RepositoryCreateForm::loadAccount() {
  const int revision = ++accountRevision;
  authenticated = false;
  organizations.clear();
  owner->clear();
  owner->setEnabled(false);
  login->hide();
  validate();
  account->setText("Vérification du compte GitHub…");
  QPointer<RepositoryCreateForm> self(this);
  client->api("user", "GET", {},
              [self, revision](QJsonValue value, QString error) {
                if (!self || revision != self->accountRevision)
                  return;
                if (!error.isEmpty()) {
                  self->account->setText("Compte GitHub non connecté");
                  self->status->setText(error);
                  self->authenticated = false;
                  self->login->show();
                } else {
                  const auto login = value.toObject()["login"].toString();
                  self->authenticated = !login.isEmpty();
                  self->account->setText("Connecté en tant que " + login);
                  self->login->setVisible(!self->authenticated);
                  self->status->clear();
                  if (self->authenticated) {
                    self->owner->addItem(login + " · Compte personnel",
                                         QJsonObject{{"login", login},
                                                     {"organization", false},
                                                     {"canPrivate", true},
                                                     {"canPublic", true}});
                    self->owner->setEnabled(!self->working);
                    self->loadOrganizations(1, revision);
                  }
                }
                self->updateOwner();
              });
}
void RepositoryCreateForm::loadOrganizations(int page, int revision) {
  QPointer<RepositoryCreateForm> self(this);
  client->api(
      "user/memberships/orgs?state=active&per_page=100&page=" +
          QString::number(page),
      "GET", {}, [self, revision, page](QJsonValue value, QString error) {
        if (!self || revision != self->accountRevision)
          return;
        if (!error.isEmpty()) {
          if (!self->working)
            self->status->setText("Le compte personnel est disponible. "
                                  "Chargement des organisations impossible : " +
                                  error);
          return;
        }
        const auto memberships = value.toArray();
        for (const auto &entry : memberships) {
          auto membership = entry.toObject();
          const auto login =
              membership["organization"].toObject()["login"].toString();
          if (membership["state"].toString() != "active" ||
              !QRegularExpression("^[A-Za-z0-9-]+$").match(login).hasMatch() ||
              self->organizations.contains(login))
            continue;
          self->organizations.insert(login);
          const bool admin = membership["role"].toString() == "admin";
          self->client->api(
              "orgs/" + login, "GET", {},
              [self, revision, login, admin](QJsonValue details,
                                             QString error) {
                if (!self || revision != self->accountRevision)
                  return;
                auto policy = details.toObject();
                const bool defaultAllowed =
                    policy["members_can_create_repositories"].toBool(true);
                const bool privateAllowed =
                    error.isEmpty() &&
                    (admin ||
                     policy["members_can_create_private_repositories"].toBool(
                         defaultAllowed));
                const bool publicAllowed =
                    error.isEmpty() &&
                    (admin ||
                     policy["members_can_create_public_repositories"].toBool(
                         defaultAllowed));
                const bool known =
                    admin ||
                    policy.contains(
                        "members_can_create_private_repositories") ||
                    policy.contains("members_can_create_public_repositories") ||
                    policy.contains("members_can_create_repositories");
                self->owner->addItem(login + " · Organisation" +
                                         (!privateAllowed && !publicAllowed
                                              ? " (création indisponible)"
                                              : ""),
                                     QJsonObject{{"login", login},
                                                 {"organization", true},
                                                 {"canPrivate", privateAllowed},
                                                 {"canPublic", publicAllowed},
                                                 {"known", known}});
                const int row = self->owner->count() - 1;
                if (auto *model = qobject_cast<QStandardItemModel *>(
                        self->owner->model()))
                  if (auto *item = model->item(row)) {
                    item->setEnabled(privateAllowed || publicAllowed);
                    item->setToolTip(
                        !error.isEmpty() ? error
                        : !known ? "GitHub confirmera les droits lors de la "
                                   "création."
                                 : "Droits de création de l’organisation.");
                  }
                self->updateOwner();
              });
        }
        if (memberships.size() == 100)
          self->loadOrganizations(page + 1, revision);
      });
}
void RepositoryCreateForm::updateOwner() {
  auto selected = owner->currentData().toJsonObject();
  const bool privateAllowed = selected["canPrivate"].toBool(),
             publicAllowed = selected["canPublic"].toBool();
  if (auto *model = qobject_cast<QStandardItemModel *>(visibility->model())) {
    if (auto *item = model->item(0))
      item->setEnabled(privateAllowed);
    if (auto *item = model->item(1))
      item->setEnabled(publicAllowed);
  }
  const bool permitted =
      visibility->currentIndex() == 0 ? privateAllowed : publicAllowed;
  if (!authenticated)
    ownerHint->clear();
  else if (!permitted)
    ownerHint->setText(
        "Cette visibilité n’est pas autorisée pour ce propriétaire. Choisissez "
        "explicitement une visibilité disponible.");
  else
    ownerHint->setText(
        "Destination : " + selected["login"].toString() + "/" + name->text() +
        ((selected["organization"].toBool() && !selected["known"].toBool())
             ? " · droits confirmés par GitHub à la création"
             : ""));
  validate();
}
void RepositoryCreateForm::validate() {
  auto value = name->text().trimmed();
  const bool valid =
      QRegularExpression("^[A-Za-z0-9_.-]{1,100}$").match(value).hasMatch() &&
      value != "." && value != "..";
  auto selected = owner->currentData().toJsonObject();
  const bool permitted = visibility->currentIndex() == 0
                             ? selected["canPrivate"].toBool()
                             : selected["canPublic"].toBool();
  submit->setEnabled(valid && authenticated && !working && permitted);
}
void RepositoryCreateForm::setWorking(bool active) {
  working = active;
  name->setEnabled(!active);
  description->setEnabled(!active);
  visibility->setEnabled(!active);
  owner->setEnabled(!active && authenticated);
  pushCommits->setEnabled(!active &&
                          pushCommits->property("hasCommits").toBool());
  readme->setEnabled(!active);
  back->setEnabled(!active);
  login->setEnabled(!active);
  progress->setVisible(active);
  submit->setText(active       ? "Création…"
                  : publishing ? "Publier le dépôt"
                               : "Créer le dépôt");
  validate();
}
void RepositoryCreateForm::create() {
  if (!submit->isEnabled())
    return;
  setWorking(true);
  status->setText("Création du dépôt sur GitHub…");
  QPointer<RepositoryCreateForm> self(this);
  client->createRepository(
      name->text().trimmed(), description->text(),
      visibility->currentIndex() == 0,
      [self](QJsonValue value, QString error) {
        if (!self)
          return;
        self->setWorking(false);
        if (!error.isEmpty()) {
          self->status->setText(error);
          return;
        }
        auto repository = value.toObject();
        if (repository["full_name"].toString().isEmpty() ||
            repository["clone_url"].toString().isEmpty()) {
          self->status->setText("GitHub n’a pas confirmé la création du dépôt. "
                                "Actualisez la liste avant de réessayer.");
          return;
        }
        self->status->setText("Dépôt créé : " +
                              repository["full_name"].toString());
        emit self->created(repository);
      },
      readme->isChecked(),
      owner->currentData().toJsonObject()["organization"].toBool()
          ? owner->currentData().toJsonObject()["login"].toString()
          : QString());
}

void RepositoryCreateForm::configurePublication(const QString &initialName,
                                                bool hasCommits) {
  publishing = true;
  heading->setText("Publier sur GitHub");
  name->setText(GitHubClient::normalizedRepositoryName(initialName.trimmed()));
  readme->setChecked(false);
  readme->hide();
  pushCommits->setProperty("hasCommits", hasCommits);
  pushCommits->setEnabled(hasCommits);
  pushCommits->show();
  back->setText("Annuler");
  submit->setText("Publier le dépôt");
  hint->setText("Le dépôt sera créé chez le propriétaire choisi puis lié à "
                "origin. Les fichiers non commités ne sont pas envoyés. Les "
                "espaces du nom sont remplacés par des tirets.");
}
bool RepositoryCreateForm::sendExistingCommits() const {
  return publishing && pushCommits->isEnabled() && pushCommits->isChecked();
}
RepositoryPublishDialog::RepositoryPublishDialog(GitHubClient *client,
                                                 const QString &name,
                                                 bool hasCommits,
                                                 QWidget *parent)
    : GlassDialog(parent) {
  setObjectName("publishRepositoryDialog");
  setWindowTitle("Publier sur GitHub");
  resize(660, 600);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 24);
  form = new RepositoryCreateForm(client, this);
  form->configurePublication(name, hasCommits);
  root->addWidget(form);
  connect(form, &RepositoryCreateForm::cancelled, this, &QDialog::reject);
  connect(form, &RepositoryCreateForm::created, this,
          [this](const QJsonObject &repo) {
            result = repo;
            accept();
          });
}
void RepositoryPublishDialog::done(int result) {
  if (form->busy())
    return;
  GlassDialog::done(result);
}
bool RepositoryPublishDialog::sendExistingCommits() const {
  return form->sendExistingCommits();
}
