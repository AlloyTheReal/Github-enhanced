#include "operationdialog.h"
#include <QtWidgets>
OperationDialog::OperationDialog(QWidget *parent) : GlassDialog(parent) {
  setObjectName("operationDialog");
  setWindowTitle("Activité");
  resize(560, 380);
  setModal(false);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(14);
  titleLabel = new QLabel("Activité des opérations");
  titleLabel->setObjectName("pageTitle");
  titleLabel->setWordWrap(true);
  root->addWidget(titleLabel);
  detail = new QLabel("Aucune opération en cours.");
  detail->setObjectName("operationDetail");
  detail->setWordWrap(true);
  root->addWidget(detail);
  progress = new QProgressBar;
  progress->setObjectName("operationProgress");
  progress->setTextVisible(false);
  root->addWidget(progress);
  details = new QPlainTextEdit;
  details->setObjectName("operationLog");
  details->setReadOnly(true);
  details->setMaximumBlockCount(300);
  root->addWidget(details, 1);
  auto *hint =
      new QLabel("Vous pouvez masquer cette fenêtre ; l’opération continue.");
  hint->setObjectName("muted");
  hint->setWordWrap(true);
  root->addWidget(hint);
  closeButton = new QPushButton("Masquer");
  root->addWidget(closeButton, 0, Qt::AlignRight);
  connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);
  reveal.setSingleShot(true);
  connect(&reveal, &QTimer::timeout, this, [this] {
    if (active) {
      show();
      raise();
    }
  });
}
void OperationDialog::begin(const QString &title) {
  ++completionRevision;
  active = true;
  titleLabel->setText(title);
  details->clear();
  pending.clear();
  detail->setText("Préparation…");
  progress->setRange(0, 0);
  closeButton->setText("Masquer");
  reveal.start(450);
}
void OperationDialog::step(const QString &text) {
  detail->setText(text);
  progress->setRange(0, 0);
  append(text + '\n');
}
void OperationDialog::append(const QString &text) {
  pending += text;
  pending.replace('\r', '\n');
  while (pending.contains('\n')) {
    int end = pending.indexOf('\n');
    auto line = pending.left(end).trimmed();
    pending.remove(0, end + 1);
    if (line.isEmpty())
      continue;
    details->appendPlainText(line.left(2000));
    auto match = QRegularExpression("(?:^|\\s)(\\d{1,3})%").match(line);
    if (match.hasMatch()) {
      progress->setRange(0, 100);
      progress->setValue(qBound(0, match.captured(1).toInt(), 100));
      detail->setText(line.left(220));
    }
  }
  if (pending.size() > 4000)
    pending = pending.right(4000);
}
void OperationDialog::finish(bool success, const QString &message) {
  if (!active && success)
    return;
  const int revision = ++completionRevision;
  active = false;
  reveal.stop();
  const auto tail = pending;
  pending.clear();
  append(tail + '\n');
  progress->setRange(0, 100);
  progress->setValue(success ? 100 : 0);
  detail->setText(message.isEmpty()
                      ? (success ? "Terminé" : "L’opération a échoué.")
                      : message);
  closeButton->setText("Fermer");
  if (!success) {
    show();
    raise();
  } else
    QTimer::singleShot(1000, this, [this, revision] {
      if (!active && revision == completionRevision)
        hide();
    });
}
