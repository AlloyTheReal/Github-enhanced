#include "settingsdialog.h"
#include "theme.h"
#include "accountdialog.h"
#include "toolruntime.h"
#include <QtWidgets>
#include <QNetworkReply>

namespace {
QLabel *caption(const QString &text, const char *name) {
    auto *label = new QLabel(text);
    label->setObjectName(name);
    label->setWordWrap(true);
    return label;
}
QFrame *settingRow(const QString &title, const QString &subtitle, QWidget *control) {
    auto *card = new QFrame;
    card->setObjectName("settingsCard");
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(18, 12, 16, 12);
    row->setSpacing(20);
    auto *labels = new QVBoxLayout;
    labels->setSpacing(4);
    labels->addWidget(caption(title, "rowTitle"));
    if (!subtitle.isEmpty())
        labels->addWidget(caption(subtitle, "rowSubtitle"));
    row->addLayout(labels, 1);
    row->addWidget(control, 0, Qt::AlignVCenter);
    return card;
}
QWidget *newPage(QVBoxLayout **layout) {
    auto *page = new QWidget;
    page->setObjectName("settingsPage");
    *layout = new QVBoxLayout(page);
    (*layout)->setContentsMargins(0, 0, 0, 0);
    (*layout)->setSpacing(10);
    return page;
}
QWidget *scrollPage(QWidget *page) {
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    return scroll;
}
QIcon navigationIcon(int index) {
    QPixmap image(40, 40);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(qApp->palette().color(QPalette::Text), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (index == 0) {
        p.drawEllipse(QRectF(3, 3, 14, 14));
        p.drawArc(QRectF(6, 6, 8, 8), 90 * 16, 180 * 16);
    } else if (index == 1) {
        p.drawEllipse(QRectF(3, 3, 14, 14));
        p.setBrush(qApp->palette().color(QPalette::Text));
        p.drawEllipse(QRectF(6, 5, 2, 2));
        p.drawEllipse(QRectF(11, 5, 2, 2));
        p.drawEllipse(QRectF(5, 10, 2, 2));
    } else if (index == 2) {
        p.drawRoundedRect(QRectF(3, 3, 14, 14), 2, 2);
        p.drawLine(9, 3, 9, 17);
        p.drawLine(3, 11, 9, 11);
    } else if (index == 3) {
        p.drawRoundedRect(QRectF(3, 3, 6, 6), 1, 1);
        p.drawRoundedRect(QRectF(12, 3, 5, 6), 1, 1);
        p.drawRoundedRect(QRectF(3, 12, 6, 5), 1, 1);
        p.drawRoundedRect(QRectF(12, 12, 5, 5), 1, 1);
    } else if (index == 4) {
        p.drawEllipse(QRectF(7, 3, 6, 6));
        p.drawArc(QRectF(3, 10, 14, 12), 0, 180 * 16);
    } else {
        p.drawEllipse(QRectF(8, 2, 4, 4));
        p.drawLine(10, 7, 10, 17);
        p.drawLine(4, 8, 16, 8);
        p.drawLine(10, 12, 5, 18);
        p.drawLine(10, 12, 15, 18);
    }
    p.end();
    return QIcon(image);
}
class ThemePreview : public QWidget {
  public:
    explicit ThemePreview(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 176);
        setMaximumHeight(220);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        const auto t = property("previewTheme").toJsonObject();
        if (t.isEmpty())
            return;
        auto c = [&](const char *key) { return QColor(t[key].toString()); };
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = rect().adjusted(2, 2, -2, -2);
        QPainterPath path;
        path.addRoundedRect(r, 14, 14);
        p.setClipPath(path);
        p.fillRect(r, c("surface"));
        const qreal split = r.width() * .32, top = 32;
        p.fillRect(QRectF(r.left(), r.top(), r.width(), top), c("background"));
        p.fillRect(QRectF(r.left(), r.top() + top, split, r.height() - top), c("background"));
        p.setPen(Qt::NoPen);
        p.setBrush(c("muted"));
        for (int i = 0; i < 3; ++i)
            p.drawEllipse(QPointF(14 + i * 10, 17), 2, 2);
        p.setPen(c("text"));
        auto f = font();
        f.setPixelSize(10);
        p.setFont(f);
        p.drawText(QRectF(51, 8, r.width() - 60, 18), Qt::AlignVCenter, "github-enhanced");
        p.setBrush(c("accent"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(8, 48, split - 12, 22), 5, 5);
        p.setBrush(c("muted"));
        p.drawRoundedRect(QRectF(13, 86, split - 24, 3), 1, 1);
        p.drawRoundedRect(QRectF(13, 102, split - 31, 3), 1, 1);
        for (int i = 0; i < 5; ++i) {
            p.setBrush(c(i == 2 ? "added" : i == 3 ? "removed" : "muted"));
            p.drawRoundedRect(QRectF(split + 14, 56 + i * 18, r.width() - split - 30 - (i % 3) * 12, 3), 1,
                              1);
        }
        p.setClipping(false);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c("border"), 1));
        p.drawPath(path);
    }
};
} // namespace

