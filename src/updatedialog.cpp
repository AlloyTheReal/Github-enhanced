#include "updatedialog.h"
#include <QtWidgets>
UpdateDialog::UpdateDialog(AppUpdater *updater, QWidget *parent)
    : GlassDialog(parent), service(updater) {
  setObjectName("updateDialog");
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle("Mises à jour");
  resize(680, 600);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(28, 28, 28, 24);
  root->setSpacing(16);
  auto *scroll = new QScrollArea;
  scroll->setObjectName("updateContentScroll");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *content = new QWidget;
  auto *body = new QVBoxLayout(content);
  body->setContentsMargins(0, 0, 8, 0);
  body->setSpacing(16);
  body->setSizeConstraint(QLayout::SetMinimumSize);
  scroll->setWidget(content);
  root->addWidget(scroll, 1);
  connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, scroll, [scroll] {
    QTimer::singleShot(0, scroll, [scroll] { scroll->verticalScrollBar()->setValue(0); });
  });
  auto *title = new QLabel("Mises à jour de GitHub Enhanced");
  title->setObjectName("pageTitle");
  title->setWordWrap(true);
  root->insertWidget(0, title);
  auto *source = new QLabel(
      "Version installée : " + QCoreApplication::applicationVersion() +
      "\nSource : github.com/" + AppUpdater::repository());
  source->setWordWrap(true);
  source->setTextFormat(Qt::PlainText);
  source->setObjectName("muted");
  root->insertWidget(1, source);
  status = new DiagnosticLabel;
  status->setObjectName("updateStatus");
  status->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  body->addWidget(status);
  notes = new QPlainTextEdit;
  notes->setReadOnly(true);
  notes->setFrameShape(QFrame::NoFrame);
  notes->setPlaceholderText("Les notes de version apparaîtront ici.");
  notes->setMinimumHeight(120);
  notes->setObjectName("updateNotes");
  body->addWidget(notes, 1);
  progress = new QProgressBar;
  progress->setTextVisible(false);
  progress->hide();
  root->addWidget(progress);
  auto *row = new QHBoxLayout;
  checkButton = new QPushButton("Rechercher");
  checkButton->setObjectName("checkForUpdates");
  row->addWidget(checkButton);
  downloadButton = new QPushButton("Télécharger");
  downloadButton->setObjectName("downloadUpdate");
  row->addWidget(downloadButton);
  installButton = new QPushButton("Installer…");
  installButton->setObjectName("installUpdate");
  row->addWidget(installButton);
  root->addLayout(row);
  auto *hint =
      new QLabel("Les téléchargements sont vérifiés avec leur empreinte "
                 "SHA-256. L’installation attend la fin des opérations Git. "
                 "Les installations par paquet restent gérées par le système.");
  hint->setObjectName("muted");
  hint->setWordWrap(true);
  body->addWidget(hint);
  auto *close = new QPushButton("Fermer");
  root->addWidget(close, 0, Qt::AlignRight);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(checkButton, &QPushButton::clicked, this, [this] {
    status->setText("Recherche d’une version stable…");
    checkButton->setEnabled(false);
    service->check(true);
  });
  connect(downloadButton, &QPushButton::clicked, this, [this] {
    if (service->available().assetUrl.isEmpty()) {
      QDesktopServices::openUrl(service->available().releaseUrl);
      return;
    }
    downloadButton->setEnabled(false);
    checkButton->setEnabled(false);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->show();
    status->setText("Téléchargement et vérification de la mise à jour…");
    service->download();
  });
  connect(installButton, &QPushButton::clicked, this, [this] {
    if (!service->downloadedFile().isEmpty())
      emit installRequested(service->downloadedFile());
  });
  connect(
      service, &AppUpdater::checked, this,
      [this](const AppUpdate &u, const QString &e, bool) { display(u, e); });
  connect(service, &AppUpdater::progress, this,
          [this](qint64 bytes, qint64 total) {
            progress->show();
            progress->setValue(total > 0 ? int(100 * bytes / total) : 0);
          });
  connect(
      service, &AppUpdater::downloadFinished, this,
      [this](const QString &file, const QString &error) {
        progress->hide();
        checkButton->setEnabled(true);
        downloadButton->setEnabled(true);
        installButton->setVisible(!file.isEmpty());
        if (error.isEmpty())
          status->setText(
              "Mise à jour téléchargée et vérifiée. Vous pouvez l’installer.");
        else
          status->setError(error);
      });
  display(service->available(), {});
  if (service->busy()) {
    status->setText("Recherche ou téléchargement en cours…");
    checkButton->setEnabled(false);
    downloadButton->setEnabled(false);
  }
}
void UpdateDialog::display(const AppUpdate &update, const QString &error) {
  checkButton->setEnabled(!service->busy());
  downloadButton->setVisible(update.available());
  downloadButton->setEnabled(!service->busy());
  installButton->setVisible(!service->downloadedFile().isEmpty());
  notes->setPlainText(update.notes);
  if (!error.isEmpty())
    status->setError(error);
  else
    status->setText(update.available()
                        ? "La version " + update.version + " est disponible."
                        : "Vous utilisez la dernière version stable publiée.");
  downloadButton->setText(update.assetUrl.isEmpty()
                              ? "Voir la release GitHub"
                              : "Télécharger la mise à jour");
}
