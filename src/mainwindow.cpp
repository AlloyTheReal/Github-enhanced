#include "mainwindow.h"
#include "custompanel.h"
#include "updatedialog.h"
#include "mergedialog.h"
#include "diagnostics.h"
#include "settingsdialog.h"
#include "layoutdialog.h"
#include "visuals.h"
#include "repobrowser.h"
#include "repositorycreateform.h"
#include "releasesdialog.h"
#include "reposettings.h"
#include "accountdialog.h"
#include "toolruntime.h"
#include "theme.h"
#include <QtWidgets>
#include <QDesktopServices>

namespace {
class MenuTextButton : public QToolButton {
public:
    using QToolButton::QToolButton;
    QSize sizeHint() const override { auto size=QToolButton::sizeHint();size.setWidth(qMax(size.width(),fontMetrics().horizontalAdvance(text())+28));return size; }
    QSize minimumSizeHint() const override { return sizeHint(); }
};
QIcon commandIcon(const QString &kind) {
    QPixmap image(40, 40); image.setDevicePixelRatio(2); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(qApp->palette().color(QPalette::WindowText), 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    auto arrow = [&](bool up) { const int tip = up ? 4 : 16, tail = up ? 15 : 5, wing = up ? 8 : 12; p.drawLine(10, tail, 10, tip); p.drawLine(6, wing, 10, tip); p.drawLine(14, wing, 10, tip); };
    if (kind == "layout") { p.drawRoundedRect(QRectF(2, 3, 16, 14), 2, 2); p.drawLine(8, 3, 8, 17); p.drawLine(2, 12, 8, 12); }
    else if (kind == "push" || kind == "publish") arrow(true);
    else if (kind == "pull") arrow(false);
    else if (kind == "fetch" || kind == "refresh" || kind == "sync") { p.drawArc(QRectF(3, 3, 14, 14), 35*16, 285*16); p.drawLine(17, 2, 17, 7); p.drawLine(12, 7, 17, 7); }
    else if (kind == "commit") { p.drawLine(2, 10, 6, 10); p.drawLine(14, 10, 18, 10); p.drawEllipse(QRectF(6, 6, 8, 8)); }
    else if (kind == "stage" || kind == "unstage" || kind == "init") { p.drawRoundedRect(QRectF(3, 3, 14, 14), 3, 3); p.drawLine(6, 10, 14, 10); if (kind != "unstage") p.drawLine(10, 6, 10, 14); }
    else if (kind == "new-branch") { p.drawEllipse(QRectF(3, 2, 4, 4)); p.drawEllipse(QRectF(3, 14, 4, 4)); p.drawEllipse(QRectF(13, 2, 4, 4)); p.drawLine(5, 6, 5, 14); QPainterPath path; path.moveTo(5, 13); path.cubicTo(5, 8, 15, 12, 15, 6); p.drawPath(path); }
    else if (kind == "stash" || kind == "apply-stash") { p.drawRoundedRect(QRectF(2, 10, 16, 8), 2, 2); p.drawLine(6, 14, 14, 14); p.drawLine(10, 2, 10, 9); int y = kind == "stash" ? 9 : 2; int wing = kind == "stash" ? 5 : 6; p.drawLine(7, wing, 10, y); p.drawLine(13, wing, 10, y); }
    else if (kind == "preferences") { p.drawEllipse(QRectF(4, 4, 12, 12)); p.drawEllipse(QRectF(8, 8, 4, 4)); for (int a = 0; a < 8; ++a) { p.save(); p.translate(10, 10); p.rotate(a*45); p.drawLine(0, -6, 0, -9); p.restore(); } }
    else if (kind == "origin") { p.save(); p.translate(10, 10); p.rotate(-40); p.drawRoundedRect(QRectF(-8, -4, 10, 8), 4, 4); p.drawRoundedRect(QRectF(-2, -4, 10, 8), 4, 4); p.restore(); }
    else if (kind == "clone") { p.drawRoundedRect(QRectF(2, 2, 10, 13), 1, 1); p.drawRoundedRect(QRectF(8, 6, 10, 12), 1, 1); }
    else { QPainterPath folder; folder.moveTo(2, 16); folder.lineTo(2, 4); folder.lineTo(8, 4); folder.lineTo(10, 6); folder.lineTo(18, 6); folder.lineTo(18, 16); folder.closeSubpath(); p.drawPath(folder); }
    return QIcon(image);
}
QLabel *label(const QString &text, const char *name = nullptr) {
    auto *l = new QLabel(text); if (name) l->setObjectName(name); return l;
}
QPushButton *button(const QString &text, QLayout *layout, std::function<void()> callback, QObject *context, bool primary = false) {
    auto *b = new QPushButton(text); if (primary) b->setObjectName("primary");
    layout->addWidget(b); QObject::connect(b, &QPushButton::clicked, context, std::move(callback)); return b;
}
QWidget *dockHeader(QDockWidget *dock) {
    auto *bar=new QWidget(dock);bar->setObjectName("panelTitleBar");auto *row=new QHBoxLayout(bar);row->setContentsMargins(12,4,6,4);row->setSpacing(4);auto *title=new QLabel(dock->windowTitle());title->setAttribute(Qt::WA_TransparentForMouseEvents);title->setObjectName("caption");row->addWidget(title,1);QObject::connect(dock,&QDockWidget::windowTitleChanged,title,&QLabel::setText);
    auto *detach=new QToolButton;detach->setAutoRaise(true);detach->setIcon(commandIcon("layout"));detach->setToolTip("Détacher ou rattacher ce panneau");detach->setAccessibleName(detach->toolTip());detach->setVisible(dock->features()&QDockWidget::DockWidgetFloatable);row->addWidget(detach);QObject::connect(detach,&QToolButton::clicked,dock,[dock]{dock->setFloating(!dock->isFloating());dock->show();});
    auto *close=new QToolButton;close->setAutoRaise(true);close->setText("×");close->setToolTip("Masquer ce panneau");close->setAccessibleName(close->toolTip());close->setVisible(dock->features()&QDockWidget::DockWidgetClosable);row->addWidget(close);QObject::connect(close,&QToolButton::clicked,dock,&QWidget::hide);return bar;
}
QDockWidget *dock(QMainWindow *window, const QString &title, const QString &id, QWidget *content, Qt::DockWidgetArea area) {
    auto *d = new QDockWidget(title, window); d->setObjectName(id); d->setWidget(content);
    window->addDockWidget(area, d); return d;
}
}

MainWindow::MainWindow() : runner(this), diffRunner(this), repositoryProbe(this), github(this), codex(this), indexer(&runner,this), syncMonitor(this), pushCheck(this) {
    runner.setEnvironmentRemovals({"GH_TOKEN","GITHUB_TOKEN","GH_ENTERPRISE_TOKEN","GITHUB_ENTERPRISE_TOKEN"});
    repositoryProbe.setObjectName("repositoryProbe");repositoryProbe.setTimeoutMilliseconds(15000);repositoryTimer.setParent(this);repositoryTimer.setObjectName("repositoryPollTimer");
    updater=new AppUpdater(this);
    connect(updater,&AppUpdater::checked,this,[this](const AppUpdate &update,const QString &,bool manual){if(!manual&&update.available()){if(!updateDialog){updateDialog=new UpdateDialog(updater,this);connect(updateDialog,&UpdateDialog::installRequested,this,&MainWindow::installUpdate);}updateDialog->show();updateDialog->raise();}});
    setWindowTitle("GitHub Enhanced"); resize(1200, 780); setMinimumSize(860, 580);
    setDockOptions(AllowNestedDocks | AllowTabbedDocks | AnimatedDocks);
    const bool migrated = settings.value("interfaceRevision", 0).toInt() >= 3;
    activeTheme = Theme::read(":/themes/obsidian.json");
    const auto stored = QJsonDocument::fromJson(settings.value("theme").toByteArray()).object();
    if (Theme::validate(stored) && (migrated || !QStringList{"midnight", "forest", "daylight", "obsidian"}.contains(stored["id"].toString()))) activeTheme = stored;
#ifdef Q_OS_MACOS
    native = settings.value("native", true).toBool(); setUnifiedTitleAndToolBarOnMac(true);
#else
    native = settings.value("native", false).toBool();
#endif
    fontSize = qBound(10, settings.value("fontSize", 11).toInt(), 22);
    Visuals::configure(settings.value("reduceMotion", false).toBool(), settings.value("reduceTransparency", false).toBool(), settings.value("glassStrength", 80).toInt());
    Theme::apply(activeTheme, native, fontSize);
    buildInterface(); buildMenus();
    operation = new OperationDialog(this);
    connect(&runner,&CommandRunner::commandStarted,this,[this](const QString &,const QStringList &args){
        if(operation->running()){
            QString step=args.value(0);
            if(step=="--no-pager")step=args.value(1);
            const QMap<QString,QString> labels{{"add","Préparation des fichiers"},{"commit","Création du commit"},{"push","Envoi vers le dépôt distant"},{"fetch","Récupération des références"},{"pull","Mise à jour du dépôt"},{"clone","Téléchargement du dépôt"},{"status","Analyse des modifications"},{"track","Suivi des gros fichiers avec Git LFS"},{"install","Configuration de Git LFS"},{"diff","Lecture des modifications"}};
            operation->step(labels.value(step,"Vérification du dépôt…"));
        }
    });
    connect(&runner,&CommandRunner::outputReceived,operation,&OperationDialog::append);
    connect(&pushCheck,&PushPreflight::detail,operation,&OperationDialog::step);
    connect(&pushCheck,&PushPreflight::output,operation,&OperationDialog::append);
    connect(&pushCheck,&PushPreflight::busyChanged,this,[this](bool){updateActions();});
    connect(&indexer,&RepositoryIndex::detail,this,[this](const QString &text){if(quietRefresh)return;if(!operation->running())beginOperation("Préparation du commit");operation->step(text);});
    connect(&repositoryTimer,&QTimer::timeout,this,&MainWindow::probeRepository);
    repositoryTimer.start(2500);
    connect(&syncMonitor,&RepositorySync::updated,this,&MainWindow::updateSyncStatus);
    connect(&syncMonitor,&RepositorySync::busyChanged,this,[this](bool busy){verifyRemote->setEnabled(!busy&&!repository.isEmpty());verifyRemote->setText(busy?"Vérification…":"Vérifier le distant");if(syncToolbarAction){syncToolbarAction->setEnabled(!busy&&!repository.isEmpty()&&!runner.busy());if(busy)syncToolbarAction->setText("Fetch en cours…");else updateSyncStatus(syncState);}updateActions();});
    connect(&remoteTimer,&QTimer::timeout,this,[this]{if(!layoutEditorOpen()&&!property("installingUpdate").toBool()&&!QApplication::activeModalWidget()&&!repository.isEmpty()&&!pushCheck.busy()&&!runner.busy()&&!indexer.busy()&&!syncMonitor.busy())syncMonitor.inspect(repository,true);});remoteTimer.start(60000);
    auto *activityButton=new QPushButton("Activité");activityButton->setObjectName("operationActivity");statusBar()->addPermanentWidget(activityButton);connect(activityButton,&QPushButton::clicked,operation,[this]{operation->show();operation->raise();});
    if (settings.value("interfaceRevision", 0).toInt() >= 2) {
        if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
        auto configuration = QJsonDocument::fromJson(settings.value("workspace").toByteArray()).object();
        if (!configuration.isEmpty()) applyLayoutConfiguration(configuration);
    }
    settings.setValue("interfaceRevision", 3);
    settings.setValue("theme", QJsonDocument(activeTheme).toJson(QJsonDocument::Compact));
    settings.setValue("fontSize", fontSize);
    recent = settings.value("recent").toStringList(); rebuildRecent();
    connect(&runner, &CommandRunner::busyChanged, this, [this](bool busy) {
        if(!busy&&!pushCheck.busy()&&!github.busy()&&!indexer.busy()&&!codex.generating()&&operation->running())operation->finish(indexError.isEmpty(),indexError);
        updateActions(); if(!quietRefresh)statusBar()->showMessage(busy ? "Opération en cours…" : repository.isEmpty() ? "Choisissez un dépôt" : repository);
    });
    connect(&github,&GitHubClient::busyChanged,this,[this](bool busy){if(!busy&&!pushCheck.busy()&&!runner.busy()&&!indexer.busy()&&!codex.generating()&&operation->running())operation->finish(indexError.isEmpty(),indexError);updateActions();});
    updateActions();
    connect(&codex,&CodexClient::generationChanged,this,[this](bool active){generateButton->setText(active?"Annuler la génération":"Générer avec Codex");generationStatus->setText(active?"Codex rédige une proposition…":QString());if(active){beginOperation("Génération du message de commit");operation->step("Codex analyse les modifications préparées…");}else if(operation->running())operation->finish(true);});
    connect(&codex,&CodexClient::errorOccurred,this,[this](const QString &message){operation->finish(false,message);generationStatus->setError(message);generateButton->setText("Générer avec Codex");});
    connect(&codex,&CodexClient::progressDetail,this,[this](const QString &text){if(codex.generating())operation->step(text);});
    connect(&codex,&CodexClient::messageGenerated,this,&MainWindow::applyGeneratedMessage);
}

void MainWindow::buildInterface() {
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setDocumentMode(true);
    mainToolbar = addToolBar("Barre d’outils"); mainToolbar->setObjectName("mainToolbar");
    mainToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mainToolbar->setIconSize(QSize(16, 16));
    auto identify = [this](QAction *action, const QString &id, const QString &title) {
        action->setObjectName(id); action->setProperty("customTitle", title);
        action->setProperty("defaultVisible", id != "tool-refresh"); toolbarItems.append(action);
        if (!qobject_cast<QWidgetAction *>(action)) action->setIcon(commandIcon(id.mid(5)));
    };
    openAction = new QAction("Ouvrir un dépôt…", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        auto path = QFileDialog::getExistingDirectory(this, "Ouvrir un dépôt Git"); if (!path.isEmpty()) openRepository(path);
    });
    repositoryMenu = new QMenu(this);
    repositorySelector = new QToolButton; repositorySelector->setObjectName("repositorySelector");
    repositorySelector->setText("Choisir un dépôt"); repositorySelector->setPopupMode(QToolButton::InstantPopup);
    repositorySelector->setMenu(repositoryMenu); repositorySelector->setMinimumWidth(220);
    repositorySelector->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    repositorySelector->setIcon(commandIcon("repository"));
    auto *repoBlock = new QWidget; repoBlock->setObjectName("toolbarBlock");
    auto *repoBlockLayout = new QVBoxLayout(repoBlock); repoBlockLayout->setContentsMargins(10, 4, 16, 4); repoBlockLayout->setSpacing(1);
    repoBlockLayout->addWidget(label("Dépôt actuel", "caption")); repoBlockLayout->addWidget(repositorySelector);
    identify(mainToolbar->addWidget(repoBlock), "tool-repository", "Sélecteur de dépôt");
    auto *branchBlock = new QWidget; branchBlock->setObjectName("toolbarBlock");
    auto *branchLayout = new QVBoxLayout(branchBlock); branchLayout->setContentsMargins(16, 4, 20, 4); branchLayout->setSpacing(1);
    branchLayout->addWidget(label("Branche actuelle", "caption"));
    branches = new GlassComboBox; branches->setObjectName("branchSelector"); branches->setMinimumWidth(175);
    branches->setAccessibleName("Branche courante"); branches->setPlaceholderText("Aucune branche");
    branchLayout->addWidget(branches);
    identify(mainToolbar->addWidget(branchBlock), "tool-branch", "Sélecteur de branche");
    connect(branches, &QComboBox::activated, this, [this](int index) {
        auto name = branches->itemData(index).toString();
        if (!name.isEmpty() && name != branch && !runner.busy()) mutate({"switch", "--", name}, "Changement de branche");
    });
    auto addRepo = [&](const QString &name, const QString &id, QStyle::StandardPixmap icon, const QStringList &args) {
        auto *a = mainToolbar->addAction(style()->standardIcon(icon), name, this, [this, args, name] { if(args.value(0)=="push"&&QStringList{"ahead","noUpstream","localOnly"}.contains(syncState.kind))performSyncAction();else mutate(args, name); });
        identify(a, id, name); repoActions.append(a); return a;
    };
    auto *legacyFetch=addRepo("Fetch", "tool-fetch", QStyle::SP_BrowserReload, {"fetch", "--prune"});
    auto *legacyPull=addRepo("Pull", "tool-pull", QStyle::SP_ArrowDown, {"pull", "--ff-only"});
    auto *legacyPush=addRepo("Push", "tool-push", QStyle::SP_ArrowUp, {"push"});
    for(auto *legacy:{legacyFetch,legacyPull,legacyPush}){legacy->setVisible(false);legacy->setProperty("defaultVisible",false);}
    syncToolbarAction=mainToolbar->addAction(commandIcon("fetch"),"Fetch origin",this,&MainWindow::performSyncAction);identify(syncToolbarAction,"tool-sync","Synchroniser le dépôt");
    refreshAction = mainToolbar->addAction("Actualiser", this, &MainWindow::refresh);
    refreshAction->setShortcut(QKeySequence::Refresh); repoActions.append(refreshAction);
    identify(refreshAction, "tool-refresh", "Actualiser"); refreshAction->setVisible(false);
    auto *spacer = new QWidget; spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    identify(mainToolbar->addWidget(spacer), "tool-space", "Espace flexible");
    auto *layoutAction = mainToolbar->addAction("Disposition", this, &MainWindow::editLayout);
    identify(layoutAction, "tool-layout", "Personnaliser la disposition");