SettingsDialog::SettingsDialog(const QJsonObject &p, QWidget *parent, GitHubClient *gh, CodexClient *cx)
    : GlassDialog(parent), profile(p), runner(this), network(this), github(gh ? gh : new GitHubClient(this)),
      codex(cx ? cx : new CodexClient(this)), draft(p["theme"].toObject()) {
    runner.setEnvironmentRemovals(
        {"GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN"});
    appearanceTimer = new QTimer(this);
    appearanceTimer->setSingleShot(true);
    appearanceTimer->setInterval(80);
    connect(appearanceTimer, &QTimer::timeout, this, [this] {
        if (liveAppearance->isChecked())
            emit appearancePreviewRequested(currentProfile());
    });
    setObjectName("settingsDialog");
    setWindowTitle("Réglages de GitHub Enhanced");
    resize(1060, 740);
    setMinimumSize(900, 650);
    themes = Theme::installed();
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);
    auto *sidebar = new QFrame;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setFixedWidth(270);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 20, 16, 18);
    side->setSpacing(14);
    auto *search = new QLineEdit;
    search->setObjectName("settingsSearch");
    search->setPlaceholderText("Rechercher un réglage");
    search->setClearButtonEnabled(true);
    side->addWidget(search);
    navigation = new QListWidget;
    navigation->setObjectName("settingsNavigation");
    navigation->setIconSize(QSize(18, 18));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    side->addWidget(navigation, 1);
    auto section = [&](const QString &name) {
        auto *item = new QListWidgetItem(name, navigation);
        item->setData(Qt::UserRole, -1);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 36));
    };
    auto destination = [&](const QString &name, int page) {
        auto *item = new QListWidgetItem(navigationIcon(page), name, navigation);
        item->setData(Qt::UserRole, page);
        item->setSizeHint(QSize(0, 44));
    };
    section("Interface");
    destination("Apparence", 0);
    destination("Couleurs", 1);
    destination("Disposition", 2);
    section("Personnalisation");
    destination("Bibliothèque de thèmes", 3);
    section("Application");
    destination("Compte GitHub", 4);
    destination("Compte Codex", 6);
    destination("Accessibilité", 5);
    side->addWidget(caption("GitHub Enhanced", "rowTitle"));
    side->addWidget(caption("Préférences locales", "rowSubtitle"));
    root->addWidget(sidebar);
    auto *content = new QFrame;
    content->setObjectName("settingsContent");
    auto *right = new QVBoxLayout(content);
    right->setContentsMargins(30, 26, 30, 22);
    right->setSpacing(18);
    auto *header = new QHBoxLayout;
    pageTitle = caption("Apparence", "pageTitle");
    header->addWidget(pageTitle, 1);
    auto *close = new QToolButton;
    close->setText("×");
    close->setToolButtonStyle(Qt::ToolButtonTextOnly);
    close->setFixedSize(32, 32);
    close->setAccessibleName("Fermer les réglages");
    close->setToolTip("Fermer");
    header->addWidget(close, 0, Qt::AlignTop);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    right->addLayout(header);
    pageSubtitle = caption("Une interface à votre image.", "muted");
    right->addWidget(pageSubtitle);
    pages = new TransitionStack;
    pages->setObjectName("settingsPages");
    right->addWidget(pages, 1);
    root->addWidget(content, 1);
    QVBoxLayout *layout;
    auto *appearance = newPage(&layout);
    auto *modes = new QFrame;
    modes->setObjectName("segmented");
    auto *modeLayout = new QHBoxLayout(modes);
    modeLayout->setContentsMargins(3, 3, 3, 3);
    modeLayout->setSpacing(3);
    auto *dark = new QPushButton("Sombre"), *light = new QPushButton("Clair");
    dark->setObjectName("modeDark");
    light->setObjectName("modeLight");
    dark->setCheckable(true);
    light->setCheckable(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(dark);
    modeGroup->addButton(light);
    modeLayout->addWidget(dark);
    modeLayout->addWidget(light);
    dark->setChecked(QColor(draft["surface"].toString()).lightness() < 128);
    light->setChecked(!dark->isChecked());
    layout->addWidget(settingRow("Style de l’interface", {}, modes));
    themeBox = new QComboBox;
    themeBox->setObjectName("themeSelector");
    themeBox->setMinimumWidth(220);
    themeBox->setMaximumWidth(260);
    layout->addWidget(settingRow("Thème", "Une palette cohérente pour tout l’espace de travail.", themeBox));
    fontBox = new QSpinBox;
    fontBox->setRange(10, 22);
    fontBox->setSuffix(" pt");
    fontBox->setValue(p["fontSize"].toInt(11));
    fontBox->setMaximumWidth(100);
    layout->addWidget(settingRow("Taille du texte", {}, fontBox));
    strengthSlider = new QSlider(Qt::Horizontal);
    strengthSlider->setObjectName("glassStrength");
    strengthSlider->setRange(40, 100);
    strengthSlider->setValue(p["glassStrength"].toInt(80));
    strengthSlider->setFixedWidth(180);
    strengthSlider->setAccessibleName("Opacité du verre");
    layout->addWidget(
        settingRow("Matériau du verre", "Transparent à gauche, plus opaque à droite.", strengthSlider));
    auto *colors = new QPushButton("Modifier…");
    layout->addWidget(settingRow("Couleurs de l’interface", "Créez votre propre palette.", colors));
    connect(colors, &QPushButton::clicked, this, [this] { navigate(1); });
    nativeBox = new GlassSwitch(this);
    nativeBox->setAccessibleName("Matériaux natifs de macOS");
    nativeBox->setChecked(p["native"].toBool());
#ifndef Q_OS_MACOS
    nativeBox->setEnabled(false);
    nativeBox->hide();
#else
    layout->addWidget(settingRow("Matériaux du système",
                                 "Adapte le verre et la transparence aux réglages de macOS.", nativeBox));
#endif
    layout->addStretch();
    pages->addWidget(scrollPage(appearance));

    auto *colorPage = newPage(&layout);
    auto *colorBody = new QHBoxLayout;
    colorBody->setSpacing(18);
    layout->addLayout(colorBody);
    auto *colorRows = new QVBoxLayout;
    colorRows->setSpacing(7);
    colorBody->addLayout(colorRows, 1);
    themeName = new QLineEdit(draft["name"].toString());
    themeName->setObjectName("themeName");
    themeName->setMaximumWidth(185);
    colorRows->addWidget(settingRow("Nom", {}, themeName));
    const QList<QPair<QString, QString>> roles{
        {"background", "Fond de fenêtre"}, {"surface", "Contenu"},
        {"text", "Texte principal"},       {"muted", "Texte secondaire"},
        {"accent", "Accent et sélection"}, {"border", "Séparateurs"},
        {"added", "Lignes ajoutées"},      {"removed", "Lignes supprimées"}};
    for (const auto &role : roles) {
        auto *controls = new QWidget;
        auto *row = new QHBoxLayout(controls);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        auto *swatch = new QPushButton;
        swatch->setObjectName("swatch-" + role.first);
        swatch->setFixedSize(36, 32);
        swatch->setAccessibleName("Choisir la couleur : " + role.second);
        row->addWidget(swatch);
        swatches.insert(role.first, swatch);
        auto *hex = new QLineEdit;
        hex->setObjectName("hexField");
        hex->setProperty("colorRole", role.first);
        hex->setFixedWidth(94);
        hex->setMaxLength(7);
        hex->setAccessibleName(role.second + " en hexadécimal");
        colorFields.insert(role.first, hex);
        row->addWidget(hex);
        auto *card = settingRow(role.second, {}, controls);
        card->layout()->setContentsMargins(14, 7, 12, 7);
        colorRows->addWidget(card);
        connect(swatch, &QPushButton::clicked, this, [this, role] {
            auto chosen = pickGlassColor(QColor(draft[role.first].toString()), this);
            if (chosen.isValid())
                changeColor(role.first, chosen);
        });
        connect(hex, &QLineEdit::textEdited, this, [this, role, hex](const QString &value) {
            if (QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch()) {
                changeColor(role.first, QColor(value));
                hex->setToolTip({});
            } else
                hex->setToolTip("Une couleur au format #RRGGBB est nécessaire.");
        });
        connect(hex, &QLineEdit::editingFinished, this, [this] { refreshColors(); });
    }
    auto *previewColumn = new QVBoxLayout;
    previewColumn->setSpacing(14);
    colorBody->addLayout(previewColumn);
    previewColumn->addWidget(caption("Aperçu", "rowTitle"));
    themePreview = new ThemePreview;
    themePreview->setFixedWidth(210);
    previewColumn->addWidget(themePreview);
    auto *previewHint =
        caption("Les couleurs sont prévisualisées ici. Enregistrez pour les appliquer à l’application.",
                "rowSubtitle");
    previewHint->setFixedWidth(210);
    previewColumn->addWidget(previewHint);
    liveAppearance = new GlassSwitch;
    liveAppearance->setObjectName("liveAppearance");
    liveAppearance->setAccessibleName("Aperçu dans la fenêtre");
    liveAppearance->setChecked(p["liveAppearance"].toBool(true));
    previewColumn->addWidget(settingRow("Aperçu dans la fenêtre", {}, liveAppearance));
    connect(liveAppearance, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled)
            appearanceTimer->start();
        else {
            appearanceTimer->stop();
            emit appearancePreviewRequested(profile);
        }
    });
    previewColumn->addStretch();
    colorRows->addStretch();
    layout->addStretch();
    pages->addWidget(scrollPage(colorPage));

    auto *layoutPage = newPage(&layout);
    auto *layoutButton = new QPushButton("Personnaliser…");
    layout->addWidget(settingRow("Disposition de l’espace de travail",
                                 "Déplacez les panneaux, créez des onglets et choisissez vos commandes.",
                                 layoutButton));
    connect(layoutButton, &QPushButton::clicked, this, [this] {
        reject();
        emit layoutEditorRequested();
    });
    layout->addStretch();
    pages->addWidget(scrollPage(layoutPage));

    auto *market = newPage(&layout);
    catalog = new QListWidget;
    catalog->setMinimumHeight(180);
    catalogThemes = themes;
    for (const auto &t : catalogThemes) {
        catalog->addItem(t["name"].toString() + "  ·  " + t["author"].toString());
    }
    layout->addWidget(catalog, 1);
    auto *install = new QPushButton("Choisir ce thème");
    layout->addWidget(install);
    connect(install, &QPushButton::clicked, this, [this] {
        int row = catalog->currentRow();
        if (row < 0 || row >= catalogThemes.size())
            return;
        QString error;
        auto t = catalogThemes[row];
        if (!Theme::install(t, &error)) {
            QMessageBox::warning(this, "Thème", error);
            return;
        }
        loadThemes(t["id"].toString());
        navigate(0);
    });
    auto *themeActions = new QHBoxLayout;
    auto *import = new QPushButton("Importer…"), *exportButton = new QPushButton("Exporter…"),
         *create = new QPushButton("Créer un thème");
    themeActions->addWidget(import);
    themeActions->addWidget(exportButton);
    themeActions->addWidget(create);
    layout->addLayout(themeActions);
    connect(import, &QPushButton::clicked, this, &SettingsDialog::importTheme);
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::exportTheme);
    connect(create, &QPushButton::clicked, this, &SettingsDialog::editTheme);
    catalogUrl = new QLineEdit;
    catalogUrl->setPlaceholderText("https://…/catalog.json");
    catalogUrl->setText(QSettings().value("catalogUrl").toString());
    layout->addWidget(settingRow("Catalogue communautaire", {}, catalogUrl));
    auto *fetch = new QPushButton("Charger le catalogue");
    layout->addWidget(fetch);
    connect(fetch, &QPushButton::clicked, this, &SettingsDialog::fetchCatalog);
    pages->addWidget(scrollPage(market));

    auto *account = newPage(&layout);
    accountLabel = caption("Compte GitHub non vérifié", "rowTitle");
    layout->addWidget(accountLabel);
    auto *loginGitHub = new QPushButton("Se connecter");
    layout->addWidget(settingRow(
        "Compte GitHub",
        "Connexion OAuth dans le navigateur. GitHub CLI gère la session et l’accès Git HTTPS.", loginGitHub));
    connect(loginGitHub, &QPushButton::clicked, this, [this] {
        AccountDialog dialog("gh", github, codex, this);
        dialog.exec();
    });
    auto *accountActions = new QHBoxLayout;
    auto *verify = new QPushButton("Actualiser le compte"), *logoutGitHub = new QPushButton("Déconnecter");
    accountActions->addWidget(verify);
    accountActions->addWidget(logoutGitHub);
    layout->addLayout(accountActions);
    connect(verify, &QPushButton::clicked, this, &SettingsDialog::identify);
    connect(logoutGitHub, &QPushButton::clicked, github, &GitHubClient::logout);
    auto *browse = new QPushButton("Mes dépôts…");
    layout->addWidget(settingRow("Dépôts accessibles",
                                 "Parcourez vos dépôts, ceux de vos organisations et clonez un projet.",
                                 browse));
    connect(browse, &QPushButton::clicked, this, [this] {
        reject();
        emit repositoriesRequested();
    });
    auto *upload = new QPushButton("Envoyer");
    layout->addWidget(settingRow("Sauvegarder mes préférences",
                                 "Le thème et la disposition sont enregistrés dans un Gist secret.", upload));
    connect(upload, &QPushButton::clicked, this, [this] { sync(true); });
    auto *download = new QPushButton("Restaurer");
    layout->addWidget(settingRow("Retrouver mes préférences", {}, download));
    connect(download, &QPushButton::clicked, this, [this] { sync(false); });
    layout->addWidget(caption("Un Gist secret est accessible aux personnes qui connaissent son lien. La "
                              "session GitHub est partagée avec GitHub CLI.",
                              "rowSubtitle"));
    syncStatus = caption({}, "muted");
    layout->addWidget(syncStatus);
    layout->addStretch();
    pages->addWidget(scrollPage(account));
    connect(github, &GitHubClient::accountChanged, this, [this](const QJsonObject &value) {
        login = value["login"].toString();
        accountLabel->setText(login.isEmpty() ? "Aucun compte GitHub connecté" : "Connecté : @" + login);
    });
    connect(github, &GitHubClient::errorOccurred, this,
            [this](const QString &error) { syncStatus->setText(error); });

    auto *accessibility = newPage(&layout);
    motionBox = new GlassSwitch;
    motionBox->setObjectName("reduceMotion");
    motionBox->setAccessibleName("Réduire les animations");
    motionBox->setChecked(p["reduceMotion"].toBool());
    layout->addWidget(settingRow("Réduire les animations",
                                 "Désactive les fondus et les animations des contrôles.", motionBox));
    transparencyBox = new GlassSwitch;
    transparencyBox->setObjectName("reduceTransparency");
    transparencyBox->setAccessibleName("Réduire la transparence");
    transparencyBox->setChecked(p["reduceTransparency"].toBool());
    layout->addWidget(settingRow("Réduire la transparence",
                                 "Utilise des surfaces opaques, sans flou d’arrière-plan.", transparencyBox));
    layout->addWidget(caption(
        "Sur Mac, les préférences d’accessibilité du système sont également respectées.", "rowSubtitle"));
    layout->addStretch();
    pages->addWidget(scrollPage(accessibility));

    auto *codexPage = newPage(&layout);
    codexStatus = caption("Compte Codex non vérifié", "rowTitle");
    layout->addWidget(codexStatus);
    auto *loginCodex = new QPushButton("Se connecter avec ChatGPT");
    layout->addWidget(settingRow(
        "Compte Codex",
        "Connexion OAuth dans le navigateur, prise en charge par le composant officiel Codex.", loginCodex));
    connect(loginCodex, &QPushButton::clicked, this, [this] {
        AccountDialog dialog("codex", github, codex, this);
        dialog.exec();
    });
    auto *codexActions = new QHBoxLayout;
    auto *verifyCodex = new QPushButton("Actualiser le compte"),
         *logoutCodex = new QPushButton("Déconnecter");
    codexActions->addWidget(verifyCodex);
    codexActions->addWidget(logoutCodex);
    layout->addLayout(codexActions);
    connect(verifyCodex, &QPushButton::clicked, codex, &CodexClient::checkAccount);
    connect(logoutCodex, &QPushButton::clicked, codex, &CodexClient::logout);
    layout->addWidget(caption(
        "Le bouton Générer avec Codex analyse les modifications préparées automatiquement pour proposer un message. Aucun "
        "commit n’est créé automatiquement. La génération utilise les limites de votre compte Codex.",
        "muted"));
    layout->addWidget(
        caption("La session est partagée avec Codex CLI : une déconnexion s’applique aussi à cet outil.",
                "rowSubtitle"));
    layout->addStretch();
    pages->addWidget(scrollPage(codexPage));
    connect(codex, &CodexClient::accountChanged, this, [this](const QJsonObject &value) {
        codexStatus->setText(value["type"].toString() == "chatgpt"
                                 ? "Connecté : " + value["email"].toString("Compte ChatGPT") + " · " +
                                       value["planType"].toString()
                                 : "Aucun compte ChatGPT connecté");
    });
    connect(codex, &CodexClient::errorOccurred, this,
            [this](const QString &error) { codexStatus->setText(error); });

    auto *footer = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    footer->setObjectName("settingsButtons");
    footer->button(QDialogButtonBox::Save)->setText("Enregistrer");
    footer->button(QDialogButtonBox::Save)->setIcon({});
    footer->button(QDialogButtonBox::Save)->setObjectName("primary");
    footer->button(QDialogButtonBox::Cancel)->setText("Annuler");
    footer->button(QDialogButtonBox::Cancel)->setIcon({});
    right->addWidget(footer);
    connect(footer, &QDialogButtonBox::accepted, this, [this] {
        QString error;
        if (!Theme::validate(draft, &error)) {
            QMessageBox::warning(this, "Thème", error);
            return;
        }
        if (draft["id"].toString().startsWith("custom-") && !Theme::install(draft, &error)) {
            QMessageBox::warning(this, "Thème", error);
            return;
        }
        emit profileDownloaded(currentProfile());
        accept();
    });
    connect(footer, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(navigation, &QListWidget::currentRowChanged, this, [this] {
        auto *item = navigation->currentItem();
        if (item && item->data(Qt::UserRole).toInt() >= 0)
            navigate(item->data(Qt::UserRole).toInt());
    });
    connect(search, &QLineEdit::textChanged, this, [this](const QString &query) {
        for (int i = 0; i < navigation->count(); ++i) {
            auto *item = navigation->item(i);
            item->setHidden(item->data(Qt::UserRole).toInt() < 0
                                ? !query.isEmpty()
                                : !item->text().contains(query, Qt::CaseInsensitive));
        }
    });
    connect(themeBox, &QComboBox::currentIndexChanged, this, [this, dark, light](int index) {
        if (index < 0 || index >= themes.size())
            return;
        draft = themes[index];
        refreshColors();
        const bool isDark = QColor(draft["surface"].toString()).lightness() < 128;
        dark->setChecked(isDark);
        light->setChecked(!isDark);
    });
    connect(dark, &QPushButton::clicked, this, [this] { loadThemes("obsidian"); });
    connect(light, &QPushButton::clicked, this, [this] { loadThemes("daylight"); });
    connect(themeName, &QLineEdit::textEdited, this, [this](const QString &value) {
        makeLocalTheme();
        draft["name"] = value.left(80);
    });
    auto previewEffects = [this] {
        Visuals::configure(motionBox->isChecked(), transparencyBox->isChecked(), strengthSlider->value());
        update();
    };
    connect(motionBox, &QCheckBox::toggled, this, previewEffects);
    connect(transparencyBox, &QCheckBox::toggled, this, previewEffects);
    connect(strengthSlider, &QSlider::valueChanged, this, [previewEffects] { previewEffects(); });
    loadThemes(draft["id"].toString());
    navigate(0);
}

