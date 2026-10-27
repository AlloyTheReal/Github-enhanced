#include "custompanel.h"
#include "visuals.h"
#include <QtWidgets>
CustomPanel::CustomPanel(const QJsonObject &definition, QWidget *parent)
    : QDockWidget(definition["title"].toString(), parent), config(definition) {
  setObjectName(config["id"].toString());
  setMinimumSize(160, 140);
  auto *content = new QWidget;
  auto *root = new QVBoxLayout(content);
  root->setContentsMargins(16, 16, 16, 16);
  root->setSpacing(10);
  auto *heading = new QLabel(windowTitle());
  heading->setObjectName("panelHeading");
  root->addWidget(heading);
  const auto type = config["type"].toString();
  if (type == "notes") {
    notes = new QPlainTextEdit;
    notes->setObjectName("customPanelNotes");
    notes->setPlaceholderText("Vos notes pour ce projet…");
    notes->setPlainText(config["body"].toString());
    notes->setFrameShape(QFrame::NoFrame);
    root->addWidget(notes, 1);
    connect(notes, &QPlainTextEdit::textChanged, this, [this] {
      const auto text = notes->toPlainText();
      if (text.size() > 65536) {
        QSignalBlocker blocker(notes);
        const auto position = notes->textCursor().position();
        notes->setPlainText(text.left(65536));
        auto cursor = notes->textCursor();
        cursor.setPosition(qMin(position, 65536));
        notes->setTextCursor(cursor);
      }
      emit contentChanged();
    });
  } else if (type == "checklist") {
    tasks = new QListWidget;
    tasks->setObjectName("customPanelTasks");
    tasks->setFrameShape(QFrame::NoFrame);
    root->addWidget(tasks, 1);
    for (const auto &value : config["tasks"].toArray()) {
      const auto t = value.toObject();
      auto *item = new QListWidgetItem(t["text"].toString(), tasks);
      item->setFlags(item->flags() | Qt::ItemIsEditable |
                     Qt::ItemIsUserCheckable);
      item->setCheckState(t["done"].toBool() ? Qt::Checked : Qt::Unchecked);
    }
    auto *row = new QHBoxLayout;
    auto *input = new QLineEdit;
    input->setObjectName("customPanelTaskInput");
    input->setPlaceholderText("Nouvelle tâche");
    row->addWidget(input, 1);
    auto *add = new QPushButton("Ajouter");
    row->addWidget(add);
    root->addLayout(row);
    auto create = [this, input] {
      auto text = input->text().trimmed();
      if (text.isEmpty() || tasks->count() >= 200)
        return;
      auto *item = new QListWidgetItem(text.left(500), tasks);
      item->setFlags(item->flags() | Qt::ItemIsEditable |
                     Qt::ItemIsUserCheckable);
      item->setCheckState(Qt::Unchecked);
      input->clear();
      emit contentChanged();
    };
    connect(add, &QPushButton::clicked, this, create);
    connect(input, &QLineEdit::returnPressed, this, create);
    connect(tasks, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *item) {
              if (item->text().size() > 500) {
                QSignalBlocker blocker(tasks);
                item->setText(item->text().left(500));
              }
              emit contentChanged();
            });
    auto *remove = new QPushButton("Retirer la tâche sélectionnée");
    root->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, [this] {
      delete tasks->takeItem(tasks->currentRow());
      emit contentChanged();
    });
  } else {
    repositoryInfo = new QLabel(
        "Ouvrez un dépôt pour afficher sa branche et son état distant.");
    repositoryInfo->setTextFormat(Qt::PlainText);
    repositoryInfo->setWordWrap(true);
    repositoryInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(repositoryInfo);
    root->addStretch();
  }
  setWidget(content);
}
QJsonObject CustomPanel::definition() const {
  auto result = config;
  result["title"] = windowTitle();
  if (notes)
    result["body"] = notes->toPlainText().left(65536);
  if (tasks) {
    QJsonArray list;
    for (int i = 0; i < tasks->count(); ++i)
      list.append(
          QJsonObject{{"text", tasks->item(i)->text().left(500)},
                      {"done", tasks->item(i)->checkState() == Qt::Checked}});
    result["tasks"] = list;
  }
  return result;
}
void CustomPanel::updateRepository(const RepositorySyncState &s) {
  if (!repositoryInfo)
    return;
  repositoryInfo->setText(
      "Dossier\n" + s.repository + "\n\nBranche\n" + s.branch +
      "\n\nSuivi distant\n" +
      (s.upstream.isEmpty() ? "Non configuré" : s.upstream) +
      QString("\n\n%1 commit(s) à envoyer · %2 à récupérer")
          .arg(s.ahead)
          .arg(s.behind) +
      (s.error.isEmpty() ? QString() : "\n\nÉtat distant non vérifié"));
}
bool CustomPanel::validate(const QJsonArray &definitions) {
  if (definitions.size() > 16 ||
      QJsonDocument(definitions).toJson(QJsonDocument::Compact).size() > 524288)
    return false;
  QSet<QString> ids;
  for (const auto &v : definitions) {
    auto c = v.toObject();
    auto id = c["id"].toString();
    if (!QRegularExpression("^custom-[a-zA-Z0-9-]{1,64}$")
             .match(id)
             .hasMatch() ||
        ids.contains(id) || c["title"].toString().trimmed().isEmpty() ||
        c["title"].toString().size() > 80 ||
        !QStringList{"notes", "checklist", "repository"}.contains(
            c["type"].toString()) ||
        c["body"].toString().size() > 65536 ||
        c["tasks"].toArray().size() > 200)
      return false;
    ids.insert(id);
    for (const auto &t : c["tasks"].toArray())
      if (!t.isObject() || t.toObject()["text"].toString().size() > 500)
        return false;
  }
  return true;
}
QJsonObject CustomPanel::edit(QWidget *parent, const QJsonObject &initial) {
  GlassDialog dialog(parent);
  dialog.setObjectName("customPanelEditor");
  dialog.setWindowTitle(initial.isEmpty() ? "Créer un panneau"
                                          : "Modifier le panneau");
  dialog.resize(480, 290);
  auto *root = new QVBoxLayout(&dialog);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(16);
  auto *heading = new QLabel(dialog.windowTitle());
  heading->setObjectName("pageTitle");
  root->addWidget(heading);
  auto *name = new QLineEdit(initial["title"].toString("Mes notes"));
  name->setObjectName("customPanelTitle");
  name->setMaxLength(80);
  auto *kind = new GlassComboBox;
  kind->setObjectName("customPanelType");
  kind->addItem("Notes libres", "notes");
  kind->addItem("Liste de tâches", "checklist");
  kind->addItem("État du dépôt", "repository");
  kind->setCurrentIndex(qMax(0, kind->findData(initial["type"].toString())));
  kind->setEnabled(initial.isEmpty());
  auto *form = new QFormLayout;
  form->addRow("Nom", name);
  form->addRow("Contenu", kind);
  root->addLayout(form);
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Save)->setText("Enregistrer");
  buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
  root->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    if (!name->text().trimmed().isEmpty())
      dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted)
    return {};
  auto result = initial;
  if (result.isEmpty())
    result["id"] =
        "custom-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
  result["title"] = name->text().trimmed();
  result["type"] = kind->currentData().toString();
  return result;
}