    auto *repoWidget = new QWidget; repoWidget->setObjectName("sidebar"); auto *repoLayout = new QVBoxLayout(repoWidget);
    repoLayout->setContentsMargins(0, 0, 0, 0); repoLayout->setSpacing(0);
    repositories = new QListWidget; repositories->setObjectName("recentRepositories"); repositories->setAccessibleName("Dépôts récents");
    repositories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); repoLayout->addWidget(repositories);
    connect(repositories, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { openRepository(item->data(Qt::UserRole).toString()); });
    repositoryDock = dock(this, "Dépôts récents", "repositories", repoWidget, Qt::LeftDockWidgetArea);
    repositoryDock->setMinimumWidth(180);

    // Every workspace view, including the primary diff, is a movable dock.
    // A zero-size central widget lets docks occupy the entire content area.
    auto *center = new QWidget; center->setFixedSize(0, 0); setCentralWidget(center);
    pages = new QStackedWidget;
    auto *welcome = new QWidget; welcome->setObjectName("welcome");
    auto *welcomeOuter = new QVBoxLayout(welcome); welcomeOuter->addStretch();
    auto *card = new QWidget; card->setMaximumWidth(400); auto *welcomeLayout = new QVBoxLayout(card);
    welcomeLayout->setSpacing(12);
    auto *title = label("GitHub Enhanced", "headline"); title->setAlignment(Qt::AlignCenter); welcomeLayout->addWidget(title);
    auto *intro = label("Ouvrez un dépôt pour commencer.", "muted"); intro->setAlignment(Qt::AlignCenter); welcomeLayout->addWidget(intro);
    welcomeLayout->addSpacing(12);
    button("Ouvrir un dépôt local…", welcomeLayout, [this] { openAction->trigger(); }, this, true);
    button("Cloner un dépôt…", welcomeLayout, [this] { cloneRepository(); }, this);
    button("Mes dépôts GitHub…", welcomeLayout, [this] { browseGitHub(); }, this);
    auto *create = button("Créer un dépôt local…", welcomeLayout, [this] { initRepository(); }, this);
    create->setFlat(true);
    welcomeOuter->addWidget(card, 0, Qt::AlignHCenter); welcomeOuter->addStretch(); pages->addWidget(welcome);

    auto *diffPage = new QWidget; diffPage->setObjectName("diffPage");
    auto *diffLayout = new QVBoxLayout(diffPage); diffLayout->setContentsMargins(0, 0, 0, 0); diffLayout->setSpacing(0);
    auto *syncBanner=new QWidget;syncBanner->setObjectName("syncBanner");auto *syncLayout=new QHBoxLayout(syncBanner);syncLayout->setContentsMargins(16,10,16,10);syncLayout->setSpacing(12);
    syncStatus=new QLabel("Aucun dépôt sélectionné");syncStatus->setObjectName("syncStatus");syncStatus->setTextFormat(Qt::PlainText);syncStatus->setWordWrap(true);syncLayout->addWidget(syncStatus,1);
    verifyRemote=new QPushButton("Vérifier le distant");verifyRemote->setObjectName("verifyRemote");syncLayout->addWidget(verifyRemote);connect(verifyRemote,&QPushButton::clicked,this,[this]{syncMonitor.inspect(repository,true);});
    syncAction=new QPushButton;syncAction->setObjectName("syncAction");syncLayout->addWidget(syncAction);syncAction->hide();connect(syncAction,&QPushButton::clicked,this,&MainWindow::performSyncAction);diffLayout->addWidget(syncBanner);
    auto *diffHeader = new QWidget; diffHeader->setObjectName("sectionHeader");
    auto *headerLayout = new QHBoxLayout(diffHeader); headerLayout->setContentsMargins(16, 10, 16, 10);
    diffTitle = label("Aucune modification", "fileTitle"); diffTitle->setTextFormat(Qt::PlainText);
    diffTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); headerLayout->addWidget(diffTitle, 1);
    auto *workingView = new QPushButton("Modifications actuelles");workingView->setObjectName("workingChanges");headerLayout->addWidget(workingView);
    connect(workingView,&QPushButton::clicked,this,[this]{selectedCommit.clear();commitBrowser->hide();diffDock->raise();showDiff();});
    diffLayout->addWidget(diffHeader);

    diff = new QPlainTextEdit; diff->setObjectName("diffEditor"); diff->setReadOnly(true);
    diff->setLineWrapMode(QPlainTextEdit::NoWrap); diff->setFrameShape(QFrame::NoFrame);
    auto codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont); codeFont.setPointSize(11); diff->setFont(codeFont);
    diff->setAccessibleName("Diff du fichier ou du commit"); auto *contentRow=new QSplitter(Qt::Horizontal);contentRow->setObjectName("commitContentSplit");contentRow->addWidget(diff);
    commitBrowser=new QWidget;commitBrowser->setObjectName("commitBrowser");commitBrowser->setMinimumWidth(230);commitBrowser->setMaximumWidth(420);
    auto *commitBrowserLayout=new QVBoxLayout(commitBrowser);commitBrowserLayout->setContentsMargins(14,14,14,14);commitBrowserLayout->setSpacing(12);
    commitSubject=new QLabel;commitSubject->setObjectName("commitSubject");commitSubject->setTextFormat(Qt::PlainText);commitSubject->setWordWrap(true);auto subjectFont=commitSubject->font();subjectFont.setBold(true);commitSubject->setFont(subjectFont);commitBrowserLayout->addWidget(commitSubject);
    commitMeta=new QLabel;commitMeta->setObjectName("muted");commitMeta->setTextFormat(Qt::PlainText);commitMeta->setWordWrap(true);commitMeta->setTextInteractionFlags(Qt::TextSelectableByMouse);commitBrowserLayout->addWidget(commitMeta);
    commitInfo=new QLabel;commitInfo->setObjectName("commitInfo");commitInfo->setTextFormat(Qt::PlainText);commitInfo->setWordWrap(true);commitInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);commitBrowserLayout->addWidget(commitInfo);
    commitView=new GlassComboBox;commitView->setObjectName("commitFileView");commitView->addItems({"Contenu du fichier","Modifications du fichier"});commitBrowserLayout->addWidget(commitView);
    commitFiles=new QTreeWidget;commitFiles->setObjectName("commitFiles");commitFiles->setHeaderHidden(true);commitFiles->setRootIsDecorated(false);commitFiles->setAccessibleName("Fichiers du commit");commitBrowserLayout->addWidget(commitFiles,1);
    contentRow->addWidget(commitBrowser);contentRow->setStretchFactor(0,1);contentRow->setSizes({800,280});commitBrowser->hide();diffLayout->addWidget(contentRow,1);pages->addWidget(diffPage);
    connect(commitFiles,&QTreeWidget::itemSelectionChanged,this,&MainWindow::showCommitFile);connect(commitView,&QComboBox::activated,this,[this]{showCommitFile();});
    diffDock = dock(this, "Diff", "diff", pages, Qt::RightDockWidgetArea);
    diffDock->setMinimumWidth(300);

    history = new QTreeWidget; history->setObjectName("commitHistory"); history->setHeaderLabels({"Historique"});
    history->setRootIsDecorated(false); history->setHeaderHidden(true);
    history->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    historyDock = dock(this, "Historique", "history", history, Qt::LeftDockWidgetArea);
    connect(history, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {showCommit(item->data(0,Qt::UserRole).toString());});

    auto *changesWidget = new QWidget; changesWidget->setObjectName("sidebar");
    auto *changesLayout = new QVBoxLayout(changesWidget); changesLayout->setContentsMargins(0, 0, 0, 0); changesLayout->setSpacing(0);
    changeCount = label("Aucun dépôt sélectionné", "changeCount"); changeCount->setMargin(12);changeCount->setToolTip("Les fichiers exclus par .gitignore restent sur le disque et ne font pas partie des commits."); changesLayout->addWidget(changeCount);
    files = new QTreeWidget; files->setObjectName("changedFiles"); files->setHeaderLabels({"État", "Fichier"});
    files->setRootIsDecorated(false); files->setHeaderHidden(true); files->setColumnWidth(0, 38);
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    files->header()->setSectionResizeMode(1, QHeaderView::Stretch); files->setAccessibleName("Fichiers modifiés"); changesLayout->addWidget(files);
    connect(files, &QTreeWidget::itemSelectionChanged, this, [this]{selectedCommit.clear();commitBrowser->hide();diffDock->raise();showDiff();});
    changesDock = dock(this, "Modifications", "changes", changesWidget, Qt::LeftDockWidgetArea); changesDock->setMinimumWidth(250);

    auto *commitWidget = new QWidget; commitWidget->setObjectName("sidebar"); auto *commitLayout = new QVBoxLayout(commitWidget);
    commitLayout->setContentsMargins(12, 12, 12, 12); commitLayout->setSpacing(8);
    summary = new QLineEdit; summary->setObjectName("commitSummary"); summary->setPlaceholderText("Résumé du commit"); summary->setAccessibleName("Résumé du commit");
    description = new QPlainTextEdit; description->setObjectName("commitDescription"); description->setPlaceholderText("Description (facultative)");
    description->setMinimumHeight(56); description->setMaximumHeight(100);
    commitLayout->addWidget(summary); commitLayout->addWidget(description);
    generateButton=button("Générer avec Codex",commitLayout,[this]{generateMessage();},this);generateButton->setObjectName("generateCommitMessage");generateButton->setToolTip("Envoie un résumé des modifications à votre session Codex pour proposer un message. Aucun commit n’est créé automatiquement.");
    generationStatus=new DiagnosticLabel;generationStatus->setWordWrap(true);generationStatus->setObjectName("generationStatus");commitLayout->addWidget(generationStatus);
    commitButton = button("Créer le commit", commitLayout, [this] { commit(); }, this, true); commitButton->setObjectName("primary");
    commitButton->setToolTip("Créer un commit avec tous les fichiers modifiés");
    commitDock = dock(this, "Commit", "commit", commitWidget, Qt::LeftDockWidgetArea);
    activity = new QPlainTextEdit; activity->setReadOnly(true); activity->setFrameShape(QFrame::NoFrame);
    activity->setMaximumBlockCount(1500); activity->setFont(codeFont);
    activityDock = dock(this, "Journal", "activity", activity, Qt::BottomDockWidgetArea);
    statusBar()->setSizeGripEnabled(false); statusBar()->showMessage("Choisissez un dépôt");
    resetLayout();
}