void SettingsDialog::loadThemes(const QString &selected) {
    themes = Theme::installed();
    const QSignalBlocker block(themeBox);
    themeBox->clear();
    int chosen = -1;
    for (const auto &t : themes) {
        themeBox->addItem(t["name"].toString());
        if (t["id"].toString() == selected)
            chosen = themeBox->count() - 1;
    }
    if (chosen >= 0) {
        themeBox->setCurrentIndex(chosen);
        draft = themes[chosen];
    } else if (Theme::validate(draft)) {
        themeBox->addItem(draft["name"].toString());
        themeBox->setCurrentIndex(themeBox->count() - 1);
    }
    refreshColors();
}

QJsonObject SettingsDialog::currentProfile() const {
    auto p = profile;
    p["theme"] = draft;
    p["native"] = nativeBox->isChecked();
    p["fontSize"] = fontBox->value();
    p["reduceMotion"] = motionBox->isChecked();
    p["reduceTransparency"] = transparencyBox->isChecked();
    p["glassStrength"] = strengthSlider->value();
    p["liveAppearance"] = liveAppearance->isChecked();
    return p;
}

void SettingsDialog::navigate(int index) {
    static const QStringList titles{"Apparence",     "Couleurs",      "Disposition", "Bibliothèque de thèmes",
                                    "Compte GitHub", "Accessibilité", "Compte Codex"};
    static const QStringList subtitles{"Une interface à votre image.",
                                       "Une palette personnelle, avec un aperçu immédiat.",
                                       "Votre espace de travail, à votre façon.",
                                       "Retrouvez vos thèmes et ceux de la communauté.",
                                       "Retrouvez votre thème et votre disposition sur vos appareils.",
                                       "Adaptez les effets visuels à votre confort.",
                                       "Connectez ChatGPT pour générer vos messages de commit."};
    if (index < 0 || index >= titles.size())
        return;
    pageTitle->setText(titles[index]);
    pageSubtitle->setText(subtitles[index]);
    pages->transitionTo(index);
    const QSignalBlocker block(navigation);
    for (int i = 0; i < navigation->count(); ++i)
        if (navigation->item(i)->data(Qt::UserRole).toInt() == index)
            navigation->setCurrentRow(i);
}

