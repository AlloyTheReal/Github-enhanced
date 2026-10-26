#include "accountdialog.h"
#include <QtWidgets>

AccountDialog::AccountDialog(const QString &name, GitHubClient *gh,
                             CodexClient *cx, QWidget *parent, bool workflows)
    : GlassDialog(parent), provider(name), github(gh), codex(cx),
      installer(this), workflowsOnly(workflows && name == "gh") {
  setObjectName("accountDialog");
  setWindowTitle(workflowsOnly  ? "Autoriser les workflows GitHub"
                 : name == "gh" ? "Connexion GitHub"
                                : "Connexion Codex");
  resize(600, 500);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(28, 28, 28, 24);
  layout->setSpacing(16);
  auto *title = new QLabel(windowTitle());
  title->setObjectName("pageTitle");
  layout->addWidget(title);
  auto *description = new QLabel(
      name == "gh" ? "Connectez votre compte dans le navigateur pour accéder à "
                     "vos dépôts. "
                     "GitHub CLI gère la session OAuth et l’accès HTTPS de Git."
                   : "Connectez votre compte ChatGPT pour utiliser Codex. Le "
                     "composant officiel "
                     "Codex gère la connexion OAuth et sa session partagée "
                     "avec Codex CLI.");
  if (workflowsOnly)
    description->setText(
        "GitHub exige l’autorisation workflow pour envoyer les fichiers de "
        ".github/workflows/. Autorisez GitHub CLI dans le navigateur pour "
        "compléter la connexion existante. Les autres autorisations sont "
        "conservées.");
  description->setWordWrap(true);
  description->setObjectName("muted");
  layout->addWidget(description);
  status = new DiagnosticLabel;
  status->setWordWrap(true);
  layout->addWidget(status);
  code = new QLabel;
  code->setObjectName("oauthDeviceCode");
  code->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                Qt::TextSelectableByKeyboard);
  auto *codeRow = new QHBoxLayout;
  codeRow->addWidget(code, 1);
  copyButton = new QPushButton("Copier le code");
  copyButton->setEnabled(false);
  codeRow->addWidget(copyButton);
  layout->addLayout(codeRow);
  connect(copyButton, &QPushButton::clicked, this, [this] {
    if (!code->text().isEmpty())
      QGuiApplication::clipboard()->setText(code->text().section(" : ", 1));
  });
  auto *actions = new QHBoxLayout;
  connectButton = new QPushButton(ToolRuntime::executable(name).isEmpty()
                                      ? "Installer et se connecter"
                                      : "Se connecter dans le navigateur");
  if (workflowsOnly)
    connectButton->setText("Autoriser workflow dans le navigateur");
  connectButton->setObjectName("primary");
  actions->addWidget(connectButton);
  browserButton = new QPushButton("Ouvrir le navigateur");
  browserButton->setEnabled(false);
  actions->addWidget(browserButton);
  layout->addLayout(actions);
  connect(browserButton, &QPushButton::clicked, this, [this] {
    if (url.isValid())
      QDesktopServices::openUrl(url);
  });
  connect(connectButton, &QPushButton::clicked, this, [this] {
    if (ToolRuntime::executable(provider).isEmpty()) {
      connectButton->setEnabled(false);
      installer.install(provider);
    } else {
      if (provider == "codex")
        codex->resetTransport();
      start();
    }
  });
  connect(&installer, &RuntimeInstaller::progress, status, &QLabel::setText);
  connect(&installer, &RuntimeInstaller::finished, this,
          [this](const QString &, const QString &error) {
            if (!error.isEmpty()) {
              status->setError(error);
              connectButton->setEnabled(true);
            } else
              start();
          });
  auto *existing = new QPushButton("Choisir un exécutable déjà installé…");
  existing->setFlat(true);
  layout->addWidget(existing);
  connect(existing, &QPushButton::clicked, this, [this] {
    auto file = QFileDialog::getOpenFileName(this, "Exécutable " + provider);
    if (!file.isEmpty()) {
      ToolRuntime::setExecutable(provider, file);
      if (provider == "codex")
        codex->resetTransport();
      else
        github->cancelLogin();
      connectButton->setText(workflowsOnly
                                 ? "Autoriser workflow dans le navigateur"
                                 : "Se connecter dans le navigateur");
      connectButton->setEnabled(true);
    }
  });
  if (provider == "codex") {
    auto *device = new QPushButton("Utiliser un code de connexion");
    device->setFlat(true);
    layout->addWidget(device);
    connect(device, &QPushButton::clicked, this, [this] { start(true); });
    connect(codex, &CodexClient::loginStarted, this,
            [this](const QUrl &link, const QString &userCode) {
              url = link;
              code->setText(userCode.isEmpty() ? QString()
                                               : "Code : " + userCode);
              copyButton->setEnabled(!userCode.isEmpty());
              status->setText("Terminez la connexion dans votre navigateur.");
              browserButton->setEnabled(true);
              QDesktopServices::openUrl(url);
            });
    connect(codex, &CodexClient::loginFinished, this, &AccountDialog::complete);
    connect(codex, &CodexClient::errorOccurred, this,
            [this](const QString &error) {
              status->setError(error);
              connectButton->setEnabled(true);
            });
  } else {
    url = QUrl("https://github.com/login/device");
    connect(
        github, &GitHubClient::authOutput, this, [this](const QString &text) {
          details->appendPlainText(text);
          const auto match = QRegularExpression("\\b[A-Z0-9]{4}-[A-Z0-9]{4}\\b")
                                 .match(details->toPlainText());
          if (match.hasMatch()) {
            code->setText("Code : " + match.captured());
            copyButton->setEnabled(true);
            browserButton->setEnabled(true);
          }
          status->setText("Saisissez le code dans le navigateur, puis "
                          "autorisez GitHub CLI.");
        });
    connect(github, &GitHubClient::loginFinished, this,
            &AccountDialog::complete);
  }
  auto *advanced = new QCheckBox("Mode avancé · détails techniques");
  advanced->setObjectName("accountAdvanced"); advanced->setChecked(Diagnostics::advanced()); layout->addWidget(advanced);
  connect(advanced, &QCheckBox::toggled, &Diagnostics::instance(), &Diagnostics::setAdvanced);
  connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, this, [advanced](bool enabled){QSignalBlocker blocker(advanced);advanced->setChecked(enabled);});
  details = new QPlainTextEdit;
  details->setReadOnly(true);
  details->setMaximumBlockCount(100);
  details->setMaximumHeight(110);
  layout->addWidget(details);
  details->setVisible(provider == "gh" && Diagnostics::advanced());
  connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, this, [this](bool enabled) {details->setVisible(provider == "gh" && enabled);});
  auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
  box->button(QDialogButtonBox::Close)->setText("Fermer");
  box->button(QDialogButtonBox::Close)->setIcon({});
  layout->addWidget(box);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
void AccountDialog::start(bool device) {
  details->clear();
  code->clear();
  copyButton->setEnabled(false);
  browserButton->setEnabled(false);
  connectButton->setEnabled(false);
  status->setText("Préparation de la connexion…");
  if (provider == "gh") {
    if (workflowsOnly)
      github->authorizeWorkflows();
    else
      github->login();
  } else
    codex->login(device);
}
void AccountDialog::complete(bool success, const QString &error) {
  connected = success;
  if (!success) { status->setError(error); connectButton->setEnabled(true); return; }
  status->setText(
      success ? (workflowsOnly
                     ? "Autorisation workflow ajoutée. Fermez cette fenêtre "
                       "pour réessayer l’envoi."
                     : "Compte connecté. Vous pouvez fermer cette fenêtre.")
              : error);
  connectButton->setEnabled(!success);
  if (success) {
    browserButton->setEnabled(false);
    code->clear();
    copyButton->setEnabled(false);
  }
}
void AccountDialog::done(int result) {
  if (!connected) {
    if (provider == "gh")
      github->cancelLogin();
    else
      codex->cancelLogin();
  }
  GlassDialog::done(result);
}