void MainWindow::buildMenus() {
    auto *fileMenu = menuBar()->addMenu("&Fichier");
    auto *toolbar = findChild<QToolBar *>("mainToolbar");
    fileMenu->addAction(openAction);
    fileMenu->addAction("Cloner un dépôt…", QKeySequence("Ctrl+Shift+O"), this, &MainWindow::cloneRepository);
    fileMenu->addAction("Créer un dépôt…", this, &MainWindow::initRepository);
    fileMenu->addAction("Mes dépôts GitHub…",this,&MainWindow::browseGitHub);
    auto *preferences = fileMenu->addAction("Préférences…", QKeySequence::Preferences, this, &MainWindow::settingsDialog); preferences->setMenuRole(QAction::PreferencesRole);
    preferences->setObjectName("preferencesAction");
    auto *colorsAction=fileMenu->addAction("Couleurs et palettes…",this,[this]{openSettings(true);});colorsAction->setObjectName("colorsSettingsAction");
    auto *quick=new QWidget(menuBar());quick->setStyleSheet("QToolButton { padding: 2px 8px; min-height: 20px; }");auto *quickRow=new QHBoxLayout(quick);quickRow->setContentsMargins(0,0,6,0);quickRow->setSpacing(4);
    auto *restoreToolbar=new MenuTextButton(quick);restoreToolbar->setObjectName("restoreToolbarButton");restoreToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);restoreToolbar->setText("Afficher la barre d’outils");restoreToolbar->setAutoRaise(true);restoreToolbar->setToolTip("Réafficher les commandes du dépôt");restoreToolbar->setAccessibleName(restoreToolbar->text());quickRow->addWidget(restoreToolbar);
    connect(restoreToolbar,&QToolButton::clicked,this,[this]{bool any=false;for(auto *action:toolbarItems)any|=action->isVisible();if(!any)for(auto *action:toolbarItems)action->setVisible(action->property("defaultVisible").toBool());mainToolbar->show();mainToolbar->raise();saveLayoutSettings();});
    connect(mainToolbar,&QToolBar::visibilityChanged,restoreToolbar,[restoreToolbar](bool visible){restoreToolbar->setVisible(!visible);});restoreToolbar->setVisible(mainToolbar->isHidden());QTimer::singleShot(0,this,[this,restoreToolbar]{restoreToolbar->setVisible(mainToolbar->isHidden());if(menuBar()->isNativeMenuBar()&&mainToolbar->isHidden())statusBar()->show();});
    auto *colorsButton=new MenuTextButton(quick);colorsButton->setObjectName("settingsButton");colorsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);colorsButton->setText("Paramètres…");colorsButton->setAutoRaise(true);colorsButton->setToolTip("Ouvrir les paramètres de l’application");colorsButton->setAccessibleName("Paramètres de l’application");quickRow->addWidget(colorsButton);connect(colorsButton,&QToolButton::clicked,preferences,&QAction::trigger);if(menuBar()->isNativeMenuBar()){quick->setParent(statusBar());statusBar()->addPermanentWidget(quick);connect(mainToolbar,&QToolBar::visibilityChanged,this,[this](bool visible){if(!visible)statusBar()->show();});}else menuBar()->setCornerWidget(quick,Qt::TopRightCorner);
    auto fitQuick=[this,quick,quickRow,restoreToolbar,colorsButton]{restoreToolbar->updateGeometry();colorsButton->updateGeometry();quickRow->invalidate();quick->adjustSize();quick->updateGeometry();if(!menuBar()->isNativeMenuBar())menuBar()->setCornerWidget(quick,Qt::TopRightCorner);};
    connect(mainToolbar,&QToolBar::visibilityChanged,quick,[quick,fitQuick](bool){QTimer::singleShot(0,quick,fitQuick);});QTimer::singleShot(0,quick,fitQuick);


    fileMenu->addSeparator(); auto *quit = fileMenu->addAction("Quitter", QKeySequence::Quit, this, &QWidget::close); quit->setMenuRole(QAction::QuitRole);
    auto *edit = menuBar()->addMenu("&Édition");
    // Standard shortcuts still belong to each native text control.
    edit->addAction("Copier", QKeySequence::Copy, this, [] { if (auto *w = QApplication::focusWidget()) QMetaObject::invokeMethod(w, "copy"); });
    edit->addAction("Coller", QKeySequence::Paste, this, [] { if (auto *w = QApplication::focusWidget()) QMetaObject::invokeMethod(w, "paste"); });
    edit->addAction("Tout sélectionner", QKeySequence::SelectAll, this, [] { if (auto *w = QApplication::focusWidget()) QMetaObject::invokeMethod(w, "selectAll"); });
    auto *repoMenu = menuBar()->addMenu("&Dépôt");
    QList<QAction *> menuCommands;
    for (auto *action : repoActions) {
        // Toolbar visibility must never hide the same command from the menu.
        auto *menuCommand = repoMenu->addAction(action->icon(), action->text());
        menuCommand->setShortcut(action->shortcut()); action->setShortcut({});
        connect(menuCommand, &QAction::triggered, action, &QAction::trigger); menuCommands.append(menuCommand);
    }
    repoActions.append(menuCommands);
    repoMenu->addSeparator();
    auto add = [&](const QString &name, std::function<void()> callback, QKeySequence shortcut = {}) {
        auto *a = repoMenu->addAction(name, shortcut, this, std::move(callback)); repoActions.append(a);
    };
    add("Fusionner une branche…",[this]{mergeBranches();});
    add("Nouvelle branche…", [this] { newBranch(); }, QKeySequence("Ctrl+Shift+N"));
    add("Publier ce dépôt sur GitHub…",[this]{publishRepository();});
    add("Réglages du dépôt…",[this]{repositorySettings();});
    add("Releases du dépôt…",[this]{showReleases();});
    add("Autoriser les workflows GitHub…",[this]{authorizeWorkflowPermission(repository);});
    add("Générer un message avec Codex",[this]{generateMessage();});
    add("Publier la branche sur origin", [this] { mutate({"push", "--set-upstream", "origin", "HEAD"}, "Publication de la branche"); });
    add("Configurer origin…", [this] {
        bool ok = false; auto url = glassInputText(this, "Dépôt distant origin", "URL HTTPS / SSH", QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || url.isEmpty() || url.startsWith('-') || runner.busy()) return;
        runGit({"remote", "get-url", "origin"}, [this, url](const CommandResult &r) {
            runGit(r.ok() ? QStringList{"remote", "set-url", "origin", url} : QStringList{"remote", "add", "origin", url}, [this](const CommandResult &result) {
                if (!result.ok()) Diagnostics::showError(this, "Origin", result.message());
                else log("Dépôt distant origin configuré.");
            });
        });
    });
    add("Créer le commit", [this] { commit(); }, QKeySequence("Ctrl+Return"));
    add("Mettre de côté (stash)", [this] { mutate({"stash", "push", "--include-untracked", "-m", "GitHub Enhanced"}, "Stash"); });
    add("Réappliquer le dernier stash", [this] { mutate({"stash", "apply"}, "Réapplication du stash (conservé)"); });
    add("Ouvrir dans le gestionnaire de fichiers", [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(repository)); });
    auto *view = menuBar()->addMenu("&Affichage");
    for (auto *d : layoutPanels()) view->addAction(d->toggleViewAction());
    view->addAction(toolbar->toggleViewAction());
    toolbar->toggleViewAction()->setShortcut(QKeySequence("Ctrl+Shift+T"));
    view->addSeparator();
    auto *advanced = view->addAction("Mode avancé · détails techniques");
    advanced->setObjectName("advancedModeAction"); advanced->setCheckable(true); advanced->setChecked(Diagnostics::advanced());
    connect(advanced, &QAction::toggled, &Diagnostics::instance(), &Diagnostics::setAdvanced);
    connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, this, [this,advanced](bool enabled){QSignalBlocker blocker(advanced);advanced->setChecked(enabled);if(!repository.isEmpty())updateSyncStatus(syncState);});
    view->addAction("Personnaliser l’interface…", QKeySequence("Ctrl+Alt+L"), this, &MainWindow::editLayout);
    lockAction = view->addAction("Verrouiller la disposition"); lockAction->setCheckable(true); lockAction->setChecked(true);
    connect(lockAction, &QAction::toggled, this, [this](bool locked) { setLayoutEditing(!locked); });
    view->addAction("Enregistrer la disposition…", this, [this] {
        bool ok = false; auto name = glassInputText(this, "Disposition", "Nom", QLineEdit::Normal, {}, &ok).trimmed();
        if (ok && !name.isEmpty() && name.size() <= 64 && !name.contains('/') && !name.contains('\\')) { settings.setValue("layouts/" + name, QJsonDocument(layoutConfiguration()).toJson(QJsonDocument::Compact)); rebuildLayouts(); }
    });
    layoutsMenu = view->addMenu("Dispositions enregistrées"); rebuildLayouts();
    view->addAction("Rétablir la disposition initiale", this, [this] { resetLayout(); });
    view->addAction("Plein écran", QKeySequence::FullScreen, this, [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    auto *help = menuBar()->addMenu("&Aide");
    help->addAction("Rechercher des mises à jour…",this,[this]{checkUpdates(true);});
    help->addAction("À propos de GitHub Enhanced", this, [this] {
        QMessageBox::about(this, "GitHub Enhanced", "GitHub Enhanced "+QCoreApplication::applicationVersion()+" — aperçu\nClient Git indépendant, écrit en C++ et Qt 6.\nNon affilié à GitHub.\n\nGit doit être installé et configuré. GitHub CLI est facultatif pour la synchronisation.\nLa parité complète avec GitHub Desktop et la marketplace publique restent en développement.");
    })->setMenuRole(QAction::AboutRole);
    const QList<QPair<QString, QString>> optionalCommands{
        {"tool-new-branch", "Nouvelle branche…"}, {"tool-publish", "Publier la branche sur origin"},
        {"tool-origin", "Configurer origin…"}, {"tool-releases", "Releases du dépôt…"}, {"tool-commit", "Créer le commit"},
        {"tool-stash", "Mettre de côté (stash)"}, {"tool-apply-stash", "Réappliquer le dernier stash"},
        {"tool-reveal", "Ouvrir dans le gestionnaire de fichiers"}, {"tool-preferences", "Préférences…"},
        {"tool-clone", "Cloner un dépôt…"}, {"tool-init", "Créer un dépôt…"}};
    for (const auto &entry : optionalCommands) {
        QAction *source = nullptr;
        for (auto *action : findChildren<QAction *>()) if (action->text() == entry.second && !toolbarItems.contains(action)) { source = action; break; }
        if (!source) continue;
        auto *action = new QAction(source->icon(), source->text(), this);
        action->setIcon(commandIcon(entry.first.mid(5)));
        action->setObjectName(entry.first); action->setProperty("customTitle", entry.second); action->setProperty("defaultVisible", false);
        connect(action, &QAction::triggered, source, &QAction::trigger);
        mainToolbar->addAction(action); action->setVisible(false); toolbarItems.append(action);
        if (repoActions.contains(source)) repoActions.append(action);
    }
}

void MainWindow::rebuildRecent() {
    repositories->clear(); repositoryMenu->clear();
    repositoryMenu->addAction(openAction);
    repositoryMenu->addAction("Cloner un dépôt…", this, &MainWindow::cloneRepository);
    repositoryMenu->addAction("Créer un dépôt…", this, &MainWindow::initRepository);
    repositoryMenu->addAction("Mes dépôts GitHub…",this,&MainWindow::browseGitHub);
    if (!recent.isEmpty()) repositoryMenu->addSeparator();
    for (const auto &path : recent) {
        auto *menuRow=new QWidget;auto *rowLayout=new QHBoxLayout(menuRow);rowLayout->setContentsMargins(6,0,6,0);
        auto *open=new QPushButton(QFileInfo(path).fileName());open->setFlat(true);open->setProperty("recentEntry",true);open->setToolTip(path);rowLayout->addWidget(open,1);
        auto *remove=new QToolButton;remove->setText("×");remove->setObjectName("removeRecentMenu");remove->setProperty("recentEntry",true);remove->setProperty("recentRemove",true);remove->setFixedSize(28,28);remove->setAccessibleName("Retirer des dépôts récents");remove->setToolTip("Retirer de l’accès rapide · les fichiers sont conservés");rowLayout->addWidget(remove);
        auto *rowAction=new QWidgetAction(repositoryMenu);rowAction->setDefaultWidget(menuRow);repositoryMenu->addAction(rowAction);
        connect(open,&QPushButton::clicked,this,[this,path]{repositoryMenu->hide();openRepository(path);});
        connect(remove,&QToolButton::clicked,this,[this,path]{repositoryMenu->hide();QTimer::singleShot(0,this,[this,path]{removeRecent(path);});});
        auto *item = new QListWidgetItem(repositories);item->setSizeHint({180,44});
        item->setData(Qt::UserRole, path); item->setToolTip(path);
        auto *row=new QWidget;auto *rowBox=new QHBoxLayout(row);rowBox->setContentsMargins(8,0,4,0);auto *name=new QLabel(QFileInfo(path).fileName());name->setAttribute(Qt::WA_TransparentForMouseEvents);rowBox->addWidget(name,1);
        auto *cross=new QToolButton;cross->setText("×");cross->setObjectName("removeRecent");cross->setProperty("recentEntry",true);cross->setProperty("recentRemove",true);cross->setFixedSize(28,28);cross->setProperty("repositoryPath",path);cross->setAccessibleName("Retirer "+QFileInfo(path).fileName()+" des dépôts récents");cross->setToolTip("Retirer de l’accès rapide");rowBox->addWidget(cross);repositories->setItemWidget(item,row);
        connect(cross,&QToolButton::clicked,this,[this,path]{QTimer::singleShot(0,this,[this,path]{removeRecent(path);});});
        if (path == repository) repositories->setCurrentItem(item);
    }
}

void MainWindow::openRepository(const QString &path) {
    if (runner.busy() || indexer.busy() || pushCheck.busy() || path.isEmpty()) return;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    runner.run("git", {"rev-parse", "--show-toplevel"}, absolute, [this](const CommandResult &r) {
        if (!r.ok()) { Diagnostics::showError(this, "Dépôt inaccessible", r.message()); return; }
        auto root = r.output;
        if (root.endsWith('\n')) root.chop(1);
        repository = QString::fromUtf8(root);repositorySnapshot.clear();historySnapshot.clear();syncState={};syncStatus->setText("Vérification de la branche et du dépôt distant…");syncAction->hide();syncMonitor.inspect(repository,true); ++diffRevision;selectedCommit.clear();commitBrowser->hide();indexError.clear();
        branch.clear();changes.clear();localStatusKnown=false; files->clear(); history->clear(); diff->clear();
        summary->clear(); description->clear();
        recent.removeAll(repository); recent.prepend(repository); while (recent.size() > 20) recent.removeLast();
        settings.setValue("recent", recent); settings.setValue("lastRepository", repository); rebuildRecent();
        repositorySelector->setText(QFileInfo(repository).fileName()); repositorySelector->setToolTip(repository);
        setWindowTitle(QFileInfo(repository).fileName() + " — GitHub Enhanced"); pages->setCurrentIndex(1);
        refresh();
    });
}

void MainWindow::runGit(const QStringList &args, CommandRunner::Callback callback) {
    runner.run("git", args, repository, std::move(callback));
}

void MainWindow::removeRecent(const QString &path){recent.removeAll(path);settings.setValue("recent",recent);rebuildRecent();}
void MainWindow::beginOperation(const QString &title){indexError.clear();operation->begin(title);}

void MainWindow::refresh() { refreshRepository(false); }

void MainWindow::refreshRepository(bool quiet) {
    if (repository.isEmpty() || refreshing || indexer.busy() || pushCheck.busy()) return;
    quietRefresh=quiet;refreshing=true;
    indexer.prepare(repository,[this](QString error){
        const bool newError=error!=indexError;indexError=error;
        if(!error.isEmpty()){
            statusBar()->showMessage(Diagnostics::explain(error).title);if(newError)log(error);
            if(!quietRefresh&&(newError||operation->running())){if(!operation->running())operation->begin("Préparation du commit");operation->finish(false,error);}
        }
        refreshViews();
    });
}
void MainWindow::refreshViews() {
    // Queued reads and writes share the same runner, preventing index races.
    const auto path = repository;
    runGit({"status", "--porcelain=v1", "-z", "--untracked-files=all"}, [this, path](const CommandResult &r) {
        if (path != repository) return;
        if (!r.ok()) { localStatusKnown=false;updateSyncStatus(syncState);log(r.message());changeCount->setText("Lecture locale impossible");diffTitle->setText("État local inconnu · voir Activité");statusBar()->showMessage("Lecture du dépôt impossible");return; }
        QString selected;
        if (auto *item = files->currentItem()) selected = item->data(1, Qt::UserRole).toString();
        localStatusKnown=true;
        const auto nextChanges=parseStatus(r.output);
        if(nextChanges!=changes){
            const auto scroll=files->verticalScrollBar()->value();const QSignalBlocker block(files);files->clear();changes=nextChanges;
            for(const auto &c:changes){auto *item=new QTreeWidgetItem(files,{c.status,c.path});item->setData(1,Qt::UserRole,c.path);item->setToolTip(0,"M : modifié · A : ajouté · D : supprimé · R : renommé · U : conflit");if(c.path==selected)files->setCurrentItem(item);}
            if(!files->currentItem()&&files->topLevelItemCount())files->setCurrentItem(files->topLevelItem(0));
            files->verticalScrollBar()->setValue(scroll);
        }
        changeCount->setText(indexError.isEmpty()?QString("%1 fichier(s) prêt(s) pour le commit").arg(changes.size()):"Préparation interrompue · voir le journal");
        updateSyncStatus(syncState);
        if(selectedCommit.isEmpty())showDiff();
    });
    runGit({"symbolic-ref", "--quiet", "--short", "HEAD"}, [this, path](const CommandResult &r) {
        if (path != repository) return;
        branch = r.ok() ? QString::fromUtf8(r.output).trimmed() : QString();
    });
    runGit({"for-each-ref", "--format=%(refname:short)", "refs/heads/"}, [this, path](const CommandResult &r) {
        if (path != repository) return;
        QStringList choices;if(!branch.isEmpty())choices<<branch;for(const auto &name:QString::fromUtf8(r.output).split('\n',Qt::SkipEmptyParts))if(name!=branch)choices<<name;
        QStringList previous;for(int i=0;i<branches->count();++i)previous<<branches->itemData(i).toString();if(previous==choices)return;
        const QSignalBlocker block(branches);branches->clear();if(branch.isEmpty())branches->addItem("HEAD détachée",QString());for(const auto &name:choices)branches->addItem(name,name);branches->setCurrentIndex(0);
    });
    runGit({"log", "-n", "150", "--format=%H%x00%h%x00%an%x00%ar%x00%s%x00"}, [this, path](const CommandResult &r) {
        if (path != repository) return;
        refreshing=false;quietRefresh=false;syncMonitor.inspect(path,false);
        if(r.output==historySnapshot)return;
        historySnapshot=r.output;const auto oldScroll=history->verticalScrollBar()->value();const auto oldSelection=history->currentItem()?history->currentItem()->data(0,Qt::UserRole).toString():QString();
        const QSignalBlocker block(history);history->clear();
        if (!r.ok()) return; // An unborn repository has no history yet.
        const auto parts = r.output.split('\0');
        for (qsizetype i = 0; i + 4 < parts.size(); i += 5) {
            auto *item = new QTreeWidgetItem(history, {QString::fromUtf8(parts[i+4]) + "\n" + QString::fromUtf8(parts[i+1]) + " · " + QString::fromUtf8(parts[i+2]) + " · " + QString::fromUtf8(parts[i+3])});
            item->setData(0, Qt::UserRole, QString::fromUtf8(parts[i]).trimmed());
            if(item->data(0,Qt::UserRole).toString()==oldSelection)history->setCurrentItem(item);
        }
        history->verticalScrollBar()->setValue(oldScroll);
    });
}

void MainWindow::updateActions() {
    bool enabled = !property("installingUpdate").toBool() && !repository.isEmpty() && !runner.busy() && !indexer.busy() && !github.busy() && !pushCheck.busy();
    for (auto *a : repoActions) a->setEnabled(enabled&&(!QStringList{"Push","Pull","Fetch"}.contains(a->text())||!syncMonitor.busy()));
    const bool canEdit=!property("installingUpdate").toBool()&&!repository.isEmpty();
    branches->setEnabled(enabled);commitDock->widget()->setEnabled(canEdit);changesDock->widget()->setEnabled(canEdit);
    commitButton->setEnabled(enabled);generateButton->setEnabled(enabled||codex.generating());
    repositories->setEnabled(!runner.busy());
    syncAction->setEnabled(enabled&&!syncMonitor.busy());
    if(syncToolbarAction)syncToolbarAction->setEnabled(enabled&&!syncMonitor.busy()&&!QStringList{"unborn","detached"}.contains(syncState.kind));
}

void MainWindow::updateSyncStatus(const RepositorySyncState &state){
    if(state.repository!=repository)return;
    if(!state.error.isEmpty()&&state.error!=syncState.error)log("Vérification du distant :\n"+state.error);
    syncState=state;
    for(auto *panel:customPanels)panel->updateRepository(state);
    auto commits=[](int count){return QString::number(count)+(count==1?" commit":" commits");};
    QString message="Branche "+state.branch;QString action;
    if(!state.error.isEmpty()){message+=" · État distant non vérifié\n"+Diagnostics::explain(state.error).text();action="Réessayer";}
    else if(state.kind=="ahead"){message+=QString(" · %1 à envoyer vers %2").arg(commits(state.ahead),state.upstream);action="Envoyer "+commits(state.ahead);}
    else if(state.kind=="behind"){message+=QString(" · %1 à récupérer depuis %2").arg(commits(state.behind),state.upstream);action="Récupérer "+commits(state.behind);}
    else if(state.kind=="diverged"){message+=QString(" · %1 à envoyer, %2 à récupérer · branches divergentes").arg(commits(state.ahead),commits(state.behind));action="Fusionner les branches…";}
    else if(state.kind=="synced"){
        if(!localStatusKnown)message+=" · Commits alignés · état des fichiers locaux non vérifié";
        else if(!indexError.isEmpty())message+=" · Commits alignés · préparation locale interrompue";
        else if(!changes.isEmpty())message+=" · Commits alignés avec "+state.upstream+" · modifications locales à commiter";
        else message+=state.checkedAt.isValid()?" · Synchronisée avec "+state.upstream:" · Aucun écart avec la référence locale "+state.upstream;
    }
    else if(state.kind=="noUpstream"){message+=" · Aucun suivi distant configuré · la branche n’est pas synchronisée";action="Publier la branche";}
    else if(state.kind=="localOnly"){message+=" · Dépôt local sans suivi distant";action="Publier sur GitHub…";}
    else if(state.kind=="unborn")message+=" · Aucun commit local · créez un premier commit";
    else if(state.kind=="detached")message="HEAD détachée · sélectionnez une branche pour la synchroniser";
    else message+=" · État distant inconnu";
    if(!state.remote.isEmpty()&&state.error.isEmpty())message+=state.checkedAt.isValid()?"\nDistant vérifié à "+state.checkedAt.toString("HH:mm:ss"):"\nDistant non vérifié · comparaison avec les références en cache";
    if(syncToolbarAction&&!syncMonitor.busy()){
        auto remote=state.remote.isEmpty()?"origin":state.remote;QString title="Fetch "+remote,icon="fetch";
        if(state.kind=="ahead"){title="Push "+remote+" · "+QString::number(state.ahead);icon="push";}
        else if(state.kind=="behind"){title="Pull "+remote+" · "+QString::number(state.behind);icon="pull";}
        else if(state.kind=="diverged")title="Fusionner · "+remote;
        else if(state.kind=="localOnly")title="Publier sur GitHub…";
        else if(state.kind=="noUpstream")title="Publier la branche";
        if(!state.error.isEmpty())title="Réessayer Fetch "+remote;
        syncToolbarAction->setText(title);syncToolbarAction->setIcon(commandIcon(icon));syncToolbarAction->setToolTip(message);
    }
    syncStatus->setText(message);syncStatus->setToolTip("Dossier : "+repository+"\nBranche locale : "+state.branch+"\nSuivi : "+state.upstream+(Diagnostics::advanced()&&!state.error.isEmpty()?"\n\nDétails techniques :\n"+state.error:QString()));syncAction->setText(action);syncAction->setVisible(!action.isEmpty());updateActions();
}
void MainWindow::performSyncAction(){
    if(repository.isEmpty()||runner.busy()||indexer.busy()||syncMonitor.busy())return;
    if(!syncState.error.isEmpty()){syncMonitor.inspect(repository,true);return;}
    const auto expected=syncState;
    runGit({"symbolic-ref","--quiet","--short","HEAD"},[this,expected](const CommandResult &branchResult){
        if(repository!=expected.repository)return;
        if(!branchResult.ok()||QString::fromUtf8(branchResult.output).trimmed()!=expected.branch){syncStatus->setText("La branche a changé · vérification en cours…");syncMonitor.inspect(repository,true);return;}
        runGit({"rev-parse","--verify","HEAD"},[this,expected](const CommandResult &headResult){
            if(repository!=expected.repository)return;
            if(!headResult.ok()||QString::fromUtf8(headResult.output).trimmed()!=expected.head){syncStatus->setText("Les commits ont changé · vérification en cours…");syncMonitor.inspect(repository,true);return;}
            QTimer::singleShot(0,this,[this,expected]{
                if(repository!=expected.repository||runner.busy()||indexer.busy())return;
                if(expected.kind=="ahead")mutate({"push","--",expected.remote,expected.head+":"+expected.remoteRef},"Envoi des commits");
                else if(expected.kind=="behind")mutate({"pull","--ff-only","--",expected.remote,expected.remoteRef},"Récupération des commits");
                else if(expected.kind=="noUpstream")mutate({"push","--set-upstream","--",expected.remote,expected.branch},"Publication de la branche");
                else if(expected.kind=="localOnly")publishRepository();
                else if(expected.kind=="diverged")mergeBranches();
                else syncMonitor.inspect(repository,true);
            });
        });
    });
}
void MainWindow::showSyncDetails(){
    GlassDialog dialog(this);dialog.setWindowTitle("Écarts entre les branches");dialog.resize(760,520);auto *root=new QVBoxLayout(&dialog);root->setContentsMargins(24,24,24,24);auto *title=new QLabel("Branches divergentes");title->setObjectName("pageTitle");root->addWidget(title);
    auto *hint=new QLabel("Les deux branches contiennent des commits différents. Vous pouvez les fusionner depuis Dépôt → Fusionner une branche… Les commits des deux branches seront conservés.");hint->setWordWrap(true);root->addWidget(hint);
    auto *text=new QPlainTextEdit;text->setReadOnly(true);text->setPlainText("Lecture des écarts…");root->addWidget(text,1);auto *commands=new CommandRunner(&dialog);commands->run("git",{"log","--left-right","--graph","--oneline","-n","100","HEAD..."+syncState.upstream,"--"},repository,[text](const CommandResult &r){text->setPlainText(r.ok()?"< commit local · > commit distant\n\n"+QString::fromUtf8(r.output):r.message());});auto *close=new QPushButton("Fermer");root->addWidget(close,0,Qt::AlignRight);connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);dialog.exec();
}

