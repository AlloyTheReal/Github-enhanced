#include "theme.h"
#include "visuals.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QPalette>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStyle>
#include <QFont>

bool Theme::validate(const QJsonObject &o, QString *error) {
    auto fail = [error](const QString &why) { if (error) *error = why; return false; };
    if (QJsonDocument(o).toJson(QJsonDocument::Compact).size() > 65536) return fail("Thème trop volumineux (64 Kio maximum).");
    static const QRegularExpression id("^[a-z0-9][a-z0-9-]{0,47}$");
    static const QRegularExpression color("^#[0-9a-fA-F]{6}$");
    if (o["version"].toInt() != 1 || !id.match(o["id"].toString()).hasMatch())
        return fail("Version de thème ou identifiant invalide.");
    for (const auto &key : {"name", "author"})
        if (o[key].toString().trimmed().isEmpty() || o[key].toString().size() > 80)
            return fail("Nom et auteur obligatoires (80 caractères maximum).");
    for (const auto &key : {"background", "surface", "text", "muted", "accent", "border", "added", "removed"})
        if (!color.match(o[key].toString()).hasMatch()) return fail(QString("Couleur invalide : %1").arg(key));
    return true;
}

QJsonObject Theme::read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return {};
    const auto o = QJsonDocument::fromJson(file.readAll()).object();
    return validate(o) ? o : QJsonObject{};
}

QString Theme::directory() {
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
    QDir().mkpath(path);
    return path;
}

QList<QJsonObject> Theme::installed() {
    QList<QJsonObject> list;
    for (const auto &name : {"midnight", "daylight", "forest", "obsidian"}) {
        auto theme = read(QString(":/themes/%1.json").arg(name));
        if (!theme.isEmpty()) list.append(theme);
    }
    const QDir dir(directory());
    for (const auto &file : dir.entryList({"*.json"}, QDir::Files)) {
        auto theme = read(dir.filePath(file));
        if (!theme.isEmpty()) {
            for (qsizetype i = list.size(); i-- > 0;) if (list[i]["id"] == theme["id"]) list.removeAt(i);
            list.append(theme);
        }
    }
    return list;
}

bool Theme::install(const QJsonObject &object, QString *error) {
    if (!validate(object, error)) return false;
    QSaveFile file(directory() + '/' + object["id"].toString() + ".json");
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(object).toJson()) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

