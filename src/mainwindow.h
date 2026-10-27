#pragma once
#include "diagnostics.h"
#include "commandrunner.h"
#include "gitstatus.h"
#include "githubclient.h"
#include "codexclient.h"
#include "repositoryindex.h"
#include "repositorysync.h"
#include "pushpreflight.h"
#include "operationdialog.h"
#include <QMainWindow>
#include <QJsonObject>
#include <QSettings>
#include <QJsonArray>
class QListWidget;
class QTreeWidget;
class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QStackedWidget;
class QTabWidget;
class QLabel;
class QDockWidget;
class QMenu;
class QToolButton;
class QToolBar;
class LayoutDialog;
class QPushButton;
class CustomPanel;
class AppUpdater;
class UpdateDialog;
class DiffViewer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    void openRepository(const QString &path);
    void checkUpdates(bool manual=true);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    friend class LayoutDialog;
    QSettings settings;
    AppUpdater *updater;
    QPointer<UpdateDialog> updateDialog;
    void installUpdate(const QString &file);
    QList<CustomPanel *> customPanels;
    void restoreCustomPanels(const QJsonArray &definitions);
    CommandRunner runner;
    CommandRunner diffRunner;
    CommandRunner repositoryProbe;
    QByteArray repositorySnapshot;
    bool quietRefresh=false;
    void probeRepository();
    bool layoutEditorOpen() const;
    void refreshRepository(bool quiet);
    GitHubClient github;
    CodexClient codex;
    RepositoryIndex indexer;
    RepositorySync syncMonitor;
    PushPreflight pushCheck;
    RepositorySyncState syncState;
    QTimer remoteTimer;
    QLabel *syncStatus;
    QPushButton *syncAction;
    QPushButton *verifyRemote;
    QByteArray historySnapshot;
    OperationDialog *operation;
    bool refreshing = false;
    bool localStatusKnown = false;
    QString indexError;
    QString selectedCommit;
    QWidget *commitBrowser;
    QTreeWidget *commitFiles;
    QLabel *commitInfo;
    QLabel *commitSubject;
    QLabel *commitMeta;
    bool generationAbbreviated = false;
    QComboBox *commitView;
    QTimer repositoryTimer;
    QString repository;
    QString branch;
    QStringList recent;
    QList<FileChange> changes;
    QJsonObject activeTheme;
    bool native = false;
    int fontSize = 11;
    bool layoutEditing = false;
    int diffRevision = 0;
    QListWidget *repositories;
    QTreeWidget *files;
    QTreeWidget *history;
    QPlainTextEdit *diff;
    DiffViewer *diffViewer;
    QPlainTextEdit *activity;
    QPlainTextEdit *description;
    QLineEdit *summary;
    QComboBox *branches;
    
    QStackedWidget *pages;
    QLabel *diffTitle;
    QLabel *changeCount;
    QDockWidget *repositoryDock;
    QDockWidget *changesDock;
    QDockWidget *commitDock;
    QDockWidget *activityDock;
    QDockWidget *diffDock;
    QDockWidget *historyDock;
    QToolBar *mainToolbar;
    QToolButton *repositorySelector;
    QMenu *repositoryMenu;
    QList<QAction *> toolbarItems;
    QAction *openAction;
    QAction *lockAction = nullptr;
    QMenu *layoutsMenu;
    QList<QAction *> repoActions;
    QAction *refreshAction;
    QPushButton *generateButton;
    QPushButton *commitButton;
    DiagnosticLabel *generationStatus;
    int messageRevision=0;
    QByteArray stagedFingerprint;
    QString generationRepository;
    QString previousSummary;
    QString previousDescription;
    void buildInterface();
    void buildMenus();
    void rebuildRecent();
    void refresh();
    void refreshViews();
    void updateActions();
    void updateSyncStatus(const RepositorySyncState &state);
    void performSyncAction();
    void showSyncDetails();
    void mergeBranches();
    QAction *syncToolbarAction=nullptr;
    void showDiff();
    void showText(const QString &text);
    void runGit(const QStringList &args, CommandRunner::Callback callback);
    void mutate(const QStringList &args, const QString &label);
    void runMutation(const QStringList &args,const QString &label);
    void authorizeWorkflowPermission(const QString &path,const QStringList &retryArguments={},const QString &retryTitle={});
    void checkedPush(const QStringList &args,const QString &label);
    void showCommit(const QString &sha);
    void showCommitFile();
    void removeRecent(const QString &path);
    void beginOperation(const QString &title);
    void commit();
    void cloneRepository();
    void cloneUrl(const QString &url,const QString &suggestedName={});
    void browseGitHub();
    void showReleases();
    void publishRepository();
    void repositorySettings();
    void generateMessage();
    void applyGeneratedMessage(const QString &summary,const QString &body);
    void previewAppearance(const QJsonObject &profile);
    void initRepository();
    void newBranch();
    void settingsDialog();
    void openSettings(bool colors);
    void rebuildLayouts();
    void editLayout();
    void setLayoutEditing(bool editing);
    void resetLayout(const QString &preset = "desktop");
    QList<QDockWidget *> layoutPanels() const;
    QJsonObject layoutConfiguration() const;
    bool applyLayoutConfiguration(const QJsonObject &configuration);
    void applyToolbarConfiguration(const QJsonArray &configuration);
    void saveLayoutSettings();
    QJsonObject profile() const;
    void applyProfile(const QJsonObject &profile);
    void log(const QString &message);
};