void SettingsDialog::makeLocalTheme() {
    if (draft["id"].toString().startsWith("custom-"))
        return;
    draft["id"] = "custom-" + QUuid::createUuid().toString(QUuid::Id128).left(12);
    draft["author"] = "Local";
    draft["name"] = (draft["name"].toString() + " personnalisé").left(80);
}
void SettingsDialog::changeColor(const QString &key, const QColor &color) {
    if (!color.isValid())
        return;
    makeLocalTheme();
    draft[key] = color.name();
    refreshColors();
}
void SettingsDialog::refreshColors() {
    if (appearanceTimer && liveAppearance)
        appearanceTimer->start();
    const bool isDark = QColor(draft["surface"].toString()).lightness() < 128;
    if (auto *mode = findChild<QPushButton *>(isDark ? "modeDark" : "modeLight"))
        mode->setChecked(true);
    for (auto it = colorFields.begin(); it != colorFields.end(); ++it) {
        const QSignalBlocker block(it.value());
        it.value()->setText(draft[it.key()].toString().toUpper());
        swatches[it.key()]->setProperty("swatchColor", QColor(draft[it.key()].toString()));
        swatches[it.key()]->update();
    }
    if (themeName) {
        const QSignalBlocker block(themeName);
        themeName->setText(draft["name"].toString());
    }
    if (themePreview) {
        themePreview->setProperty("previewTheme", draft);
        themePreview->update();
    }
}
void SettingsDialog::done(int result) {
    appearanceTimer->stop();
    if (result != QDialog::Accepted)
        emit appearancePreviewRequested(profile);
    if (result != QDialog::Accepted)
        Visuals::configure(profile["reduceMotion"].toBool(), profile["reduceTransparency"].toBool(),
                           profile["glassStrength"].toInt(80));
    GlassDialog::done(result);
}

