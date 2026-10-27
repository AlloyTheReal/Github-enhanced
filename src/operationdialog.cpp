#include "operationdialog.h"
#include <QtWidgets>
OperationDialog::OperationDialog(QWidget *parent) : GlassDialog(parent) {
  setObjectName("operationDialog");
  setWindowTitle("Activité");
  resize(640, 480);
  setModal(false);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(14);
  titleLabel = new QLabel("Activité des opérations");
  titleLabel->setObjectName("pageTitle");
  titleLabel->setWordWrap(true);
  root->addWidget(titleLabel);
  detail = new QLabel;
  detail->setTextFormat(Qt::PlainText);
  detail->setText("Aucune opération en cours.");
  detail->setObjectName("operationDetail");
  detail->setWordWrap(true);
  detail->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);
  titleLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);
  root->addWidget(detail);
  progress = new QProgressBar;
  progress->setObjectName("operationProgress");
  progress->setTextVisible(false);
  root->addWidget(progress);
  advancedToggle = new QCheckBox("Mode avancé · détails techniques");
  advancedToggle->setObjectName("operationAdvanced");
  advancedToggle->setChecked(Diagnostics::advanced());
  root->addWidget(advancedToggle);
  connect(advancedToggle, &QCheckBox::toggled, &Diagnostics::instance(),
          &Diagnostics::setAdvanced);
  details = new QPlainTextEdit;
  details->setObjectName("operationLog");
  details->setReadOnly(true);
  details->setMaximumBlockCount(300);
  details->setMinimumHeight(120);
  details->document()->setDocumentMargin(10);
  root->addWidget(details, 1);
  details->setVisible(Diagnostics::advanced());
  connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, this,
          [this](bool enabled) {
            QSignalBlocker blocker(advancedToggle);
            advancedToggle->setChecked(enabled);
            details->setVisible(enabled);
          });
  hint =
      new QLabel("Vous pouvez masquer cette fenêtre ; l’opération continue.");
  hint->setObjectName("muted");
  hint->setWordWrap(true);
  hint->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);
  root->addWidget(hint);
  recoveryButton = new QPushButton;
  recoveryButton->setObjectName("operationRecovery");
  recoveryButton->hide();
  root->addWidget(recoveryButton);
  connect(recoveryButton, &QPushButton::clicked, this, [this] {
    auto action = recovery;
    recovery = {};
    recoveryButton->hide();
    if (action)
      action();
  });
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
  recovery = {};
  recoveryButton->hide();
  ++completionRevision;
  active = true;
  titleLabel->setText(title);
  details->clear();
  pending.clear();
  detail->setText("Préparation…");
  hint->setText("Vous pouvez masquer cette fenêtre ; l’opération continue.");
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
      if (active)
        detail->setText("Transfert en cours · " + match.captured(1) + "%");
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
  if (success)
    detail->setText(message.isEmpty() ? "Terminé" : message);
  else {
    detail->setText(Diagnostics::explain(
                        message.isEmpty() ? "L’opération a échoué." : message)
                        .text());
    if (!message.isEmpty())
      details->appendPlainText("\nÉchec de l’opération :\n" + message);
  }
  hint->setText(success ? "L’opération est terminée."
                        : "Consultez l’explication avant de réessayer.");
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

void OperationDialog::setRecoveryAction(const QString &title,
                                        std::function<void()> action) {
  recovery = std::move(action);
  recoveryButton->setText(title);
  recoveryButton->setVisible(bool(recovery));
}