void Theme::apply(const QJsonObject &o, bool native, int fontSize) {
    auto font = QApplication::font(); font.setPointSize(fontSize); QApplication::setFont(font);
    Q_UNUSED(native);
    Visuals::install();
    if (!validate(o)) return;
    auto c = [&](const char *key) { return QColor(o[key].toString()); };
    QPalette p;
    p.setColor(QPalette::Window, c("background")); p.setColor(QPalette::WindowText, c("text"));
    p.setColor(QPalette::Base, c("surface")); p.setColor(QPalette::AlternateBase, c("background"));
    p.setColor(QPalette::Text, c("text"));
    p.setColor(QPalette::Button, Visuals::mix(c("surface"), c("text"), c("surface").lightness() < 128 ? .085 : .045));
    p.setColor(QPalette::Mid, c("border")); p.setColor(QPalette::Dark, c("background"));
    p.setColor(QPalette::ButtonText, c("text")); p.setColor(QPalette::Highlight, c("accent"));
    p.setColor(QPalette::HighlightedText, Visuals::foreground(c("accent"))); p.setColor(QPalette::PlaceholderText, c("muted"));
    p.setColor(QPalette::Link, c("accent")); p.setColor(QPalette::LinkVisited, c("accent"));
    p.setColor(QPalette::ToolTipBase, c("surface")); p.setColor(QPalette::ToolTipText, c("text"));
    p.setColor(QPalette::Disabled, QPalette::Text, c("muted"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c("muted"));
    qApp->setPalette(p);
    auto rgba = [](QColor color, int opacity) { return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(opacity); };
    QString css = R"(
        QMainWindow, QDialog { background: @background; color: @text; }
        QDialog#settingsDialog, QDialog#layoutEditor, QDialog#colorPicker { background: transparent; }
        QMainWindow::separator { background: @border; width: 1px; height: 1px; }
        QToolBar { background: @toolbar; border: 1px solid @hairline; border-radius: 14px; spacing: 6px; padding: 5px 10px; margin: 8px 10px; }
        QToolBar::separator { width: 1px; background: @hairline; margin: 6px; }
        QWidget#toolbarBlock { border: 0; background: transparent; }
        QToolBar QToolButton, QToolBar QComboBox { min-height: 28px; padding: 4px 10px; border: 0; background: transparent; }
        QToolButton#repositorySelector { font-weight: 500; }
        QDockWidget { color: @text; border: 0; }
        QDockWidget::title { background: @background; padding: 8px 12px; font-weight: 500; }
        QWidget#sidebar { background: @background; }
        QWidget#welcome, QWidget#diffPage { background: @surface; }
        QWidget#sectionHeader { background: @background; border-bottom: 1px solid @hairline; }
        QLabel#caption { font-size: 11px; color: @muted; }
        QLabel#headline { font-size: 22px; font-weight: 600; color: @text; }
        QLabel#pageTitle { font-size: 24px; font-weight: 600; color: @text; background: transparent; }
        QLabel#fileTitle { font-weight: 500; }
        QLabel#muted, QLabel#repositoryBrowserStatus, QLabel#changeCount, QLabel#rowSubtitle { color: @muted; background: transparent; }
        QLabel#rowTitle { color: @text; font-size: 14px; background: transparent; }
        QLabel#rowSubtitle { font-size: 12px; }
        QLabel#changeCount { border-bottom: 1px solid @hairline; }
        QLabel { background: transparent; }
        QListWidget, QTreeWidget { background: @surface; color: @text; border: 0; outline: 0; padding: 0; }
        QListWidget::item, QTreeWidget::item { padding: 8px 10px; border: 0; }
        QListWidget#layoutPanels::item { padding: 4px 8px; }
        QListWidget#recentRepositories::item:selected { background: @navSelection; color: @text; }
        QListWidget::item:selected, QTreeWidget::item:selected { background: @accent; color: @selectionText; }
        QListWidget::item:hover:!selected, QTreeWidget::item:hover:!selected { background: @background; }
        QPlainTextEdit, QTextEdit, QLineEdit, QComboBox, QSpinBox {
            background: @field; color: @text; border: 1px solid @hairline; border-radius: 10px;
            padding: 7px 10px; selection-background-color: @accent; selection-color: @selectionText;
        }
        QComboBox { padding: 7px 30px 7px 12px; min-height: 22px; }
        QComboBox::drop-down { border: 0; width: 28px; }
        QComboBox::down-arrow { width: 0; height: 0; image: none; }
        QSpinBox::up-button, QSpinBox::down-button { width: 24px; border: 0; background: transparent; }
        QSpinBox::up-arrow, QSpinBox::down-arrow { width: 0; height: 0; image: none; }
        QSpinBox QLineEdit { border: 0; padding: 0; background: transparent; }
        QFrame#glassComboPopup { background: @background; border: 1px solid @hairline; border-radius: 12px; }
        QComboBox QAbstractItemView { background: @background; color: @text; border: 1px solid @hairline; border-radius: 12px; padding: 6px; selection-background-color: @card; selection-color: @text; outline: 0; }
        QComboBox QAbstractItemView::item { min-height: 28px; padding: 4px 12px; border-radius: 7px; }
        QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover { background: @card; color: @text; }
        QComboBox QAbstractItemView::item:disabled { color: @muted; background: transparent; }
        QPlainTextEdit#diffEditor { border: 0; border-radius: 0; padding: 14px 18px; background: @surface; }
        QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: @focus; }
        QPushButton, QToolButton { background: transparent; color: @text; border: 0; border-radius: 11px; padding: 8px 16px; min-height: 20px; }
        QPushButton:disabled, QToolButton:disabled { color: @muted; }
        QTabWidget::pane { border: 0; background: transparent; }
        QTabBar::tab { background: transparent; color: @muted; padding: 10px 18px; border: 0; }
        QTabBar::tab:selected { background: @card; color: @text; border-radius: 10px; }
        QHeaderView::section { background: @background; color: @muted; border: 0; padding: 8px; }
        QStatusBar { background: @background; color: @muted; border: 0; font-size: 11px; }
        QSplitter::handle { background: @hairline; }
        QProgressBar { background: @card; border: 0; border-radius: 4px; min-height: 8px; max-height: 8px; }
        QProgressBar::chunk { background: @accent; border-radius: 4px; }
        QToolButton#removeRecent, QToolButton#removeRecentMenu { padding: 2px; min-width: 22px; min-height: 22px; border-radius: 11px; }
        QToolButton#removeRecent:hover, QToolButton#removeRecentMenu:hover { background: @card; }
        QWidget#commitBrowser { background: @background; }
        QTreeWidget#commitFiles { background: transparent; }
        QFrame#settingsSidebar { background: @sidebar; border-top-left-radius: 24px; border-bottom-left-radius: 24px; }
        QFrame#settingsContent { background: @content; border-radius: 22px; }
        QFrame#settingsCard { background: @card; border: 1px solid @cardBorder; border-radius: 14px; }
        QFrame#segmented { background: @field; border: 1px solid @hairline; border-radius: 12px; }
        QListWidget#settingsNavigation { background: transparent; border: 0; padding: 0; }
        QListWidget#settingsNavigation::item { color: @muted; padding: 11px 14px; border: 0; margin: 2px 0; border-radius: 12px; }
        QListWidget#settingsNavigation::item:selected { background: @navSelection; color: @text; }
        QListWidget#settingsNavigation::item:hover:!selected { background: @navHover; }
        QListWidget#settingsNavigation::item:disabled { background: transparent; color: @muted; padding: 10px 10px 4px 10px; font-size: 11px; }
        QScrollArea, QStackedWidget, QWidget#settingsPage { background: transparent; border: 0; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QLineEdit#settingsSearch { background: @card; border: 1px solid @hairline; border-radius: 12px; min-height: 22px; padding: 9px 12px; }
        QLineEdit#hexField { font-family: monospace; padding: 5px 8px; min-height: 18px; border: 0; background: @field; }
        QSlider::groove:horizontal { height: 5px; border-radius: 2px; background: @border; }
        QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
        QSlider::handle:horizontal { width: 16px; margin: -6px 0; background: @text; border: 1px solid @hairline; border-radius: 8px; }
        QSlider#hueSlider::groove:horizontal { height: 10px; border-radius: 5px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #ff0000,stop:0.17 #ffff00,stop:0.33 #00ff00,stop:0.5 #00ffff,stop:0.67 #0000ff,stop:0.83 #ff00ff,stop:1 #ff0000); }
        QSlider#hueSlider::sub-page:horizontal { background: transparent; }
        QMenu { background: @background; color: @text; border: 1px solid @hairline; padding: 6px; }
        QMenu::item { padding: 7px 24px 7px 14px; border-radius: 7px; }
        QMenu::item:selected { background: @card; }
        QGroupBox { border: 1px solid @hairline; border-radius: 12px; margin-top: 12px; padding: 14px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; color: @muted; }
    )";
    const bool dark = c("surface").lightness() < 128;
    css.replace("@scrollThumb", rgba(c("text"), 75));
    css.replace("@scrollHover", rgba(c("text"), 125));
    css.replace("@selectionText", Visuals::foreground(c("accent")).name());
    css.replace("@toolbar", rgba(Visuals::mix(c("surface"), c("text"), dark ? .045 : .025), 240));
    css.replace("@sidebar", rgba(Visuals::mix(c("surface"), c("text"), dark ? .1 : .04), 125));
    css.replace("@content", rgba(c("surface"), 210));
    css.replace("@cardBorder", rgba(c("text"), dark ? 9 : 14));
    css.replace("@card", rgba(Visuals::mix(c("surface"), c("text"), dark ? .09 : .045), 240));
    css.replace("@field", rgba(Visuals::mix(c("surface"), c("text"), dark ? .055 : .025), 235));
    css.replace("@hairline", rgba(c("text"), dark ? 22 : 25));
    css.replace("@navSelection", rgba(c("text"), dark ? 25 : 20));
    css.replace("@navHover", rgba(c("text"), dark ? 12 : 10));
    css.replace("@focus", rgba(c("accent"), 135));
    for (const auto &key : {"background", "surface", "text", "muted", "accent", "border"})
        css.replace(QString('@') + key, o[key].toString());
    qApp->setStyleSheet(css);
}