void SettingsDialog::importTheme() {
    auto path = QFileDialog::getOpenFileName(this, "Importer un thème", {}, "Thème JSON (*.json)");
    if (path.isEmpty())
        return;
    auto t = Theme::read(path);
    QString error;
    if (!Theme::install(t, &error))
        QMessageBox::warning(this, "Import impossible", error);
    else
        loadThemes(t["id"].toString());
}

void SettingsDialog::exportTheme() {
    if (themeBox->currentIndex() < 0)
        return;
    auto path = QFileDialog::getSaveFileName(this, "Exporter le thème", "theme.json", "Thème JSON (*.json)");
    if (path.isEmpty())
        return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(draft).toJson()) < 0 || !file.commit())
        QMessageBox::warning(this, "Export impossible", file.errorString());
}

void SettingsDialog::editTheme() {
    makeLocalTheme();
    draft["name"] = "Mon thème";
    refreshColors();
    navigate(1);
}

void SettingsDialog::fetchCatalog() {
    const QUrl url(catalogUrl->text());
    if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        QMessageBox::warning(this, "Catalogue", "Une URL HTTPS valide est nécessaire.");
        return;
    }
    QSettings().setValue("catalogUrl", url.toString());
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network.get(request);
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, data] {
        data->append(reply->readAll());
        if (data->size() > 1024 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data] {
        reply->deleteLater();
        data->append(reply->readAll());
        const auto document = QJsonDocument::fromJson(*data);
        if (reply->error() != QNetworkReply::NoError || data->size() > 1024 * 1024 || !document.isArray() ||
            document.array().size() > 200) {
            QMessageBox::warning(this, "Catalogue", "Catalogue illisible, trop volumineux, ou inaccessible.");
            return;
        }
        QList<QJsonObject> valid;
        for (const auto &entry : document.array()) {
            if (!Theme::validate(entry.toObject())) {
                QMessageBox::warning(this, "Catalogue", "Un thème est invalide. Catalogue refusé.");
                return;
            }
            valid.append(entry.toObject());
        }
        catalogThemes = valid;
        catalog->clear();
        for (const auto &t : valid)
            catalog->addItem(t["name"].toString() + "  ·  " + t["author"].toString());
    });
}

