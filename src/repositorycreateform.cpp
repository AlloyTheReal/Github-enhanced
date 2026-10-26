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
  auto *heading = new QLabel("Nouveau dépôt GitHub");
  heading->setObjectName("pageTitle");
  root->addWidget(heading);
  auto *accountRow = new QHBoxLayout;
  account = new QLabel("Vérification du compte GitHub…");
  account->setObjectName("createRepositoryAccount");
  account->setTextFormat(Qt::PlainText);
  accountRow->addWidget(account, 1);
  login = new QPushButton("Compte GitHub…");
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
  form->addRow("Nom du dépôt", name);
  form->addRow("Description", description);
  form->addRow("Visibilité", visibility);
  root->addLayout(form);
  readme = new QCheckBox(
      "Créer un README pour pouvoir cloner le dépôt immédiatement");
  readme->setObjectName("initializeRepositoryReadme");
  readme->setChecked(true);
  root->addWidget(readme);
  auto *hint = new QLabel(
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
  connect(name, &QLineEdit::textChanged, this, &RepositoryCreateForm::validate);
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
  authenticated = false;
  validate();
  account->setText("Vérification du compte…");
  QPointer<RepositoryCreateForm> self(this);
  client->api("user", "GET", {}, [self](QJsonValue value, QString error) {
    if (!self)
      return;
    if (!error.isEmpty()) {
      self->account->setText("Compte GitHub non connecté");
      self->status->setText(error);
      self->authenticated = false;
    } else {
      auto login = value.toObject()["login"].toString();
      self->authenticated = !login.isEmpty();
      self->account->setText("Propriétaire : " + login);
      self->status->clear();
    }
    self->validate();
  });
}
void RepositoryCreateForm::validate() {
  auto value = name->text().trimmed();
  const bool valid =
      QRegularExpression("^[A-Za-z0-9_.-]{1,100}$").match(value).hasMatch() &&
      value != "." && value != "..";
  submit->setEnabled(valid && authenticated && !working);
}
void RepositoryCreateForm::setWorking(bool active) {
  working = active;
  name->setEnabled(!active);
  description->setEnabled(!active);
  visibility->setEnabled(!active);
  readme->setEnabled(!active);
  back->setEnabled(!active);
  login->setEnabled(!active);
  progress->setVisible(active);
  submit->setText(active ? "Création…" : "Créer le dépôt");
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
      readme->isChecked());
}