void MainWindow::mutate(const QStringList &args,const QString &title){
    if(repository.isEmpty()||runner.busy()||indexer.busy()||pushCheck.busy())return;
    if(args.value(0)=="push")checkedPush(args,title);else runMutation(args,title);
}
void MainWindow::checkedPush(const QStringList &args,const QString &title){
    const auto path=repository;beginOperation("Vérification avant envoi");
    pushCheck.inspect(path,[this,path,args,title](PushInspection result,QString error){
        if(repository!=path)return;
        if(!error.isEmpty()){indexError=error;operation->finish(false,error);return;}
        // A frozen SHA from the sync banner must still be the reviewed local revision.
        for(const auto &arg:args){auto source=arg.section(':',0,0);if(QRegularExpression("^[a-fA-F0-9]{40,64}$").match(source).hasMatch()&&source!=result.head){operation->finish(false,"Les commits ont changé. Vérifiez la branche puis relancez l’envoi.");syncMonitor.inspect(path,true);return;}}
        if(result.largeObjects.isEmpty()){QTimer::singleShot(0,this,[this,path,args,title]{if(repository==path&&!runner.busy()&&!indexer.busy())runMutation(args,title);});return;}
        operation->finish(true,"Gros fichiers détectés dans les commits à envoyer.");operation->hide();
        GlassDialog review(this);review.setObjectName("lfsMigrationReview");review.setWindowTitle("Gros fichiers déjà commités");review.resize(680,500);auto *root=new QVBoxLayout(&review);root->setContentsMargins(24,24,24,24);root->setSpacing(14);auto *heading=new QLabel("Convertir les commits en Git LFS");heading->setObjectName("pageTitle");heading->setWordWrap(true);root->addWidget(heading);
        auto *hint=new QLabel("Ces fichiers sont déjà stockés dans des commits non publiés. Ajouter une règle LFS ou créer un nouveau commit ne retire pas les gros objets des anciens commits à envoyer.");hint->setWordWrap(true);root->addWidget(hint);
        auto *list=new QListWidget;list->setObjectName("lfsCommittedFiles");for(const auto &file:result.largeObjects){auto *item=new QListWidgetItem(file.path+" · "+QString::number(file.bytes/1048576.0,'f',1)+" Mio",list);item->setToolTip(file.path);}root->addWidget(list,1);
        auto *scope=new QLabel("La conversion modifie les identifiants des commits non publiés de cette branche. Les commits connus des dépôts distants sont exclus. Une référence de sauvegarde locale sera créée. Les fichiers restent sur le disque. L’envoi reprendra après vérification.");scope->setWordWrap(true);scope->setObjectName("muted");root->addWidget(scope);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText("Convertir en LFS et envoyer");buttons.button(QDialogButtonBox::Ok)->setObjectName("convertCommittedLfs");buttons.button(QDialogButtonBox::Cancel)->setText("Annuler l’envoi");root->addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&review,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
        if(review.exec()!=QDialog::Accepted){operation->finish(true);statusBar()->showMessage("Envoi annulé. Les commits n’ont pas été modifiés.");return;}
        beginOperation("Conversion des commits non publiés en LFS");
        pushCheck.convert(result,[this,path,args,title,old=result.head](QString newHead,QString backupRef,QString conversionError){
            if(repository!=path)return;
            if(!backupRef.isEmpty())log("Sauvegarde avant conversion LFS : "+backupRef);
            if(!conversionError.isEmpty()){indexError=conversionError;operation->finish(false,conversionError+(backupRef.isEmpty()?QString():"\nSauvegarde conservée : "+backupRef));refresh();syncMonitor.inspect(path,true);return;}
            if(newHead.isEmpty()||newHead==old){operation->finish(false,"Aucun nouveau commit LFS n’a été créé. L’envoi n’a pas été lancé.");return;}
            auto newArgs=args;for(auto &arg:newArgs)if(arg==old||arg.startsWith(old+':'))arg.replace(0,old.size(),newHead);
            // Re-scan converted commits; never send the SHA retained by the backup.
            QTimer::singleShot(0,this,[this,path,newArgs,title]{if(repository==path)checkedPush(newArgs,title);});
        });
    });
}

