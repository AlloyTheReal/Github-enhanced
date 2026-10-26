#include "reposettings.h"
#include <QtWidgets>

QString RepoSettings::ignoreTemplate(const QString &name) {
    if (name == "C++")
        return "# C++\nbuild*/\nCMakeFiles/\nCMakeCache.txt\n*.o\n*.obj\n*.user\n";
    if (name == "Node.js")
        return "# Node.js\nnode_modules/\ndist/\ncoverage/\n.env\n.env.local\n";
    if (name == "Python")
        return "# Python\n__pycache__/\n*.py[cod]\n.venv/\nvenv/\n.pytest_cache/\n.env\n";
    if (name == "Rust")
        return "# Rust\ntarget/\n";
    return "# Système\n.DS_Store\nThumbs.db\n*.swp\n";
}
RepoSettings::RepoSettings(const QString &repository, const QJsonObject &account, QWidget *parent)
    : GlassDialog(parent), path(repository), runner(this) {
    setObjectName("repoSettings");
    setWindowTitle("Réglages du dépôt");
    resize(680, 580);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);
    auto *title = new QLabel(windowTitle());
    title->setObjectName("pageTitle");
    root->addWidget(title);
    auto *tabs = new QTabWidget;
    root->addWidget(tabs, 1);
    auto *general = new QWidget;
    auto *form = new QFormLayout(general);
    name = new QLineEdit;
    name->setObjectName("gitUserName");
    email = new QLineEdit;
    email->setObjectName("gitUserEmail");
    origin = new QLineEdit;
    origin->setObjectName("gitOrigin");
    form->addRow("Nom des commits", name);
    form->addRow("E-mail des commits", email);
    form->addRow("Dépôt distant origin", origin);
    auto *hint = new QLabel(
        "Ces réglages d’identité s’appliquent uniquement à ce dépôt. Laissez origin vide pour le conserver.");
    hint->setWordWrap(true);
    form->addRow(hint);
    if (!account["login"].toString().isEmpty()) {
        auto *fill = new QPushButton("Utiliser mon identité GitHub");
        form->addRow(fill);
        connect(fill, &QPushButton::clicked, this, [this, account] {
            name->setText(account["name"].toString(account["login"].toString()));
            email->setText(QString::number(qint64(account["id"].toDouble())) + '+' +
                           account["login"].toString() + "@users.noreply.github.com");
        });
    }
    tabs->addTab(general, "Général");
    auto *ignorePage = new QWidget;
    auto *ignoreLayout = new QVBoxLayout(ignorePage);
    auto *templates = new QHBoxLayout;
    for (const auto &key : {"C++", "Node.js", "Python", "Rust", "Système"}) {
        auto *button = new QPushButton(key);
        templates->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, key] {
            QString current = ignore->toPlainText();
            auto existing = current.split('\n');
            for (const auto &line : ignoreTemplate(key).split('\n'))
                if (!line.isEmpty() && !existing.contains(line)) {
                    if (!current.isEmpty() && !current.endsWith('\n'))
                        current += '\n';
                    current += line + '\n';
                    existing.append(line);
                }
            ignore->setPlainText(current);
        });
    }
    ignoreLayout->addLayout(templates);
    ignore = new QPlainTextEdit;
    ignore->setObjectName("gitIgnoreEditor");
    ignore->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ignoreLayout->addWidget(ignore, 1);
    tabs->addTab(ignorePage, ".gitignore");
    const auto ignorePath = QDir(path).filePath(".gitignore");
    QFile file(ignorePath);
    if (QFileInfo(ignorePath).isSymLink()) {
        ignoreWritable = false;
        ignore->setReadOnly(true);
    } else if (file.open(QIODevice::ReadOnly)) {
        if (file.size() > 1024 * 1024) {
            ignoreWritable = false;
            ignore->setReadOnly(true);
        } else
            initialIgnore = file.readAll();
    }
    ignore->setPlainText(QString::fromUtf8(initialIgnore));
    status = new QLabel;
    status->setWordWrap(true);
    root->addWidget(status);
    if (!ignoreWritable)
        status->setText("Le .gitignore est un lien symbolique ou un fichier trop volumineux. Modifiez-le "
                        "dans votre éditeur.");
    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    save = box->button(QDialogButtonBox::Save);
    save->setText("Enregistrer");
    save->setIcon({});
    save->setEnabled(false);
    box->button(QDialogButtonBox::Cancel)->setText("Annuler");
    box->button(QDialogButtonBox::Cancel)->setIcon({});
    root->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, this, &RepoSettings::apply);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (auto *field : {name, email, origin})
        field->setEnabled(false);
    runner.run("git", {"config", "--get", "user.name"}, path,
               [this](const CommandResult &r) { name->setText(QString::fromUtf8(r.output).trimmed()); });
    runner.run("git", {"config", "--get", "user.email"}, path,
               [this](const CommandResult &r) { email->setText(QString::fromUtf8(r.output).trimmed()); });
    runner.run("git", {"remote", "get-url", "origin"}, path, [this](const CommandResult &r) {
        hadOrigin = r.ok();
        origin->setText(r.ok() ? QString::fromUtf8(r.output).trimmed() : QString());
        for (auto *field : {name, email, origin})
            field->setEnabled(true);
        save->setEnabled(true);
    });
}
void RepoSettings::apply() {
    if (runner.busy())
        return;
    for (auto *field : {name, email, origin})
        if (field->text().contains('\n') || field->text().contains('\r')) {
            status->setText("Les valeurs doivent tenir sur une ligne.");
            return;
        }
    if (!email->text().isEmpty() &&
        !QRegularExpression("^[^\\s@]+@[^\\s@]+$").match(email->text()).hasMatch()) {
        status->setText("L’adresse e-mail est invalide.");
        return;
    }
    const auto remote = origin->text().trimmed();
    if (remote.startsWith('-')) {
        status->setText("L’URL distante est invalide.");
        return;
    }
    if (ignoreWritable) {
        if (QFileInfo(QDir(path).filePath(".gitignore")).isSymLink()) {
            status->setText("Le .gitignore est devenu un lien symbolique. Rouvrez les réglages.");
            return;
        }
        QFile current(QDir(path).filePath(".gitignore"));
        QByteArray bytes;
        if (current.open(QIODevice::ReadOnly))
            bytes = current.readAll();
        if (bytes != initialIgnore) {
            status->setText(
                "Le .gitignore a changé en dehors de cette fenêtre. Rouvrez les réglages pour le recharger.");
            return;
        }
    }
    jobs.clear();
    if (!name->text().trimmed().isEmpty())
        jobs.append({"config", "--local", "--", "user.name", name->text().trimmed()});
    if (!email->text().trimmed().isEmpty())
        jobs.append({"config", "--local", "--", "user.email", email->text().trimmed()});
    if (!remote.isEmpty())
        jobs.append({"remote", hadOrigin ? "set-url" : "add", "origin", remote});
    save->setEnabled(false);
    status->setText("Enregistrement…");
    next();
}
void RepoSettings::next() {
    if (!jobs.isEmpty()) {
        auto args = jobs.takeFirst();
        runner.run("git", args, path, [this](const CommandResult &r) {
            if (!r.ok()) {
                status->setText(r.message());
                save->setEnabled(true);
            } else
                next();
        });
        return;
    }
    const auto content = ignore->toPlainText().toUtf8();
    if (ignoreWritable && content != initialIgnore) {
        QSaveFile file(QDir(path).filePath(".gitignore"));
        if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) {
            status->setText(file.errorString());
            save->setEnabled(true);
            return;
        }
    }
    accept();
}
