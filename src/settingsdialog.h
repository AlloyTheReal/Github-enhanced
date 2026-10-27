#pragma once
#include "diagnostics.h"
#include "commandrunner.h"
#include "visuals.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QHash>
class GitHubClient;
class CodexClient;
class QTimer;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QSlider;
class QPushButton;

class SettingsDialog : public GlassDialog {
    Q_OBJECT
  public:
    explicit SettingsDialog(const QJsonObject &profile, QWidget *parent = nullptr,
                            GitHubClient *github = nullptr, CodexClient *codex = nullptr);
    void showColors() { navigate(1); }
  signals:
    void profileDownloaded(const QJsonObject &profile);
    void layoutEditorRequested();
    void repositoriesRequested();
    void appearancePreviewRequested(const QJsonObject &profile);

  private:
    QJsonObject profile;
    CommandRunner runner;
    QNetworkAccessManager network;
    QComboBox *themeBox;
    QCheckBox *nativeBox;
    QSpinBox *fontBox;
    QLabel *accountLabel;
    DiagnosticLabel *syncStatus;
    QListWidget *catalog;
    QLineEdit *catalogUrl;
    QString login;
    GitHubClient *github;
    CodexClient *codex;
    DiagnosticLabel *codexStatus;
    QCheckBox *liveAppearance;
    QTimer *appearanceTimer;
    QJsonObject draft;
    TransitionStack *pages;
    QListWidget *navigation;
    QLineEdit *themeName = nullptr;
    QCheckBox *motionBox;
    QCheckBox *transparencyBox;
    QSlider *strengthSlider;
    QWidget *themePreview = nullptr;
    QLabel *pageTitle;
    QLabel *pageSubtitle;
    QHash<QString, QPushButton *> swatches;
    QHash<QString, QLineEdit *> colorFields;
    QList<QJsonObject> themes;
    QList<QJsonObject> catalogThemes;
    void loadThemes(const QString &selected = {});
    void editTheme();
    void importTheme();
    void exportTheme();
    void fetchCatalog();
    void identify();
    void sync(bool upload);
    QJsonObject currentProfile() const;
    void changeColor(const QString &key, const QColor &color);
    void refreshColors();
    void makeLocalTheme();
    void navigate(int index);

  protected:
    void done(int result) override;
};