void MainWindow::runMutation(const QStringList &args, const QString &title) {
    if (repository.isEmpty() || runner.busy()) return;
    beginOperation(title);log(title + "…");
    auto progressArgs=args;if(!args.isEmpty()&&QStringList{"push","pull","fetch"}.contains(args.first()))progressArgs.insert(1,"--progress");
    const auto path=repository;
    runGit(progressArgs, [this, path, args, title](const CommandResult &r) {
        log(title + (r.ok() ? " : terminé" : " : échec") + '\n' + r.message());
        if (!r.ok()) {
            QStringList important;
            for(const auto &line:r.message().split(QRegularExpression("[\\r\\n]+")))if(QRegularExpression("error:|erreur\\s*:|fatal:|remote rejected|GH001",QRegularExpression::CaseInsensitiveOption).match(line).hasMatch())important.append(line);
            indexError=important.isEmpty()?r.message().right(2500):important.join('\n').right(3500);
            if(args.value(0)=="push"&&GitHubClient::workflowPermissionMissing(r.message())){
                indexError="GitHub a refusé les fichiers .github/workflows/ : la connexion OAuth ne possède pas l’autorisation workflow. Autorisez-la pour pouvoir envoyer ces commits.";
                operation->finish(false,indexError);operation->setRecoveryAction("Autoriser workflow et réessayer",[this,path,args,title]{authorizeWorkflowPermission(path,args,title);});
            }else operation->finish(false,indexError);
        }
        refresh();syncMonitor.inspect(repository,true);
    });
}