void SettingsDialog::identify() {
    github->checkAccount();
}

void SettingsDialog::sync(bool upload) {
    if (runner.busy())
        return;
    if (login.isEmpty()) {
        syncStatus->setText("Vérifiez d’abord le compte GitHub.");
        return;
    }
    syncStatus->setText("Recherche des préférences…");
    // Paginate in gh; its output is a stream of arrays with --slurp.
    runner.run(
        "gh", {"api", "--hostname", "github.com", "--paginate", "--slurp", "gists?per_page=100"}, {},
        [this, upload](const CommandResult &r) {
            if (!r.ok()) {
                syncStatus->setText(r.message().left(1000));
                return;
            }
            QString gist;
            for (const auto &page : QJsonDocument::fromJson(r.output).array())
                for (const auto &value : page.toArray()) {
                    auto o = value.toObject();
                    if (!o["public"].toBool(true) &&
                        o["description"].toString() == "GitHub Enhanced preferences v1" &&
                        o["owner"].toObject()["login"].toString() == login &&
                        o["files"].toObject().contains("github-enhanced.json")) {
                        gist = o["id"].toString();
                        break;
                    }
                    if (!gist.isEmpty())
                        break;
                }
            if (!upload && gist.isEmpty()) {
                syncStatus->setText("Aucune préférence trouvée pour @" + login);
                return;
            }
            if (upload) {
                QJsonObject payload{
                    {"description", "GitHub Enhanced preferences v1"},
                    {"public", false},
                    {"files",
                     QJsonObject{{"github-enhanced.json",
                                  QJsonObject{{"content", QString::fromUtf8(
                                                              QJsonDocument(currentProfile()).toJson())}}}}}};
                QStringList args{"api",
                                 "--hostname",
                                 "github.com",
                                 "--method",
                                 gist.isEmpty() ? "POST" : "PATCH",
                                 gist.isEmpty() ? "gists" : "gists/" + gist,
                                 "--input",
                                 "-"};
                runner.run(
                    "gh", args, {},
                    [this](const CommandResult &result) {
                        syncStatus->setText(result.ok() ? "Préférences envoyées pour @" + login
                                                        : result.message().left(1000));
                    },
                    QJsonDocument(payload).toJson(QJsonDocument::Compact));
            } else {
                runner.run(ToolRuntime::executable("gh"),
                           {"api", "--hostname", "github.com", "gists/" + gist}, {},
                           [this](const CommandResult &result) {
                               auto file = QJsonDocument::fromJson(result.output)
                                               .object()["files"]
                                               .toObject()["github-enhanced.json"]
                                               .toObject();
                               auto data = file["content"].toString().toUtf8();
                               auto p = QJsonDocument::fromJson(data).object();
                               if (!result.ok() || file["truncated"].toBool() || data.size() > 262144 ||
                                   p["version"].toInt() != 1 || !Theme::validate(p["theme"].toObject())) {
                                   syncStatus->setText("Préférences invalides ou téléchargement impossible.");
                                   return;
                               }
                               emit profileDownloaded(p);
                               accept();
                           });
            }
        });
}
