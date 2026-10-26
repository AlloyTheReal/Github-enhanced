#include "repobrowser.h"
#include "accountdialog.h"
#include "repositorycreateform.h"
#include <QtWidgets>

RepoBrowser::RepoBrowser(GitHubClient *service, QWidget *parent)
    : GlassDialog(parent), client(service) {
  setObjectName("repoBrowser");
  setWindowTitle("Mes dépôts GitHub");
  resize(860, 630);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(26, 24, 26, 24);
  root->setSpacing(14);
  pages = new QStackedWidget;
  root->addWidget(pages, 1);
  auto *browsePage = new QWidget;
  pages->addWidget(browsePage);
  auto *browse = new QVBoxLayout(browsePage);
  browse->setContentsMargins(0, 0, 0, 0);
  browse->setSpacing(14);
  auto *header = new QHBoxLayout;
  auto *title = new QLabel(windowTitle());
  title->setObjectName("pageTitle");
  header->addWidget(title, 1);
  auto *connectButton = new QPushButton("Compte GitHub…");
  header->addWidget(connectButton);
  browse->addLayout(header);
  connect(connectButton, &QPushButton::clicked, this, [this] {
    AccountDialog auth("gh", client, nullptr, this);
    auth.exec();
    refresh();
  });
  search = new QLineEdit;
  search->setPlaceholderText("Filtrer les dépôts chargés");
  search->setObjectName("repositorySearch");
  browse->addWidget(search);
  connect(search, &QLineEdit::textChanged, this, &RepoBrowser::filter);
  list = new QTreeWidget;
  list->setObjectName("githubRepositories");
  list->setHeaderLabels({"Dépôt", "Accès", "Branche"});
  list->setRootIsDecorated(false);
  list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  browse->addWidget(list, 1);
  status = new DiagnosticLabel;
  status->setWordWrap(true);
  status->setObjectName("repositoryBrowserStatus");
  browse->addWidget(status);
  auto *commands = new QHBoxLayout;
  auto *reload = new QPushButton("Actualiser");
  commands->addWidget(reload);
  connect(reload, &QPushButton::clicked, this, &RepoBrowser::refresh);
  more = new QPushButton("Charger la suite");
  more->setEnabled(false);
  commands->addWidget(more);
  connect(more, &QPushButton::clicked, this, [this] {
    more->setEnabled(false);
    client->repositories(page + 1);
  });
  auto *createButton = new QPushButton("Nouveau dépôt…");
  createButton->setObjectName("newGithubRepository");
  commands->addWidget(createButton);
  auto *releases = new QPushButton("Releases…");
  releases->setObjectName("repositoryReleases");
  commands->addWidget(releases);
  connect(releases, &QPushButton::clicked, this, [this] {
    if (auto *item = list->currentItem()) {
      auto repo = item->data(0, Qt::UserRole).toJsonObject();
      emit releasesRequested(repo["full_name"].toString());
    }
  });
  connect(createButton, &QPushButton::clicked, this, &RepoBrowser::create);
  commands->addStretch();
  auto *view = new QPushButton("Voir sur GitHub");
  commands->addWidget(view);
  connect(view, &QPushButton::clicked, this, [this] {
    if (!list->currentItem())
      return;
    auto repo = list->currentItem()->data(0, Qt::UserRole).toJsonObject();
    auto url = QUrl(repo["html_url"].toString());
    if (url.scheme() == "https" && url.host() == "github.com")
      QDesktopServices::openUrl(url);
  });
  auto *clone = new QPushButton("Cloner…");
  clone->setObjectName("primary");
  commands->addWidget(clone);
  browse->addLayout(commands);
  connect(clone, &QPushButton::clicked, this, [this] {
    auto *item = list->currentItem();
    if (!item)
      return;
    auto repo = item->data(0, Qt::UserRole).toJsonObject();
    auto url = QUrl(repo["clone_url"].toString());
    if (url.scheme() != "https" || url.host() != "github.com")
      return;
    emit cloneRequested(url.toString(), repo["name"].toString());
    accept();
  });
  connect(list, &QTreeWidget::itemDoubleClicked, clone, &QPushButton::click);
  auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
  close->button(QDialogButtonBox::Close)->setText("Fermer");
  close->button(QDialogButtonBox::Close)->setIcon({});
  root->addWidget(close);
  connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(client, &GitHubClient::repositoriesLoaded, this,
          [this](const QJsonArray &items, int current, bool hasMore) {
            page = current;
            for (const auto &item : items)
              repositories.append(item);
            filter();
            more->setEnabled(hasMore);
            status->setText(QString("%1 dépôt(s) accessible(s) chargé(s)")
                                .arg(repositories.size()));
          });
  connect(client, &GitHubClient::errorOccurred, this,
          [this](const QString &error) {
            status->setError(error);
            more->setEnabled(page > 0);
          });
  QTimer::singleShot(0, this, &RepoBrowser::refresh);
}
void RepoBrowser::refresh() {
  repositories = {};
  page = 0;
  list->clear();
  more->setEnabled(false);
  status->setText("Chargement des dépôts accessibles…");
  client->repositories(1);
}
void RepoBrowser::filter() {
  list->clear();
  const auto query = search->text().trimmed();
  for (const auto &value : repositories) {
    const auto repo = value.toObject();
    if (!query.isEmpty() &&
        !(repo["full_name"].toString() + ' ' + repo["description"].toString())
             .contains(query, Qt::CaseInsensitive))
      continue;
    auto *item = new QTreeWidgetItem(
        list, {repo["full_name"].toString(),
               repo["private"].toBool() ? "Privé" : "Public",
               repo["default_branch"].toString()});
    item->setData(0, Qt::UserRole, repo);
    item->setToolTip(0, repo["description"].toString());
  }
  if (list->topLevelItemCount())
    list->setCurrentItem(list->topLevelItem(0));
}
void RepoBrowser::create() {
  if (!createForm) {
    createForm = new RepositoryCreateForm(client, this);
    pages->addWidget(createForm);
    connect(createForm, &RepositoryCreateForm::cancelled, this,
            [this] { pages->setCurrentIndex(0); });
    connect(createForm, &RepositoryCreateForm::created, this,
            [this](const QJsonObject &repo) {
              QJsonArray updated;
              updated.append(repo);
              for (const auto &entry : repositories)
                if (entry.toObject()["full_name"] != repo["full_name"])
                  updated.append(entry);
              repositories = updated;
              search->clear();
              filter();
              pages->setCurrentIndex(0);
              status->setText(
                  "Dépôt créé : " + repo["full_name"].toString() +
                  ". Sélectionnez Cloner pour l’ouvrir localement.");
            });
  }
  pages->setCurrentWidget(createForm);
  createForm->focusName();
}
void RepoBrowser::done(int result) {
  if (createForm && createForm->busy())
    return;
  GlassDialog::done(result);
}