void MainWindow::authorizeWorkflowPermission(const QString &path,const QStringList &retryArguments,const QString &retryTitle){
    if(path.isEmpty()||repository!=path||pushCheck.busy())return;
    const auto expectedBranch=syncState.branch,expectedHead=syncState.head;
    operation->hide();AccountDialog auth("gh",&github,&codex,this,true);
    QTimer::singleShot(0,&auth,[&auth]{if(auto *button=auth.findChild<QPushButton *>("primary"))button->click();});
    auth.exec();
    if(!auth.succeeded()){
        operation->finish(false,"L’autorisation workflow n’a pas été ajoutée. L’envoi reste en attente.");operation->setRecoveryAction("Autoriser workflow et réessayer",[this,path,retryArguments,retryTitle]{authorizeWorkflowPermission(path,retryArguments,retryTitle);});return;
    }
    if(repository!=path)return;
    if(retryArguments.isEmpty()){statusBar()->showMessage("Autorisation workflow ajoutée. Vous pouvez envoyer vos commits.");return;}
    // Consent can take minutes. Recheck the branch and revision before retrying.
    runGit({"symbolic-ref","--quiet","--short","HEAD"},[this,path,expectedBranch,expectedHead,retryArguments,retryTitle](const CommandResult &branch){
        if(repository!=path)return;
        if(!branch.ok()||QString::fromUtf8(branch.output).trimmed()!=expectedBranch){syncMonitor.inspect(path,true);statusBar()->showMessage("La branche a changé. Vérifiez-la avant de relancer l’envoi.");return;}
        runGit({"rev-parse","--verify","HEAD"},[this,path,expectedHead,retryArguments,retryTitle](const CommandResult &head){
            if(repository!=path)return;
            if(!head.ok()||QString::fromUtf8(head.output).trimmed()!=expectedHead){syncMonitor.inspect(path,true);statusBar()->showMessage("Les commits ont changé. Relancez l’envoi depuis le bandeau.");return;}
            QTimer::singleShot(0,this,[this,path,retryArguments,retryTitle]{if(repository==path)mutate(retryArguments,retryTitle);});
        });
    });
}

void MainWindow::commit() {
    if (repository.isEmpty() || runner.busy()) return;
    const auto originalSummary=summary->text();auto message = originalSummary.trimmed();
    if (message.isEmpty()) { summary->setFocus(); statusBar()->showMessage("Saisissez un résumé du commit.", 5000); return; }
    const auto body=description->toPlainText();beginOperation("Création du commit");
    indexer.prepare(repository,[this,message,body,originalSummary](QString error){
        if(!error.isEmpty()){indexError=error;operation->finish(false,error);updateActions();return;}
        QStringList args{"commit","-m",message};if(!body.trimmed().isEmpty())args<<"-m"<<body;
        runGit(args,[this,message,body,originalSummary](const CommandResult &r){log(r.message());if(r.ok()){if(summary->text()==originalSummary)summary->clear();if(description->toPlainText()==body)description->clear();}else {indexError=r.message();operation->finish(false,indexError);}refresh();});
    });
}

void MainWindow::showDiff() {
    if(!selectedCommit.isEmpty())return;
    auto *item = files->currentItem(); const int revision = ++diffRevision;
    if (!item) { diffTitle->setText(changes.isEmpty() ? "Aucune modification locale non commitée" : "Sélectionnez un fichier pour examiner ses modifications"); diff->clear(); return; }
    const QString file = item->data(1, Qt::UserRole).toString(); const auto path = repository;
    FileChange change;
    for (const auto &c : changes) if (c.path == file) change = c;
    if(diffTitle->text()!=file)diff->clear();
    diffTitle->setText(file);
    if (change.untracked()) {
        QFileInfo info(QDir(repository).filePath(file));
        if (info.isSymLink()) { showText("Lien symbolique → " + info.symLinkTarget()); return; }
        QFile source(info.absoluteFilePath());
        if (!source.open(QIODevice::ReadOnly)) { showText("Lecture impossible : " + source.errorString()); return; }
        auto bytes = source.read(1024 * 1024);
        showText(bytes.contains('\0') ? "Fichier binaire — aperçu indisponible." : QString::fromUtf8(bytes) + (source.atEnd() ? "" : "\n[… aperçu limité à 1 Mio]"));
        return;
    }
    QStringList args{"--no-pager", "diff", "--no-ext-diff", "--no-textconv", "--no-color"};
    args << "--cached";
    args << "--" << file; if (!change.originalPath.isEmpty()) args << change.originalPath;
    diffRunner.run("git", args, path, [this, revision, path](const CommandResult &r) {
        if (path != repository || revision != diffRevision) return;
        showText(r.ok() ? (r.output.isEmpty() ? "Aucune modification à afficher." : QString::fromUtf8(r.output)+(r.truncated?"\n[… aperçu limité à 1 Mio]":"")) : r.message());
    },{},1024*1024,true);
}

void MainWindow::showCommit(const QString &sha) {
    if(!QRegularExpression("^[a-fA-F0-9]{40,64}$").match(sha).hasMatch())return;
    selectedCommit=sha;commitBrowser->show();commitFiles->clear();commitSubject->setText("Chargement du commit…");commitInfo->clear();commitMeta->clear();diffTitle->setText("Commit "+sha.left(12));diff->clear();diffDock->show();diffDock->raise();pages->setCurrentIndex(1);
    const auto path=repository;const int revision=++diffRevision;
    diffRunner.run("git",{"show","--no-patch","--format=%s%x00%an%x00%aI%x00%b",sha,"--"},path,[this,path,sha](const CommandResult &r){
        if(path!=repository||sha!=selectedCommit)return;
        if(!r.ok()){commitInfo->setText(r.message());return;}
        const auto parts=r.output.split('\0');if(parts.size()<4)return;
        commitSubject->setText(QString::fromUtf8(parts[0]));
        const auto date=QDateTime::fromString(QString::fromUtf8(parts[2]),Qt::ISODate).toLocalTime();
        commitMeta->setText(QString::fromUtf8(parts[1])+"\n"+QLocale(QLocale::French).toString(date,"d MMM yyyy · HH:mm")+"\n"+sha.left(12));
        commitInfo->setText(QString::fromUtf8(parts[3]).trimmed());
    });
    diffRunner.run("git",{"diff-tree","--root","--no-commit-id","--name-status","-z","-r","--no-renames","--first-parent","-m",sha,"--"},path,[this,path,sha,revision](const CommandResult &r){
        if(path!=repository||sha!=selectedCommit||revision!=diffRevision)return;
        if(!r.ok()){showText(r.message());return;}
        const auto records=r.output.split('\0');
        {QSignalBlocker block(commitFiles);for(qsizetype i=0;i+1<records.size();i+=2){if(records[i].isEmpty())continue;const auto file=QString::fromUtf8(records[i+1]);auto *item=new QTreeWidgetItem(commitFiles,{QString::fromLatin1(records[i])+"  "+file});item->setData(0,Qt::UserRole,file);item->setData(0,Qt::UserRole+1,QString::fromLatin1(records[i]));item->setToolTip(0,file);}}
        if(commitFiles->topLevelItemCount())commitFiles->setCurrentItem(commitFiles->topLevelItem(0));else showText("Ce commit ne contient pas de changement de fichier par rapport à ses parents.");
    });
}
void MainWindow::showCommitFile(){
    auto *item=commitFiles->currentItem();if(!item||selectedCommit.isEmpty())return;
    const auto sha=selectedCommit,path=repository,file=item->data(0,Qt::UserRole).toString(),status=item->data(0,Qt::UserRole+1).toString();const int revision=++diffRevision;diffTitle->setText(file+" · "+sha.left(10)+(status=="D"?" · supprimé":""));
    QStringList args;
    const bool patch=commitView->currentIndex()==1;
    if(patch)args={"--no-pager","show","--format=","--first-parent","--no-ext-diff","--no-textconv","--no-color",sha,"--",file};
    else args={"show",(status=="D"?sha+"^":sha)+":"+file};
    diffRunner.run("git",args,path,[this,path,sha,revision,patch](const CommandResult &r){
        if(path!=repository||sha!=selectedCommit||revision!=diffRevision)return;
        if(!r.ok()){showText(r.message());return;}
        if(r.output.contains('\0')){showText("Fichier binaire · aperçu textuel indisponible.");return;}
        auto text=QString::fromUtf8(r.output);if(text.startsWith("version https://git-lfs.github.com/spec/v1"))text="Fichier stocké avec Git LFS\n\n"+text;
        showText(text+(r.truncated?"\n[… aperçu limité à 1 Mio]":""));if(!patch)diff->setExtraSelections({});
    },{},1024*1024,true);
}

void MainWindow::showText(const QString &text) {
    // Limit UI rendering separately from process output so very large diffs remain responsive.
    const auto limited=text.left(1024*1024);const auto rendered=limited+(text.size()>limited.size()?"\n[… aperçu limité à 1 million de caractères]":"");
    const bool same=diff->toPlainText()==rendered;
    if(same&&!diff->property("refreshHighlight").toBool())return;
    diff->setProperty("refreshHighlight",false);
    if(!same){const int vertical=diff->verticalScrollBar()->value(),horizontal=diff->horizontalScrollBar()->value();diff->setPlainText(rendered);diff->verticalScrollBar()->setValue(vertical);diff->horizontalScrollBar()->setValue(horizontal);}
    QList<QTextEdit::ExtraSelection> selections;
    auto block = diff->document()->firstBlock();
    int count = 0;
    while (block.isValid() && count++ < 12000) {
        auto line = block.text();
        if (line.startsWith('+') || line.startsWith('-') || line.startsWith("@@")) {
            QTextEdit::ExtraSelection selection; selection.cursor = QTextCursor(block);
            selection.cursor.select(QTextCursor::LineUnderCursor);
            QColor color(activeTheme[line.startsWith('+') ? "added" : line.startsWith('-') ? "removed" : "accent"].toString());
            if (native) {
                const bool dark = palette().color(QPalette::Base).lightness() < 128;
                color = line.startsWith('+') ? QColor(dark ? "#66c58a" : "#1a7f37") : line.startsWith('-') ? QColor(dark ? "#f08080" : "#cf222e") : palette().color(QPalette::Highlight);
            }
            selection.format.setForeground(color);
            selections.append(selection);
        }
        block = block.next();
    }
    diff->setExtraSelections(selections);
}

void MainWindow::cloneRepository() {
    if (runner.busy()) return;
    bool ok = false; auto url = glassInputText(this, "Cloner un dépôt", "URL HTTPS / SSH ou chemin du dépôt", QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || url.isEmpty() || url.startsWith('-')) return;
    cloneUrl(url);
}

void MainWindow::cloneUrl(const QString &url, const QString &suggestedName) {
    if (runner.busy() || url.isEmpty() || url.startsWith('-')) return;
    bool ok = false;
    const auto parent = QFileDialog::getExistingDirectory(this, "Dossier parent du clone"); if (parent.isEmpty()) return;
    auto name = glassInputText(this, "Nom du dossier", "Nouveau dossier", QLineEdit::Normal, suggestedName.isEmpty() ? "mon-depot" : suggestedName, &ok).trimmed();
    if (!ok || name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\')) return;
    auto destination = QDir(parent).absoluteFilePath(name);
    if (QFileInfo::exists(destination)) { QMessageBox::warning(this, "Cloner", "Le dossier existe déjà. Choisissez un nouveau nom."); return; }
    beginOperation("Clonage du dépôt");log("Clonage en cours…");
    runner.run("git", {"clone", "--progress", "--", url, destination}, parent, [this, destination](const CommandResult &r) {
        log(r.message());
        if (r.ok()) QTimer::singleShot(0, this, [this, destination] { openRepository(destination); });
        else {indexError=r.message().left(4000);operation->finish(false,indexError);}
    });
}

void MainWindow::browseGitHub() {
    RepoBrowser dialog(&github,this);connect(&dialog,&RepoBrowser::releasesRequested,&dialog,[this,&dialog](const QString &name){ReleasesDialog releases({},&github,&dialog,name);releases.exec();}); connect(&dialog,&RepoBrowser::cloneRequested,this,[this](const QString &url,const QString &name){QTimer::singleShot(0,this,[this,url,name]{cloneUrl(url,name);});}); dialog.exec();
}

void MainWindow::showReleases(){if(repository.isEmpty())return;ReleasesDialog dialog(repository,&github,this);dialog.exec();}

void MainWindow::repositorySettings() {
    if(repository.isEmpty()||runner.busy())return;
    RepoSettings dialog(repository,github.account(),this);if(dialog.exec()==QDialog::Accepted)refresh();
}

void MainWindow::publishRepository() {
    if(repository.isEmpty()||runner.busy()||indexer.busy()||pushCheck.busy())return;
    const auto path=repository;
    runGit({"remote","get-url","origin"},[this,path](const CommandResult &remote){
        if(repository!=path)return;
        if(remote.ok()){QMessageBox::information(this,"Publication","Ce dépôt possède déjà un origin. Modifiez-le dans les réglages du dépôt ou publiez sa branche.");return;}
        runGit({"rev-parse","--verify","HEAD"},[this,path](const CommandResult &head){
            const bool hasCommits=head.ok();
            QTimer::singleShot(0,this,[this,path,hasCommits]{
                if(repository!=path||runner.busy())return;
                RepositoryPublishDialog dialog(&github,QFileInfo(path).fileName(),hasCommits,this);
                if(dialog.exec()!=QDialog::Accepted)return;
                const auto object=dialog.repository();const auto url=object["clone_url"].toString();const auto html=object["html_url"].toString();const bool sendCommits=dialog.sendExistingCommits();
                if(GitHubClient::repositoryFromRemote(url)!=object["full_name"].toString()||object["full_name"].toString().isEmpty()){QMessageBox::warning(this,"Origin","GitHub a retourné une URL de dépôt non reconnue. Vérifiez le dépôt créé avant de configurer origin.");return;}
                if(repository!=path){QMessageBox::information(this,"Dépôt créé","Le dépôt distant est créé : "+html+"\nRouvrez le dossier initial pour configurer origin.");return;}
                beginOperation("Liaison du dépôt GitHub");
                runner.run("git",{"remote","add","origin",url},path,[this,path,html,sendCommits](const CommandResult &result){
                    if(!result.ok()){indexError="Le dépôt GitHub a été créé : "+html+"\nMais origin n’a pas été configuré : "+result.message();operation->finish(false,indexError);return;}
                    log("Dépôt GitHub créé et lié à origin : "+html);
                    if(sendCommits)QTimer::singleShot(0,this,[this,path]{if(repository==path)mutate({"push","--set-upstream","origin","HEAD"},"Publication des commits");});else {refresh();syncMonitor.inspect(path,true);}
                });
            });
        });
    });
}

void MainWindow::generateMessage() {
    if(codex.generating()){++messageRevision;codex.cancelGeneration();generationStatus->setText("Génération annulée.");return;}
    if(repository.isEmpty()||runner.busy())return;
    if(ToolRuntime::executable("codex").isEmpty()){AccountDialog dialog("codex",&github,&codex,this);dialog.exec();return;}
    const int revision=++messageRevision;generationRepository=repository;generationAbbreviated=false;previousSummary=summary->text();previousDescription=description->toPlainText();beginOperation("Préparation du message Codex");generationStatus->setText("Lecture des modifications…");
    indexer.prepare(repository,[this,revision](QString error){
        if(revision!=messageRevision)return;
        if(!error.isEmpty()){indexError=error;generationStatus->setError(error);operation->finish(false,error);return;}
        runGit({"diff","--cached","--raw","--no-abbrev","-z","--"},[this,revision](const CommandResult &raw){
            if(revision!=messageRevision)return;
            if(!raw.ok()||raw.output.isEmpty()){generationStatus->setText("Aucune modification à décrire.");operation->finish(true,"Aucune modification à décrire.");return;}
            stagedFingerprint=QCryptographicHash::hash(raw.output,QCryptographicHash::Sha256);
            runner.run("git",{"diff","--cached","--stat=120,80,200","--no-renames","--"},repository,[this,revision](const CommandResult &stats){
                if(revision!=messageRevision)return;
                if(!stats.ok()){generationStatus->setText(stats.message());operation->finish(false,stats.message());return;}
                generationAbbreviated=stats.truncated;const QString overview=QString::fromUtf8(stats.output);
                runner.run("git",{"--no-pager","diff","--cached","--no-ext-diff","--no-textconv","--no-color","--unified=3","--"},repository,[this,revision,overview](const CommandResult &diff){
                    if(revision!=messageRevision)return;
                    if(!diff.ok()){generationStatus->setText(diff.message());operation->finish(false,diff.message());return;}
                    generationAbbreviated=generationAbbreviated||diff.truncated;
                    const QString context="Résumé des changements :\n"+overview+"\nExtraits des modifications :\n"+QString::fromUtf8(diff.output)+(diff.truncated?"\n[Extraits abrégés : certains contenus ne sont pas inclus. Fonder le message sur le résumé et les extraits disponibles.]":QString());
                    codex.generateCommit(context);
                },{},96*1024,true);
            },{},24*1024,true);
        });
    });
}

void MainWindow::applyGeneratedMessage(const QString &message,const QString &body) {
    if(repository!=generationRepository){generationStatus->setText("Le dépôt a changé. La proposition n’a pas été appliquée.");return;}
    const auto path=repository;const auto fingerprint=stagedFingerprint;
    runGit({"diff","--cached","--raw","--no-abbrev","-z","--"},[this,path,fingerprint,message,body](const CommandResult &raw){
        if(repository!=path||!raw.ok()||QCryptographicHash::hash(raw.output,QCryptographicHash::Sha256)!=fingerprint){generationStatus->setText("Les fichiers préparés ont changé. Relancez la génération.");return;}
        if(summary->text()!=previousSummary||description->toPlainText()!=previousDescription){
            GlassDialog review(this);review.setWindowTitle("Proposition Codex");review.resize(540,350);auto *layout=new QVBoxLayout(&review);auto *hint=new QLabel("Votre brouillon a changé pendant la génération. Voici la proposition Codex.");hint->setWordWrap(true);layout->addWidget(hint);auto *text=new QPlainTextEdit(message+"\n\n"+body);text->setReadOnly(true);layout->addWidget(text);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText("Utiliser la proposition");buttons.button(QDialogButtonBox::Cancel)->setText("Garder mon brouillon");layout->addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&review,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&review,&QDialog::reject);if(review.exec()!=QDialog::Accepted)return;
        }
        summary->selectAll();summary->insert(message);description->selectAll();description->insertPlainText(body);generationStatus->setText(generationAbbreviated?"Proposition ajoutée à partir d’extraits abrégés. Vérifiez qu’elle décrit bien tous les changements.":"Proposition ajoutée. Relisez-la avant de créer le commit.");
    });
}

void MainWindow::previewAppearance(const QJsonObject &p) {
    if(!Theme::validate(p["theme"].toObject()))return;
    activeTheme=p["theme"].toObject();fontSize=qBound(10,p["fontSize"].toInt(11),22);native=p["native"].toBool();
    Visuals::configure(p["reduceMotion"].toBool(),p["reduceTransparency"].toBool(),p["glassStrength"].toInt(80));Theme::apply(activeTheme,native,fontSize);
    repositorySelector->setIcon(commandIcon("repository"));for(auto *a:toolbarItems)if(!qobject_cast<QWidgetAction *>(a))a->setIcon(commandIcon(a->objectName().mid(5)));
    diff->setProperty("refreshHighlight",true);showText(diff->toPlainText());
}

void MainWindow::initRepository() {
    if (runner.busy()) return;
    auto path = QFileDialog::getExistingDirectory(this, "Dossier du nouveau dépôt"); if (path.isEmpty()) return;
    beginOperation("Création du dépôt local");
    runner.run("git", {"init", "--initial-branch=main", "--", path}, {}, [this, path](const CommandResult &r) {
        if (r.ok()) QTimer::singleShot(0, this, [this, path] { openRepository(path); });
        else Diagnostics::showError(this, "Création impossible", r.message().left(4000));
    });
}

void MainWindow::newBranch() {
    if (repository.isEmpty() || runner.busy()) return;
    bool ok = false; auto name = glassInputText(this, "Nouvelle branche", "Nom", QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    beginOperation("Création de la branche");
    runGit({"check-ref-format", "--branch", name}, [this, name](const CommandResult &r) {
        if (!r.ok()) { Diagnostics::showError(this, "Branche invalide", r.message()); return; }
        runGit({"switch", "-c", name, "--"}, [this](const CommandResult &result) {
            if (!result.ok()) Diagnostics::showError(this, "Branche", result.message());
            refresh();
        });
    });
}

void MainWindow::log(const QString &message) {
    activity->appendPlainText(QTime::currentTime().toString("HH:mm:ss") + "  " + message);
}

QList<QDockWidget *> MainWindow::layoutPanels() const {
    QList<QDockWidget *> panels{changesDock, historyDock, commitDock, diffDock, repositoryDock, activityDock};for(auto *panel:customPanels)panels.append(panel);return panels;
}

void MainWindow::setLayoutEditing(bool editing) {
    layoutEditing = editing;
    for (auto *d : layoutPanels()) {
        d->setFeatures(editing ? QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable : QDockWidget::NoDockWidgetFeatures);
        auto *previous = d->titleBarWidget();
        if (editing || d->isFloating()) d->setTitleBarWidget(dockHeader(d));
        else { auto *empty = new QWidget(d); empty->setFixedHeight(0); d->setTitleBarWidget(empty); }
        if (previous && previous != d->titleBarWidget()) previous->deleteLater();
    }
    mainToolbar->setMovable(editing);
    if (lockAction) { const QSignalBlocker block(lockAction); lockAction->setChecked(!editing); }
}

void MainWindow::resetLayout(const QString &preset) {
    for (auto *d : layoutPanels()) { d->setFloating(false); removeDockWidget(d); d->hide(); }
    addDockWidget(Qt::LeftDockWidgetArea, changesDock);
    addDockWidget(Qt::LeftDockWidgetArea, commitDock); splitDockWidget(changesDock, commitDock, Qt::Vertical);
    addDockWidget(Qt::LeftDockWidgetArea, historyDock); tabifyDockWidget(changesDock, historyDock);
    for (auto *bar : findChildren<QTabBar *>()) bar->setDrawBase(false);
    addDockWidget(Qt::RightDockWidgetArea, diffDock);
    addDockWidget(Qt::LeftDockWidgetArea, repositoryDock);
    addDockWidget(Qt::BottomDockWidgetArea, activityDock);
    changesDock->show(); historyDock->show(); commitDock->show(); diffDock->show(); changesDock->raise();
    repositoryDock->hide(); activityDock->hide();
    if (preset == "review") {
        removeDockWidget(historyDock); addDockWidget(Qt::LeftDockWidgetArea, historyDock);
        splitDockWidget(changesDock, historyDock, Qt::Vertical); historyDock->show(); commitDock->hide();
        activityDock->show(); resizeDocks({activityDock}, {140}, Qt::Vertical);
    } else if (preset == "focus") {
        for (auto *d : layoutPanels()) if (d != diffDock) d->hide();
    }
    addToolBar(Qt::TopToolBarArea, mainToolbar); mainToolbar->show();
    for (auto *a : toolbarItems) { mainToolbar->removeAction(a); mainToolbar->addAction(a); a->setVisible(a->property("defaultVisible").toBool()); }
    mainToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); statusBar()->show();
    resizeDocks({changesDock, diffDock}, {300, 900}, Qt::Horizontal);
    resizeDocks({changesDock, commitDock}, {440, 200}, Qt::Vertical);
    setLayoutEditing(layoutEditing);
}

QJsonObject MainWindow::layoutConfiguration() const {
    QJsonArray tools;
    for (auto *action : mainToolbar->actions()) tools.append(QJsonObject{{"id", action->objectName()}, {"visible", action->isVisible()}});
    QJsonArray custom;for(auto *panel:customPanels)custom.append(panel->definition());
    return {{"customPanels",custom},{"version", 1}, {"layoutVersion", 2}, {"layout", QString::fromLatin1(saveState(2).toBase64())},
        {"toolbar", tools}, {"toolbarStyle", int(mainToolbar->toolButtonStyle())}, {"locked", !layoutEditing}, {"statusBar", !statusBar()->isHidden()}};
}

void MainWindow::applyToolbarConfiguration(const QJsonArray &config) {
    if (config.size() > 64) return;
    QList<QAction *> ordered;
    for (const auto &value : config) {
        const auto object = value.toObject();
        for (auto *action : toolbarItems) if (action->objectName() == object["id"].toString() && !ordered.contains(action)) {
            ordered.append(action); action->setVisible(object["visible"].toBool(true)); break;
        }
    }
    for (auto *action : toolbarItems) if (!ordered.contains(action)) ordered.append(action);
    for (auto *action : mainToolbar->actions()) mainToolbar->removeAction(action);
    for (auto *action : ordered) mainToolbar->addAction(action);
}

bool MainWindow::applyLayoutConfiguration(const QJsonObject &p) {
    const auto encoded = p["layout"].toString();
    if (p["version"].toInt() != 1 || encoded.isEmpty() || encoded.size() > 131072 || p["toolbar"].toArray().size() > 64) return false;
    const auto state = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (state.isEmpty()||(p.contains("customPanels")&&!p["customPanels"].isArray())||!CustomPanel::validate(p["customPanels"].toArray())) return false;
    const auto oldCustom=layoutConfiguration()["customPanels"].toArray();
    const auto backup = saveState(2);
    restoreCustomPanels(p["customPanels"].toArray());
    if (!restoreState(state, p["layoutVersion"].toInt(1))) { restoreCustomPanels(oldCustom);restoreState(backup, 2); return false; }
    if (p["toolbar"].isArray()) applyToolbarConfiguration(p["toolbar"].toArray());
    const int style = p["toolbarStyle"].toInt(int(Qt::ToolButtonTextBesideIcon));
    if (style >= Qt::ToolButtonIconOnly && style <= Qt::ToolButtonFollowStyle) mainToolbar->setToolButtonStyle(Qt::ToolButtonStyle(style));
    statusBar()->setVisible(p["statusBar"].toBool(true));
    setLayoutEditing(!p["locked"].toBool(true)); return true;
}

void MainWindow::saveLayoutSettings() {
    settings.setValue("workspace", QJsonDocument(layoutConfiguration()).toJson(QJsonDocument::Compact));
}

void MainWindow::editLayout() {
    for (auto *existing : findChildren<LayoutDialog *>()) if (existing->isVisible()) { existing->raise(); existing->activateWindow(); return; }
    auto *dialog = new LayoutDialog(this); dialog->show();
}

void MainWindow::rebuildLayouts() {
    layoutsMenu->clear(); settings.beginGroup("layouts"); const auto names = settings.childKeys(); settings.endGroup();
    for (const auto &name : names) layoutsMenu->addAction(name, this, [this, name] {
        const auto config = QJsonDocument::fromJson(settings.value("layouts/" + name).toByteArray()).object();
        if (!applyLayoutConfiguration(config)) QMessageBox::information(this, "Disposition", "Cette disposition utilise un ancien format. Créez une nouvelle disposition depuis Personnaliser l’interface.");
    });
    layoutsMenu->setEnabled(!names.isEmpty());
}

QJsonObject MainWindow::profile() const {
    auto p = layoutConfiguration(); p["theme"] = activeTheme; p["native"] = native; p["fontSize"] = fontSize;
    p["liveAppearance"] = settings.value("liveAppearance", true).toBool();
    p["reduceMotion"] = settings.value("reduceMotion", false).toBool();
    p["reduceTransparency"] = settings.value("reduceTransparency", false).toBool();
    p["glassStrength"] = settings.value("glassStrength", 80).toInt(); return p;
}

void MainWindow::applyProfile(const QJsonObject &p) {
    if (p["version"].toInt() != 1 || !Theme::validate(p["theme"].toObject())) return;
    activeTheme = p["theme"].toObject(); native = p["native"].toBool(); fontSize = qBound(10, p["fontSize"].toInt(11), 22);
    settings.setValue("theme", QJsonDocument(activeTheme).toJson(QJsonDocument::Compact));
    settings.setValue("native", native); settings.setValue("fontSize", fontSize);
    settings.setValue("liveAppearance", p["liveAppearance"].toBool(true));
    settings.setValue("reduceMotion", p["reduceMotion"].toBool()); settings.setValue("reduceTransparency", p["reduceTransparency"].toBool());
    settings.setValue("glassStrength", qBound(40, p["glassStrength"].toInt(80), 100));
    Visuals::configure(p["reduceMotion"].toBool(), p["reduceTransparency"].toBool(), p["glassStrength"].toInt(80));
    Theme::apply(activeTheme, native, fontSize); applyLayoutConfiguration(p); saveLayoutSettings(); showDiff();
    repositorySelector->setIcon(commandIcon("repository"));
    for (auto *action : toolbarItems) if (!qobject_cast<QWidgetAction *>(action)) action->setIcon(commandIcon(action->objectName().mid(5)));
    if(!repository.isEmpty())updateSyncStatus(syncState);
}

void MainWindow::settingsDialog() { openSettings(false); }

void MainWindow::openSettings(bool colors) {
    SettingsDialog dialog(profile(), this, &github, &codex); connect(&dialog, &SettingsDialog::profileDownloaded, this, &MainWindow::applyProfile);
    connect(&dialog, &SettingsDialog::appearancePreviewRequested, this, &MainWindow::previewAppearance);
    connect(&dialog, &SettingsDialog::checkUpdatesRequested, this, [this]{checkUpdates(true);});
    connect(&dialog, &SettingsDialog::repositoriesRequested, this, [this] { QTimer::singleShot(0, this, &MainWindow::browseGitHub); });
    connect(&dialog, &SettingsDialog::layoutEditorRequested, this, [this] { QTimer::singleShot(0, this, &MainWindow::editLayout); }); if(colors)dialog.showColors();dialog.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (property("installingUpdate").toBool() || runner.busy() || indexer.busy() || github.busy() || pushCheck.busy()) {
        QMessageBox::information(this, "Opération en cours", "Attendez la fin de l’opération en cours avant de fermer l’application."); event->ignore(); return;
    }
    codex.cancelGeneration();
    for (auto *editor : findChildren<LayoutDialog *>()) if (editor->isVisible()) editor->reject();
    settings.setValue("geometry", saveGeometry()); saveLayoutSettings(); event->accept();
}

void MainWindow::restoreCustomPanels(const QJsonArray &definitions){
    for(auto *panel:customPanels){removeDockWidget(panel);delete panel;}customPanels.clear();
    for(const auto &value:definitions){auto *panel=new CustomPanel(value.toObject(),this);customPanels.append(panel);addDockWidget(Qt::RightDockWidgetArea,panel);panel->show();panel->updateRepository(syncState);connect(panel,&CustomPanel::contentChanged,this,[this]{saveLayoutSettings();});}
}

void MainWindow::checkUpdates(bool manual){
    if(manual){
        auto *parent=QApplication::activeModalWidget();
        if(!parent)parent=this;
        if(!updateDialog){
            updateDialog=new UpdateDialog(updater,parent);
            connect(updateDialog,&UpdateDialog::installRequested,this,&MainWindow::installUpdate);
        }else if(parent!=updateDialog&&updateDialog->parentWidget()!=parent){
            updateDialog->setParent(parent,updateDialog->windowFlags());
        }
        updateDialog->setWindowModality(parent==this?Qt::NonModal:Qt::WindowModal);
        updateDialog->show();updateDialog->raise();
    }
    updater->check(manual);
}
void MainWindow::installUpdate(const QString &file){
    if(runner.busy()||indexer.busy()||github.busy()||pushCheck.busy()||codex.generating()){Diagnostics::showError(this,"Mise à jour","Une opération est en cours. Attendez sa fin avant d’installer la mise à jour.");return;}
#ifdef Q_OS_LINUX
    const auto image=qEnvironmentVariable("APPIMAGE");
    if(image.isEmpty()){QDesktopServices::openUrl(updater->available().releaseUrl);return;}
    if(property("installingUpdate").toBool())return;
    setProperty("installingUpdate",true);updateActions();beginOperation("Installation de la mise à jour");operation->step("Vérification, sauvegarde et remplacement de l’AppImage…");
    auto outcome=std::make_shared<QPair<bool,QString>>();const auto expectedHash=updater->available().sha256;
    auto *worker=QThread::create([outcome,image,file,expectedHash]{outcome->first=AppUpdater::replaceAppImage(image,file,expectedHash,&outcome->second);});
    connect(worker,&QThread::finished,this,[this,outcome,image]{setProperty("installingUpdate",false);updateActions();if(!outcome->first){operation->finish(false,outcome->second);return;}operation->finish(true,"La mise à jour a été installée.");
    GlassDialog restart(this);restart.setWindowTitle("Mise à jour installée");auto *layout=new QVBoxLayout(&restart);layout->setContentsMargins(24,24,24,24);auto *label=new QLabel("La nouvelle version est installée. Une sauvegarde .previous a été conservée. Redémarrez pour l’utiliser.");label->setWordWrap(true);layout->addWidget(label);auto *buttons=new QDialogButtonBox;buttons->addButton("Plus tard",QDialogButtonBox::RejectRole);auto *now=buttons->addButton("Redémarrer",QDialogButtonBox::AcceptRole);layout->addWidget(buttons);connect(now,&QPushButton::clicked,&restart,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&restart,&QDialog::reject);
    if(restart.exec()==QDialog::Accepted){if(runner.busy()||indexer.busy()||github.busy()||pushCheck.busy()){statusBar()->showMessage("Redémarrez après la fin de l’opération en cours.");return;}auto env=QProcessEnvironment::systemEnvironment();for(const auto &key:{"APPIMAGE","APPDIR","LD_LIBRARY_PATH","QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH"})env.remove(key);QProcess next;next.setProcessEnvironment(env);next.setProgram(image);if(next.startDetached())close();else Diagnostics::showError(this,"Redémarrage","Impossible de relancer l’application. Fermez-la puis ouvrez la nouvelle AppImage.");}
    });connect(worker,&QThread::finished,worker,&QObject::deleteLater);worker->start();
#else
    QFile input(file);QCryptographicHash digest(QCryptographicHash::Sha256);if(!input.open(QIODevice::ReadOnly)||!digest.addData(&input)||digest.result().toHex()!=updater->available().sha256.toLatin1()){Diagnostics::showError(this,"Installation","La mise à jour a changé depuis sa vérification. Téléchargez-la à nouveau.");return;}
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(file)))Diagnostics::showError(this,"Installation","Impossible d’ouvrir le paquet téléchargé. Ouvrez-le depuis votre dossier de cache.");
#ifdef Q_OS_WIN
    else close();
#endif
#endif
}

void MainWindow::mergeBranches(){if(repository.isEmpty()||runner.busy()||indexer.busy()||pushCheck.busy())return;MergeDialog dialog(repository,syncState.upstream,this);dialog.exec();refresh();syncMonitor.inspect(repository,true);}

void MainWindow::probeRepository(){
    if(repository.isEmpty()||layoutEditorOpen()||!isActiveWindow()||QApplication::activeModalWidget()||property("installingUpdate").toBool()||runner.busy()||repositoryProbe.busy()||refreshing||indexer.busy()||pushCheck.busy()||github.busy()||codex.generating())return;
    const auto path=repository;
    repositoryProbe.run("git",{"--no-optional-locks","status","--porcelain=v2","--branch","-z","--untracked-files=all"},path,[this,path](const CommandResult &status){
        if(repository!=path)return;
        const auto signature=status.ok()?status.output:status.error;
        repositoryProbe.run("git",{"for-each-ref","--format=%(refname)%00%(objectname)","refs/heads/","refs/remotes/"},path,[this,path,signature](const CommandResult &refs){
            if(repository!=path||layoutEditorOpen()||runner.busy()||refreshing||indexer.busy()||pushCheck.busy()||github.busy()||codex.generating()||QApplication::activeModalWidget())return;
            const auto next=signature+'\0'+refs.output;if(next==repositorySnapshot)return;repositorySnapshot=next;refreshRepository(true);
        });
    });
}

bool MainWindow::layoutEditorOpen()const{for(auto *editor:findChildren<LayoutDialog *>())if(editor->isVisible())return true;return false;}
