#include "mainwindow.h"
#include "custompanel.h"
#include "accountdialog.h"
#include "layoutdialog.h"
#include "settingsdialog.h"
#include "theme.h"
#include "layoutcanvas.h"
#include "toolruntime.h"
#include "capsulescrollbar.h"
#include <QtTest>
#include <QtWidgets>

class TestEnvironment {
    QByteArray key,value;bool existed;
public:
    TestEnvironment(const char *name,const QByteArray &replacement):key(name),value(qgetenv(name)),existed(qEnvironmentVariableIsSet(name)){qputenv(name,replacement);}
    ~TestEnvironment(){if(existed)qputenv(key,value);else qunsetenv(key);}
};

static bool mainIdle(MainWindow &window){
    return !window.findChild<CommandRunner *>()->busy()&&!window.findChild<RepositoryIndex *>()->busy()&&!window.findChild<GitHubClient *>()->busy()&&!window.findChild<PushPreflight *>()->busy();
}

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("GitHubEnhancedTests");
        QCoreApplication::setApplicationName("UI");
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }
    void customPanelsPersistAndUndo(){
        QTemporaryDir config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());
        MainWindow window;window.show();window.findChild<QAction *>("tool-layout")->trigger();auto *editor=window.findChild<LayoutDialog *>();QVERIFY(editor);auto *create=editor->findChild<QPushButton *>("addCustomPanel");QVERIFY(create);
        QTimer::singleShot(0,editor,[&]{auto *form=editor->findChild<QDialog *>("customPanelEditor");if(!form)return;form->findChild<QLineEdit *>("customPanelTitle")->setText("Carnet du projet");for(auto *box:form->findChildren<QDialogButtonBox *>())box->button(QDialogButtonBox::Save)->click();});
        create->click();auto panels=window.findChildren<CustomPanel *>();QCOMPARE(panels.size(),1);QTRY_VERIFY(panels.first()->isVisible());auto *notes=panels.first()->findChild<QPlainTextEdit *>("customPanelNotes");QVERIFY(notes);notes->setPlainText("Notes conservées après redémarrage");const auto id=panels.first()->objectName();
        editor->findChild<QPushButton *>("layoutUndo")->click();QCOMPARE(window.findChildren<CustomPanel *>().size(),0);
        editor->findChild<QPushButton *>("layoutRedo")->click();panels=window.findChildren<CustomPanel *>();QCOMPARE(panels.size(),1);QCOMPARE(panels.first()->findChild<QPlainTextEdit *>("customPanelNotes")->toPlainText(),"Notes conservées après redémarrage");
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))editor->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/custom-panels.png");
        for(auto *box:editor->findChildren<QDialogButtonBox *>())if(box->button(QDialogButtonBox::Save))box->button(QDialogButtonBox::Save)->click();QTest::qWait(30);QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
        MainWindow restored;restored.show();auto *panel=restored.findChild<CustomPanel *>(id);QVERIFY(panel);QCOMPARE(panel->windowTitle(),"Carnet du projet");QCOMPARE(panel->findChild<QPlainTextEdit *>("customPanelNotes")->toPlainText(),"Notes conservées après redémarrage");QVERIFY(restored.close());
    }
    void palettePresetsRemainEditable(){
        QTemporaryDir config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());const auto base=Theme::read(QFINDTESTDATA("../themes/obsidian.json"));
        SettingsDialog dialog(QJsonObject{{"version",1},{"theme",base},{"fontSize",11}});dialog.show();auto *preset=dialog.findChild<QComboBox *>("palettePreset");QVERIFY(preset);int paper=-1;for(int i=0;i<preset->count();++i)if(preset->itemData(i).toJsonObject()["id"]=="paper")paper=i;QVERIFY(paper>=0);preset->setCurrentIndex(paper);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,paper));
        QLineEdit *background=nullptr;for(auto *field:dialog.findChildren<QLineEdit *>("hexField"))if(field->property("colorRole")=="background")background=field;QVERIFY(background);QCOMPARE(background->text(),"#F5F3EF");
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){auto *nav=dialog.findChild<QListWidget *>("settingsNavigation");for(int i=0;i<nav->count();++i)if(nav->item(i)->data(Qt::UserRole).toInt()==1)nav->setCurrentRow(i);QTest::qWait(150);dialog.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/palette-presets.png");}
        dialog.reject();
    }
    void advancedModeMenu() {
        QTemporaryDir config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());Diagnostics::instance().setAdvanced(false);
        MainWindow window;window.show();auto *action=window.findChild<QAction *>("advancedModeAction");QVERIFY(action);QVERIFY(!action->isChecked());action->trigger();QVERIFY(Diagnostics::advanced());
        auto *operation=window.findChild<OperationDialog *>();auto *toggle=operation->findChild<QCheckBox *>("operationAdvanced");QVERIFY(toggle->isChecked());
        toggle->click();QVERIFY(!action->isChecked());QVERIFY(!Diagnostics::advanced());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void repositoryWorkflow() {
        QTemporaryDir temporary;
        QTemporaryDir configuration;
        QVERIFY(temporary.isValid());
        QCoreApplication::setOrganizationName("GitHubEnhancedTests");
        QCoreApplication::setApplicationName("UI");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configuration.path());
        auto git = [&](const QStringList &args) {
            QProcess p; p.setWorkingDirectory(temporary.path()); p.start("git", args);
            return p.waitForFinished(10000) && p.exitCode() == 0;
        };
        QVERIFY(git({"init", "--initial-branch=main"}));
        QVERIFY(git({"config", "user.name", "UI Test"}));
        QVERIFY(git({"config", "user.email", "ui@example.invalid"}));
        QVERIFY(git({"config", "commit.gpgsign", "false"}));
        QVERIFY(git({"config", "core.hooksPath", temporary.path() + "/no-hooks"}));
        QFile file(temporary.path() + "/hello.cpp"); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("int main() { return 0; }\n"); file.close();
        MainWindow window; window.show(); window.openRepository(temporary.path());
        auto *files = window.findChild<QTreeWidget *>("changedFiles"); QVERIFY(files);
        QTRY_COMPARE_WITH_TIMEOUT(files->topLevelItemCount(), 1, 10000);
        QTRY_VERIFY(files->isEnabled());
        QCOMPARE(QApplication::palette().color(QPalette::Window), QColor("#252224"));
        QCOMPARE(files->topLevelItem(0)->text(0), "A ");
        files->setCurrentItem(files->topLevelItem(0));
        if (!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")) {
            QDir().mkpath(qEnvironmentVariable("GHE_CAPTURE_DIR"));
            QTest::qWait(100); window.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR") + "/workspace.png");
        }
        auto action = [&](const QString &text) -> QAction * {
            for (auto *a : window.findChildren<QAction *>()) if (a->text() == text) return a;
            return nullptr;
        };
        QVERIFY(!action("Indexer la sélection")); QVERIFY(!action("Désindexer la sélection"));
        QTRY_COMPARE(files->topLevelItem(0)->text(0), "A "); QTRY_VERIFY(files->isEnabled());
        auto *summary = window.findChild<QLineEdit *>("commitSummary"); QVERIFY(summary);
        summary->setText("Commit from the actual UI");
        auto *commit = action("Créer le commit"); QVERIFY(commit); QTRY_VERIFY(commit->isEnabled()); commit->trigger();
        QTRY_COMPARE(files->topLevelItemCount(), 0); QTRY_VERIFY(files->isEnabled());
        QVERIFY(summary->text().isEmpty());
        QVERIFY(git({"rev-parse", "--verify", "HEAD"}));

        // Validate the actual preferences dialog and persistence through its Save button.
        auto *preferences = action("Préférences…"); QVERIFY(preferences);
        bool saved = false;
        QTimer::singleShot(100, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *combo = dialog->findChild<QComboBox *>("themeSelector");
            auto *box = dialog->findChild<QDialogButtonBox *>();
            if (combo && box) {
                for (auto *check : dialog->findChildren<QCheckBox *>()) check->setChecked(false);
                combo->setCurrentIndex(0); saved = true; box->button(QDialogButtonBox::Save)->click();
            }
            else dialog->reject();
        });
        preferences->trigger(); QVERIFY(saved);
        QCOMPARE(QJsonDocument::fromJson(QSettings().value("theme").toByteArray()).object()["id"].toString(), "midnight");
        QCOMPARE(QApplication::palette().color(QPalette::Window), QColor("#252528"));
        auto *changes = window.findChild<QDockWidget *>("changes"); QVERIFY(changes);
        window.addDockWidget(Qt::RightDockWidgetArea, changes);
        auto *reset = action("Rétablir la disposition initiale"); QVERIFY(reset); reset->trigger();
        QCOMPARE(window.dockWidgetArea(changes), Qt::LeftDockWidgetArea);
        QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }

    void layoutCustomization() {
        QTemporaryDir configuration;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configuration.path());
        MainWindow window; window.show(); QTest::qWait(100);
        auto *diff = window.findChild<QDockWidget *>("diff");
        auto *changes = window.findChild<QDockWidget *>("changes");
        auto *history = window.findChild<QDockWidget *>("history");
        QVERIFY(diff && changes && history);
        QCOMPARE(window.dockWidgetArea(diff), Qt::RightDockWidgetArea);
        QVERIFY(window.tabifiedDockWidgets(changes).contains(history));
        auto openEditor = [&] {
            for (auto *a : window.findChildren<QAction *>()) if (a->text() == "Personnaliser l’interface…") { a->trigger(); return; }
        };
        openEditor(); auto *dialog = window.findChild<LayoutDialog *>(); QVERIFY(dialog);
        auto *panels = dialog->findChild<QListWidget *>("layoutPanels");
        auto *position = dialog->findChild<QComboBox *>("panelPosition"); QVERIFY(panels && position);
        auto selectPanel = [&](const QString &id) {
            for (int i = 0; i < panels->count(); ++i) if (panels->item(i)->data(Qt::UserRole).toString() == id) panels->setCurrentRow(i);
        };
        selectPanel("diff"); QTest::qWait(100);
        if (!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")) dialog->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR") + "/customization.png");
        const int originalWidth = diff->width();
        dialog->findChild<QSpinBox *>("panelWidth")->setValue(600);
        for (auto *b : dialog->findChildren<QPushButton *>()) if (b->text() == "Appliquer les dimensions") b->click();
        QTest::qWait(50); QVERIFY(diff->width() < originalWidth);
        position->setCurrentIndex(position->findData(int(Qt::LeftDockWidgetArea)));
        QMetaObject::invokeMethod(position, "activated", Q_ARG(int, position->currentIndex()));
        QCOMPARE(window.dockWidgetArea(diff), Qt::LeftDockWidgetArea);
        selectPanel("history"); position->setCurrentIndex(position->findData(-1));
        QMetaObject::invokeMethod(position, "activated", Q_ARG(int, position->currentIndex()));
        QVERIFY(history->isFloating());
        auto *relative = dialog->findChild<QComboBox *>("panelRelativeTo");
        auto *placement = dialog->findChild<QComboBox *>("panelPlacement");
        relative->setCurrentIndex(relative->findData("diff")); placement->setCurrentIndex(2);
        dialog->findChild<QPushButton *>("placePanel")->click();
        QVERIFY(!history->isFloating()); QVERIFY(window.tabifiedDockWidgets(diff).contains(history));
        // Both ordering directions work, including a dock before its target.
        relative->setCurrentIndex(relative->findData("diff")); placement->setCurrentIndex(3);
        dialog->findChild<QPushButton *>("placePanel")->click(); QTest::qWait(30);
        QVERIFY(history->x() < diff->x());
        relative->setCurrentIndex(relative->findData("diff")); placement->setCurrentIndex(2);
        dialog->findChild<QPushButton *>("placePanel")->click();

        auto *tools = dialog->findChild<QListWidget *>("toolbarItems"); QVERIFY(tools);
        for (int i = 0; i < tools->count(); ++i) {
            auto *item = tools->item(i);
            if (item->data(Qt::UserRole).toString() == "tool-push") item->setCheckState(Qt::Unchecked);
            if (item->data(Qt::UserRole).toString() == "tool-fetch") tools->setCurrentItem(item);
        }
        QPushButton *up = nullptr;
        for (auto *b : dialog->findChildren<QPushButton *>()) if (b->text() == "Monter") up = b;
        QVERIFY(up); up->click(); up->click();
        auto *toolbar = window.findChild<QToolBar *>("mainToolbar");
        QCOMPARE(toolbar->actions().first()->objectName(), "tool-fetch");
        QVERIFY(!window.findChild<QAction *>("tool-push")->isVisible());
        bool menuPushVisible = false;
        for (auto *a : window.findChildren<QAction *>()) if (a->text() == "Push" && a->isVisible()) menuPushVisible = true;
        QVERIFY(menuPushVisible); // Hiding a toolbar item must not hide its menu command.
        auto *saveProfile = static_cast<QPushButton *>(nullptr);
        for (auto *b : dialog->findChildren<QPushButton *>()) if (b->text() == "Enregistrer sous…") saveProfile = b;
        QVERIFY(saveProfile);
        QTimer::singleShot(100, &window, [] {
            if (auto *input = qobject_cast<QDialog *>(QApplication::activeModalWidget())) { if(auto *field=input->findChild<QLineEdit *>("inputValue")){field->setText("Disposition de test");input->accept();} }
        });
        saveProfile->click();
        QVERIFY(QSettings().contains("layouts/Disposition de test"));
        auto *profiles = dialog->findChild<QListWidget *>("layoutProfiles"); QVERIFY(profiles);
        QCOMPARE(profiles->count(), 1); profiles->setCurrentRow(0);
        history->hide();
        for (auto *b : dialog->findChildren<QPushButton *>()) if (b->text() == "Charger la disposition") b->click();
        QVERIFY(!history->isHidden()); QVERIFY(window.tabifiedDockWidgets(diff).contains(history));
        auto *box = dialog->findChild<QDialogButtonBox *>(); QVERIFY(box); box->button(QDialogButtonBox::Save)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!window.findChild<LayoutDialog *>());
        QCOMPARE(diff->features(), QDockWidget::NoDockWidgetFeatures);

        // A cancelled live edit must restore tabs, dock positions and toolbar state.
        openEditor(); dialog = window.findChild<LayoutDialog *>(); QVERIFY(dialog);
        panels = dialog->findChild<QListWidget *>("layoutPanels");
        for (int i = 0; i < panels->count(); ++i) if (panels->item(i)->data(Qt::UserRole).toString() == "diff") panels->item(i)->setCheckState(Qt::Unchecked);
        QVERIFY(diff->isHidden());
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
        QVERIFY(!diff->isHidden()); QVERIFY(window.tabifiedDockWidgets(diff).contains(history));
        QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
        MainWindow restored; restored.show(); QTest::qWait(50);
        QCOMPARE(restored.dockWidgetArea(restored.findChild<QDockWidget *>("diff")), Qt::LeftDockWidgetArea);
        QCOMPARE(restored.findChild<QToolBar *>("mainToolbar")->actions().first()->objectName(), "tool-fetch");
        QVERIFY(!restored.findChild<QAction *>("tool-push")->isVisible());
        QVERIFY(restored.close());
    }

    void glassPreferences() {
        QTemporaryDir configuration;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configuration.path());
        MainWindow window; window.show(); QTest::qWait(80);
        const bool originalMotion = Visuals::reducedMotion(), originalTransparency = Visuals::reducedTransparency();
        QJsonObject profile{{"version",1},{"theme",Theme::read(":/themes/obsidian.json")},{"fontSize",11},{"glassStrength",80},{"native",false}};
        SettingsDialog dialog(profile,&window); dialog.show(); QTest::qWait(250);
        auto *navigation = dialog.findChild<QListWidget *>("settingsNavigation");
        auto *pages = dialog.findChild<TransitionStack *>("settingsPages");
        auto *theme = dialog.findChild<QComboBox *>("themeSelector"); QVERIFY(navigation && pages && theme);
        auto navigate = [&](int page) {
            for(int i=0;i<navigation->count();++i)if(navigation->item(i)->data(Qt::UserRole).toInt()==page)navigation->setCurrentRow(i);
            QTest::qWait(190);
        };
        auto capture = [&](const QString &name,QWidget *widget) {
            if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){QDir().mkpath(qEnvironmentVariable("GHE_CAPTURE_DIR"));widget->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+'/'+name+".png");}
        };
        capture("settings-appearance",&dialog);
        navigate(4); QCOMPARE(pages->currentIndex(),4); capture("settings-github",&dialog);
        navigate(6); QCOMPARE(pages->currentIndex(),6); capture("settings-codex",&dialog);
        navigate(0);
        QTest::mouseClick(theme,Qt::LeftButton,{},QPoint(theme->width()-18,theme->height()/2));
        QTRY_VERIFY(theme->view()->isVisible());
        QTest::keyClick(theme->view(),Qt::Key_Escape);
        navigate(1); QCOMPARE(pages->currentIndex(),1); capture("settings-colors",&dialog);
        auto *swatch = dialog.findChild<QPushButton *>("swatch-accent"); QVERIFY(swatch);
        bool picked = false;
        QTimer::singleShot(150,&dialog,[&]{
            auto *picker = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!picker || picker->objectName()!="colorPicker"){if(picker)picker->reject();return;}
            auto *hex=picker->findChild<QLineEdit *>("colorHex");auto *box=picker->findChild<QDialogButtonBox *>();
            if(!hex || !box){picker->reject();return;}
            hex->setText("nothex");
            if(box->button(QDialogButtonBox::Ok)->isEnabled()){picker->reject();return;}
            hex->setText("#b4a5df");QTest::qWait(80);capture("color-picker",picker);
            picked=true;box->button(QDialogButtonBox::Ok)->click();
        });
        swatch->click(); QVERIFY(picked);
        QCOMPARE(swatch->property("swatchColor").value<QColor>(),QColor("#b4a5df"));
        QLineEdit *accentField=nullptr;
        for(auto *field:dialog.findChildren<QLineEdit *>())if(field->property("colorRole").toString()=="accent")accentField=field;
        QVERIFY(accentField);accentField->setFocus();accentField->selectAll();QTest::keyClicks(accentField,"#9ac7ba");
        QCOMPARE(swatch->property("swatchColor").value<QColor>(),QColor("#9ac7ba"));
        navigate(5);
        auto *motion=dialog.findChild<GlassSwitch *>("reduceMotion");auto *transparency=dialog.findChild<GlassSwitch *>("reduceTransparency");QVERIFY(motion && transparency);
        // The entire switch is clickable, including the end opposite the indicator.
        QTest::mouseClick(motion,Qt::LeftButton,{},QPoint(motion->width()-4,motion->height()/2));
        QVERIFY(motion->isChecked()); QVERIFY(Visuals::reducedMotion());
        QTest::mouseClick(transparency,Qt::LeftButton,{},QPoint(transparency->width()-4,transparency->height()/2));
        QVERIFY(transparency->isChecked()); QVERIFY(Visuals::reducedTransparency());
        dialog.reject(); QCOMPARE(Visuals::reducedMotion(),originalMotion); QCOMPARE(Visuals::reducedTransparency(),originalTransparency);

        SettingsDialog saved(profile,&window); QJsonObject received;
        connect(&saved,&SettingsDialog::profileDownloaded,&saved,[&](const QJsonObject &value){received=value;});
        saved.show();QTest::qWait(100);
        auto *savedNav=saved.findChild<QListWidget *>("settingsNavigation");
        for(int i=0;i<savedNav->count();++i)if(savedNav->item(i)->data(Qt::UserRole).toInt()==1)savedNav->setCurrentRow(i);
        QTest::qWait(180);
        for(auto *field:saved.findChildren<QLineEdit *>())if(field->property("colorRole").toString()=="accent"){field->setFocus();field->selectAll();QTest::keyClicks(field,"#9ac7ba");}
        saved.findChild<QDialogButtonBox *>("settingsButtons")->button(QDialogButtonBox::Save)->click();
        QVERIFY(Theme::validate(received["theme"].toObject()));
        QCOMPARE(received["theme"].toObject()["accent"].toString(),"#9ac7ba");
        QVERIFY(received["theme"].toObject()["id"].toString().startsWith("custom-"));
        QFile::remove(Theme::directory()+'/'+received["theme"].toObject()["id"].toString()+".json");
        QVERIFY(window.findChild<QWidget *>("dialogBackdrop")->isHidden());
        QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void directLayoutAndAppearancePreview() {
        QTemporaryDir configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());MainWindow window;window.show();QTest::qWait(80);
        for(auto *a:window.findChildren<QAction *>())if(a->text()=="Personnaliser l’interface…"){a->trigger();break;}
        auto *editor=window.findChild<LayoutDialog *>();QVERIFY(editor);auto *canvas=editor->findChild<LayoutCanvas *>("layoutCanvas");QVERIFY(canvas);QTest::qWait(80);
        auto handles=canvas->splitters();QVERIFY(!handles.isEmpty());int selected=-1;for(int i=0;i<handles.size();++i)if(handles[i].orientation==Qt::Horizontal){selected=i;break;}QVERIFY(selected>=0);
        auto handle=handles[selected];const int initial=handle.first->width();auto point=handle.rect.center().toPoint();
        QTest::mousePress(canvas,Qt::LeftButton,{},point);auto moved=point+QPoint(35,0);QMouseEvent resizeEvent(QEvent::MouseMove,QPointF(moved),QPointF(canvas->mapToGlobal(moved)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&resizeEvent);QTest::qWait(25);QTest::mouseRelease(canvas,Qt::LeftButton,{},moved);
        QTRY_VERIFY(handle.first->width()!=initial);auto *undo=editor->findChild<QPushButton *>("layoutUndo");QVERIFY(undo&&undo->isEnabled());undo->click();QTRY_VERIFY(qAbs(handle.first->width()-initial)<=2);
        auto *commit=window.findChild<QDockWidget *>("commit"),*diff=window.findChild<QDockWidget *>("diff");QVERIFY(commit&&diff);
        auto from=canvas->panelRect("commit").center().toPoint(),to=canvas->panelRect("diff").center().toPoint();QVERIFY(!canvas->panelRect("commit").isEmpty());
        QTest::mousePress(canvas,Qt::LeftButton,{},from);QMouseEvent dragEvent(QEvent::MouseMove,QPointF(to),QPointF(canvas->mapToGlobal(to)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&dragEvent);QTest::mouseRelease(canvas,Qt::LeftButton,{},to);
        QTRY_VERIFY(window.tabifiedDockWidgets(diff).contains(commit));undo->click();QTRY_VERIFY(!window.tabifiedDockWidgets(diff).contains(commit));
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        const auto original=QApplication::palette().color(QPalette::Window);QAction *preferences=window.findChild<QAction *>("preferencesAction");QVERIFY(preferences);bool previewed=false;
        QTimer::singleShot(100,&window,[&]{auto *settings=qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());if(!settings)return;auto *combo=settings->findChild<QComboBox *>("themeSelector");if(!combo){settings->reject();return;}combo->setCurrentIndex(1);QTest::qWait(140);previewed=QApplication::palette().color(QPalette::Window)==QColor("#f5f5f7");settings->reject();});preferences->trigger();QVERIFY(previewed);QCOMPARE(QApplication::palette().color(QPalette::Window),original);QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void commitFilesAndRecentRemoval(){
        QTemporaryDir repository,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());
        auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(repository.path());p.start("git",args);return p.waitForFinished(5000)&&p.exitCode()==0;};
        QVERIFY(git({"init","--initial-branch=main"}));QVERIFY(git({"config","user.name","History Test"}));QVERIFY(git({"config","user.email","history@example.invalid"}));QVERIFY(git({"config","commit.gpgsign","false"}));
        for(auto name:{"alpha.cpp","beta.cpp"}){QFile file(repository.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray("// ")+name+"\nint example = 42;\n");}
        QVERIFY(git({"add","--all"}));QVERIFY(git({"commit","-m","Commit lisible","-m","Description du commit"}));
        MainWindow window;window.show();window.openRepository(repository.path());auto *history=window.findChild<QTreeWidget *>("commitHistory");auto *changed=window.findChild<QTreeWidget *>("changedFiles");QTRY_COMPARE(history->topLevelItemCount(),1);QTRY_VERIFY(changed->isEnabled());
        auto *item=history->topLevelItem(0);history->setCurrentItem(item);emit history->itemClicked(item,0);
        auto *list=window.findChild<QTreeWidget *>("commitFiles");auto *editor=window.findChild<QPlainTextEdit *>("diffEditor");QVERIFY(list&&editor);QTRY_COMPARE(list->topLevelItemCount(),2);QTRY_VERIFY(editor->toPlainText().contains("alpha.cpp"));QVERIFY(!editor->toPlainText().contains("beta.cpp"));
        list->setCurrentItem(list->topLevelItem(1));QTRY_VERIFY(editor->toPlainText().contains("beta.cpp"));QVERIFY(!editor->toPlainText().contains("alpha.cpp"));QVERIFY(window.findChild<QLabel *>("commitInfo")->text().contains("Description du commit"));
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))window.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/commit-browser.png");
        window.findChild<QPushButton *>("workingChanges")->click();QVERIFY(window.findChild<QWidget *>("commitBrowser")->isHidden());
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){
            window.findChild<QDockWidget *>("repositories")->show();QTest::qWait(50);window.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/recent-sidebar.png");
            auto *selector=window.findChild<QToolButton *>("repositorySelector");auto *menu=selector->menu();menu->popup(selector->mapToGlobal(QPoint(0,selector->height())));QTest::qWait(50);menu->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/recent-menu.png");menu->hide();
        }
        const auto removers=window.findChildren<QToolButton *>("removeRecent");QCOMPARE(removers.size(),1);removers.first()->click();QTRY_VERIFY(QSettings().value("recent").toStringList().isEmpty());QVERIFY(QFileInfo::exists(repository.filePath(".git")));QVERIFY(window.windowTitle().contains(QFileInfo(repository.path()).fileName()));
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){
            auto *op=window.findChild<OperationDialog *>();op->begin("Clonage — aperçu de progression");op->step("Réception des objets Git");op->append("Receiving objects: 42% (42/100)\r");op->show();op->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/operation-progress.png");op->finish(true);
        }
        QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void deletedCommitFileAndPatch(){
        QTemporaryDir repository,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(repository.path());p.start("git",args);return p.waitForFinished(5000)&&p.exitCode()==0;};
        QVERIFY(git({"init","--initial-branch=main"}));QVERIFY(git({"config","user.name","Test"}));QVERIFY(git({"config","user.email","test@example.invalid"}));QVERIFY(git({"config","commit.gpgsign","false"}));QFile file(repository.filePath("deleted.txt"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("previous content\n");file.close();QVERIFY(git({"add","--all"}));QVERIFY(git({"commit","-m","initial"}));QVERIFY(file.remove());QVERIFY(git({"add","--all"}));QVERIFY(git({"commit","-m","delete"}));
        MainWindow window;window.show();window.openRepository(repository.path());auto *history=window.findChild<QTreeWidget *>("commitHistory");auto *files=window.findChild<QTreeWidget *>("changedFiles");QTRY_COMPARE(history->topLevelItemCount(),2);QTRY_VERIFY(files->isEnabled());emit history->itemClicked(history->topLevelItem(0),0);
        auto *editor=window.findChild<QPlainTextEdit *>("diffEditor");QTRY_VERIFY(editor->toPlainText().contains("previous content"));QVERIFY(window.findChild<QTreeWidget *>("commitFiles")->topLevelItem(0)->text(0).startsWith("D"));auto *view=window.findChild<QComboBox *>("commitFileView");view->setCurrentIndex(1);emit view->activated(1);QTRY_VERIFY(editor->toPlainText().contains("-previous content"));QTRY_VERIFY(files->isEnabled());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void committedLfsPushReview_data(){QTest::addColumn<bool>("convert");QTest::newRow("cancel")<<false;QTest::newRow("convert-and-send")<<true;}
    void committedLfsPushReview(){
        QFETCH(bool,convert);auto lfs=qEnvironmentVariable("GHE_TEST_LFS");if(lfs.isEmpty())lfs=QStandardPaths::findExecutable("git-lfs");if(lfs.isEmpty())QSKIP("Git LFS indisponible");
        QTemporaryDir data,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());ToolRuntime::setExecutable("git-lfs",lfs);const auto remote=data.filePath("remote.git"),path=data.filePath("local");QDir().mkpath(path);
        auto git=[&](QStringList args){QProcess p;auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",QFileInfo(lfs).absolutePath()+QDir::listSeparator()+env.value("PATH"));p.setProcessEnvironment(env);p.setWorkingDirectory(path);p.start("git",args);p.waitForFinished(10000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        QVERIFY(git({"init","--bare","--initial-branch=main",remote}).ok());QVERIFY(git({"init","--initial-branch=main"}).ok());QVERIFY(git({"config","user.name","LFS UI"}).ok());QVERIFY(git({"config","user.email","ui@example.invalid"}).ok());QVERIFY(git({"config","commit.gpgsign","false"}).ok());QFile small(path+"/README.md");QVERIFY(small.open(QIODevice::WriteOnly));small.write("base\n");small.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","base"}).ok());QVERIFY(git({"remote","add","origin",remote}).ok());QVERIFY(git({"push","-u","origin","main"}).ok());const auto published=git({"rev-parse","HEAD"}).output.trimmed();
        QFile large(path+"/Release.AppImage");QVERIFY(large.open(QIODevice::WriteOnly));QVERIFY(large.resize(51LL*1024*1024));QCOMPARE(large.write("AppImage QA payload\n"),qint64(20));large.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","large unpushed file"}).ok());const auto originalBlob=git({"hash-object","--no-filters","--","Release.AppImage"}).output.trimmed();const auto original=git({"rev-parse","HEAD"}).output.trimmed();
        MainWindow window;window.show();window.openRepository(path);auto *action=window.findChild<QPushButton *>("syncAction");auto *status=window.findChild<QLabel *>("syncStatus");auto *files=window.findChild<QTreeWidget *>("changedFiles");QTRY_VERIFY(status->text().contains("1 commit à envoyer"));QTRY_VERIFY(action->isEnabled());
        bool reviewed=false;QTimer reviewTimer;reviewTimer.setInterval(40);connect(&reviewTimer,&QTimer::timeout,&window,[&]{auto *review=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!review||review->objectName()!="lfsMigrationReview")return;auto *list=review->findChild<QListWidget *>("lfsCommittedFiles");reviewed=list&&list->count()==1&&list->item(0)->text().contains("Release.AppImage");if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){QDir().mkpath(qEnvironmentVariable("GHE_CAPTURE_DIR"));review->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/lfs-migration.png");}reviewTimer.stop();if(convert&&reviewed)review->accept();else review->reject();});reviewTimer.start();QTimer::singleShot(12000,&window,[]{if(auto *d=qobject_cast<QDialog *>(QApplication::activeModalWidget()))d->reject();});action->click();QTRY_VERIFY_WITH_TIMEOUT(reviewed,15000);
        if(convert){QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Synchronisée avec origin/main"),30000);const auto head=git({"rev-parse","HEAD"}).output.trimmed();QVERIFY(head!=original);QCOMPARE(git({"rev-parse","origin/main"}).output.trimmed(),head);QVERIFY(git({"show","HEAD:Release.AppImage"}).output.startsWith("version https://git-lfs.github.com/spec/v1"));QVERIFY(git({"for-each-ref","--format=%(objectname)","refs/github-enhanced/lfs-backups/"}).output.contains(original));}
        else{QTRY_VERIFY(files->isEnabled());QCOMPARE(git({"rev-parse","HEAD"}).output.trimmed(),original);QCOMPARE(git({"rev-parse","origin/main"}).output.trimmed(),published);QVERIFY(!QFileInfo::exists(path+"/.gitattributes"));}
        QCOMPARE(QFileInfo(large.fileName()).size(),51LL*1024*1024);QCOMPARE(git({"hash-object","--no-filters","--","Release.AppImage"}).output.trimmed(),originalBlob);QTRY_VERIFY(files->isEnabled());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void workflowPushRecovery_data() {
        QTest::addColumn<bool>("consent"); QTest::newRow("authorize-and-retry")<<true; QTest::newRow("denied-no-retry")<<false;
    }
    void workflowPushRecovery() {
#ifdef Q_OS_WIN
        QSKIP("Le serveur Git de test utilise un hook POSIX");
#endif
        QFETCH(bool,consent);
        QTemporaryDir data,configuration; QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());
        ToolRuntime::setExecutable("gh",QCoreApplication::applicationDirPath()+"/fake-services");
        TestEnvironment denied("GHE_FAKE_GH_AUTH_DENIED",consent?"0":"1");
        const auto local=data.filePath("local"),remote=data.filePath("remote.git"); QDir().mkpath(local);
        auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(local);p.start("git",args);p.waitForFinished(10000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        QVERIFY(git({"init","--bare","--initial-branch=main",remote}).ok()); QVERIFY(git({"init","--initial-branch=main"}).ok());
        QVERIFY(git({"config","user.name","Workflow Test"}).ok()); QVERIFY(git({"config","user.email","workflow@example.invalid"}).ok()); QVERIFY(git({"config","commit.gpgsign","false"}).ok());
        QFile file(local+"/test.txt"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("base\n"); file.close();
        QVERIFY(git({"add","--all"}).ok()); QVERIFY(git({"commit","-m","base"}).ok()); QVERIFY(git({"remote","add","origin",remote}).ok()); QVERIFY(git({"push","-u","origin","main"}).ok());
        const auto oldHead=git({"rev-parse","HEAD"}).output.trimmed();
        QVERIFY(file.open(QIODevice::Append)); file.write("pending\n"); file.close(); QVERIFY(git({"commit","-am","pending"}).ok());
        QFile hook(remote+"/hooks/pre-receive"); QVERIFY(hook.open(QIODevice::WriteOnly));
        hook.write("#!/bin/sh\necho 'refusing to allow an OAuth App to create or update workflow `.github/workflows/build.yml` without `workflow` scope' >&2\nexit 1\n"); hook.close(); QVERIFY(hook.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        MainWindow window;window.show();window.openRepository(local);
        auto *status=window.findChild<QLabel *>("syncStatus");auto *send=window.findChild<QPushButton *>("syncAction");
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("1 commit à envoyer"),10000);QTRY_VERIFY(send->isEnabled());send->click();
        auto *recovery=window.findChild<QPushButton *>("operationRecovery");QVERIFY(recovery);QTRY_VERIFY_WITH_TIMEOUT(recovery->isVisible(),10000);
        QVERIFY(window.findChild<QLabel *>("operationDetail")->text().contains("autorisation workflow"));
        QCOMPARE(git({"--git-dir",remote,"rev-parse","main"}).output.trimmed(),oldHead);
        QTimer closeAuth;closeAuth.setInterval(20);bool completed=false;
        auto *github=window.findChild<GitHubClient *>(); QVERIFY(github);
        connect(github,&GitHubClient::loginFinished,&window,[&](bool success,const QString &){completed=true;QCOMPARE(success,consent);});
        connect(&closeAuth,&QTimer::timeout,&window,[&]{
            auto *dialog=window.findChild<AccountDialog *>(); if(!dialog||!completed)return;
            if(consent)hook.remove(); dialog->close(); closeAuth.stop();
        });
        closeAuth.start();recovery->click();QVERIFY(completed);
        if(consent){QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Synchronisée avec origin/main"),10000);}
        else {QVERIFY(recovery->isVisible());QCOMPARE(git({"--git-dir",remote,"rev-parse","main"}).output.trimmed(),oldHead);}
        QTRY_VERIFY(send->isEnabled()); QTRY_VERIFY(!github->busy());
        QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void cleanWorkingTreeWithUnpushedCommit(){
        QTemporaryDir repository,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(repository.path());p.start("git",args);return p.waitForFinished(10000)&&p.exitCode()==0;};
        const auto remote=repository.path()+"-remote.git";QVERIFY(git({"init","--bare","--initial-branch=main",remote}));QVERIFY(git({"init","--initial-branch=main"}));QVERIFY(git({"config","user.name","UI Sync"}));QVERIFY(git({"config","user.email","ui@example.invalid"}));QVERIFY(git({"config","commit.gpgsign","false"}));QFile file(repository.filePath("sample.txt"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("base\n");file.close();QVERIFY(git({"add","--all"}));QVERIFY(git({"commit","-m","base"}));QVERIFY(git({"remote","add","origin",remote}));QVERIFY(git({"push","-u","origin","main"}));QVERIFY(file.open(QIODevice::WriteOnly));file.write("local change\n");file.close();QVERIFY(git({"commit","-am","unpushed"}));
        MainWindow window;window.show();window.openRepository(repository.path());auto *status=window.findChild<QLabel *>("syncStatus");auto *action=window.findChild<QPushButton *>("syncAction");auto *changed=window.findChild<QTreeWidget *>("changedFiles");QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("1 commit à envoyer"),10000);QTRY_VERIFY(action->isEnabled());QCOMPARE(changed->topLevelItemCount(),0);QVERIFY(window.findChild<QAction *>("tool-sync")->text().contains("Push origin"));QVERIFY(!window.findChild<QLabel *>("fileTitle")->text().contains("à jour"));
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))window.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/sync-ahead.png");
        QVERIFY(git({"switch","--detach"}));action->click();QTRY_VERIFY(status->text().contains("HEAD détachée"));QVERIFY(git({"switch","main"}));window.findChild<QPushButton *>("verifyRemote")->click();QTRY_VERIFY(status->text().contains("1 commit à envoyer"));QTRY_VERIFY(action->isEnabled());
        action->click();QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Synchronisée avec origin/main"),10000);QTRY_VERIFY(window.findChild<QAction *>("tool-sync")->text().contains("Fetch origin"));QVERIFY(file.open(QIODevice::Append));file.write("not committed yet\n");file.close();window.findChild<QAction *>("tool-refresh")->trigger();QTRY_COMPARE(changed->topLevelItemCount(),1);QTRY_VERIFY(status->text().contains("modifications locales à commiter"));QVERIFY(!status->text().contains("Synchronisée avec"));QTRY_VERIFY(changed->isEnabled());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());QVERIFY(QDir(remote).removeRecursively());
    }
    void adaptiveSyncTracksOtherRemotesAndPulls(){
        QTemporaryDir data,config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());const auto local=data.filePath("local"),remote=data.filePath("remote.git"),peer=data.filePath("peer");QDir().mkpath(local);
        auto git=[&](const QString &cwd,QStringList args){QProcess p;p.setWorkingDirectory(cwd);p.start("git",args);p.waitForFinished(10000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        auto configure=[&](const QString &cwd){return git(cwd,{"config","user.name","Adaptive Sync"}).ok()&&git(cwd,{"config","user.email","sync@example.invalid"}).ok()&&git(cwd,{"config","commit.gpgsign","false"}).ok();};
        QVERIFY(git(data.path(),{"init","--bare","--initial-branch=main",remote}).ok());QVERIFY(git(local,{"init","--initial-branch=main"}).ok());QVERIFY(configure(local));QFile file(local+"/file.txt");QVERIFY(file.open(QIODevice::WriteOnly));file.write("base\n");file.close();QVERIFY(git(local,{"add","--all"}).ok());QVERIFY(git(local,{"commit","-m","base"}).ok());QVERIFY(git(local,{"remote","add","upstream",remote}).ok());QVERIFY(git(local,{"push","upstream","main"}).ok());
        QVERIFY(file.open(QIODevice::Append));file.write("local\n");file.close();QVERIFY(git(local,{"commit","-am","local"}).ok());
        MainWindow window;window.show();window.openRepository(local);auto *action=window.findChild<QAction *>("tool-sync");auto *status=window.findChild<QLabel *>("syncStatus");QTRY_VERIFY_WITH_TIMEOUT(action->text().contains("Push upstream"),10000);QTRY_VERIFY(action->isEnabled());QVERIFY(!git(local,{"config","--get","branch.main.remote"}).ok());action->trigger();QTRY_VERIFY_WITH_TIMEOUT(action->text().contains("Fetch upstream"),10000);QTRY_VERIFY(action->isEnabled());
        QVERIFY(git(data.path(),{"clone",remote,peer}).ok());QVERIFY(configure(peer));QFile incoming(peer+"/from-peer.txt");QVERIFY(incoming.open(QIODevice::WriteOnly));incoming.write("incoming\n");incoming.close();QVERIFY(git(peer,{"add","--all"}).ok());QVERIFY(git(peer,{"commit","-m","incoming"}).ok());QVERIFY(git(peer,{"push"}).ok());action->trigger();QTRY_VERIFY_WITH_TIMEOUT(action->text().contains("Pull upstream"),10000);QTRY_VERIFY(action->isEnabled());QVERIFY(status->text().contains("1 commit à récupérer"));action->trigger();QTRY_VERIFY_WITH_TIMEOUT(action->text().contains("Fetch upstream"),10000);QVERIFY(QFileInfo::exists(local+"/from-peer.txt"));QCOMPARE(git(local,{"rev-parse","HEAD"}).output,git(peer,{"rev-parse","HEAD"}).output);QTRY_VERIFY(action->isEnabled());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void backgroundRefreshKeepsDraftFocus(){
        QTemporaryDir repo,config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());
        auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(repo.path());p.start("git",args);p.waitForFinished(5000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        QVERIFY(git({"init","--initial-branch=main"}).ok());QVERIFY(git({"config","user.name","Focus Test"}).ok());QVERIFY(git({"config","user.email","focus@example.invalid"}).ok());QVERIFY(git({"config","commit.gpgsign","false"}).ok());QFile source(repo.filePath("file.txt"));QVERIFY(source.open(QIODevice::WriteOnly));source.write("base\n");source.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","base"}).ok());
        MainWindow window;window.show();window.activateWindow();window.openRepository(repo.path());auto *summary=window.findChild<QLineEdit *>("commitSummary");auto *description=window.findChild<QPlainTextEdit *>("commitDescription");auto *files=window.findChild<QTreeWidget *>("changedFiles");QTRY_VERIFY(summary->isEnabled());QTRY_VERIFY(files->isEnabled());
        QTimer *poll=nullptr;for(auto *timer:window.findChildren<QTimer *>())if(timer->interval()==2500)poll=timer;QVERIFY(poll);poll->setInterval(100);
        auto *commitDock=window.findChild<QDockWidget *>("commit");auto *diffDock=window.findChild<QDockWidget *>("diff");window.tabifyDockWidget(diffDock,commitDock);commitDock->show();commitDock->raise();
        summary->setText("Un brouillon conservé");summary->setSelection(3,6);summary->setFocus();QTRY_VERIFY(summary->hasFocus());QTest::qWait(650);QVERIFY2(summary->hasFocus(),"La vérification automatique vole le focus du résumé");QCOMPARE(summary->selectedText(),"brouil");
        QVERIFY(source.open(QIODevice::Append));source.write("external change\n");source.close();QTRY_COMPARE_WITH_TIMEOUT(files->topLevelItemCount(),1,5000);QVERIFY(summary->hasFocus());QCOMPARE(summary->text(),"Un brouillon conservé");QCOMPARE(summary->selectedText(),"brouil");QVERIFY(!window.findChild<OperationDialog *>()->isVisible());
        description->setPlainText("Description en cours");auto cursor=description->textCursor();cursor.setPosition(4);cursor.setPosition(11,QTextCursor::KeepAnchor);description->setTextCursor(cursor);description->setFocus();QTRY_VERIFY(description->hasFocus());window.findChild<QAction *>("tool-refresh")->trigger();QTest::qWait(400);QVERIFY(description->hasFocus());QCOMPARE(description->textCursor().selectedText(),"ription");QVERIFY(!commitDock->visibleRegion().isEmpty());
        auto *commands=window.findChild<CommandRunner *>();QSignalSpy writes(commands,&CommandRunner::commandStarted);QTest::qWait(450);QCOMPARE(writes.count(),0);
        bool changedDuringCommit=false,editableDuringCommit=false;connect(commands,&CommandRunner::commandStarted,&window,[&](const QString &,const QStringList &args){if(args.value(0)!="commit")return;editableDuringCommit=summary->isEnabled()&&description->isEnabled();summary->setText("Brouillon suivant");description->setPlainText("Détails suivants");changedDuringCommit=true;});
        QAction *commit=nullptr;for(auto *action:window.findChildren<QAction *>())if(action->text()=="Créer le commit"){commit=action;break;}QVERIFY(commit);QTRY_VERIFY(commit->isEnabled());commit->trigger();QTRY_COMPARE(files->topLevelItemCount(),0);QTRY_VERIFY(mainIdle(window));QVERIFY(changedDuringCommit);QVERIFY(editableDuringCommit);QCOMPARE(summary->text(),"Brouillon suivant");QCOMPARE(description->toPlainText(),"Détails suivants");QCOMPARE(git({"log","-1","--format=%s"}).output.trimmed(),QByteArray("Un brouillon conservé"));QVERIFY(window.close());
    }
    void directLayoutPlacementAndOuterEdges(){
        QTemporaryDir config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());MainWindow window;window.show();window.findChild<QAction *>("tool-layout")->trigger();auto *editor=window.findChild<LayoutDialog *>();auto *panels=editor->findChild<QListWidget *>("layoutPanels");auto *reference=editor->findChild<QComboBox *>("panelRelativeTo");
        for(int i=0;i<panels->count();++i)if(panels->item(i)->data(Qt::UserRole).toString()=="history")panels->setCurrentRow(i);reference->setCurrentIndex(reference->findData("changes"));editor->findChild<QToolButton *>("placePanelSide2")->click();auto *history=window.findChild<QDockWidget *>("history");auto *changes=window.findChild<QDockWidget *>("changes");QVERIFY(window.tabifiedDockWidgets(changes).contains(history));QCOMPARE(reference->currentData().toString(),"changes");
        auto *floating=editor->findChild<QPushButton *>("togglePanelFloating");floating->click();QVERIFY(history->isFloating());floating->click();QVERIFY(!history->isFloating());
        auto *canvas=editor->findChild<LayoutCanvas *>();QTest::qWait(50);auto rect=canvas->panelRect("diff");QVERIFY(!rect.isEmpty());const auto start=rect.center().toPoint(),end=QPoint(canvas->width()/2,38);QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(canvas,end,30);QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,end);auto *diff=window.findChild<QDockWidget *>("diff");QCOMPARE(window.dockWidgetArea(diff),Qt::TopDockWidgetArea);editor->findChild<QPushButton *>("layoutUndo")->click();QCOMPARE(window.dockWidgetArea(diff),Qt::RightDockWidgetArea);
        auto *tabs=editor->findChild<QTabWidget *>();for(auto *button:editor->findChildren<QToolButton *>())if(button->text()=="Dimensions exactes")button->click();auto *width=editor->findChild<QSpinBox *>("panelWidth");qobject_cast<QScrollArea *>(tabs->widget(0))->ensureWidgetVisible(width);editor->activateWindow();auto *widthText=width->findChild<QLineEdit *>();widthText->setFocus();widthText->setText("321 px");widthText->setSelection(1,1);QTRY_VERIFY(widthText->hasFocus());QTest::qWait(450);QCOMPARE(widthText->text(),"321 px");QCOMPARE(widthText->selectedText(),"2");
        tabs->setCurrentIndex(1);auto *search=editor->findChild<QLineEdit *>("layoutCommandSearch");search->setText("push");auto *tools=editor->findChild<QListWidget *>("toolbarItems");int visible=0;for(int i=0;i<tools->count();++i)if(!tools->item(i)->isHidden()){++visible;QVERIFY(tools->item(i)->text().contains("push",Qt::CaseInsensitive));}QCOMPARE(visible,1);search->clear();editor->reject();QTest::qWait(20);QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void layoutRowsFollowFontSize_data(){QTest::addColumn<int>("points");for(int size:{11,16,22})QTest::newRow(qPrintable(QString::number(size)))<<size;}
    void layoutRowsFollowFontSize(){
        QFETCH(int,points);QTemporaryDir config;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());QSettings().setValue("fontSize",points);
        MainWindow window;window.show();window.findChild<QAction *>("tool-layout")->trigger();auto *editor=window.findChild<LayoutDialog *>();QVERIFY(editor);auto *list=editor->findChild<QListWidget *>("toolbarItems");auto *tabs=editor->findChild<QTabWidget *>();tabs->setCurrentIndex(1);QTest::qWait(50);
        for(int i=0;i<list->count();++i){const auto rect=list->visualItemRect(list->item(i));QVERIFY2(rect.height()>=QFontMetrics(list->font()).height()+12,"Une ligne est trop petite pour la police");if(i+1<list->count())QVERIFY(rect.bottom()<list->visualItemRect(list->item(i+1)).top());}
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))editor->grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/layout-commands-"+QString::number(points)+".png");editor->reject();QTest::qWait(20);QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void scrollingSurvivesRefresh(){
        QTemporaryDir repository,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());QProcess git;git.setWorkingDirectory(repository.path());git.start("git",{"init","--initial-branch=main"});QVERIFY(git.waitForFinished(5000)&&git.exitCode()==0);
        QByteArray text;for(int i=0;i<600;++i)text+="line "+QByteArray::number(i)+" with readable content\n";
        for(int i=0;i<60;++i){QFile f(repository.filePath(QString("file-%1.txt").arg(i,3,10,QChar('0'))));QVERIFY(f.open(QIODevice::WriteOnly));f.write(text);}
        MainWindow window;window.show();window.openRepository(repository.path());auto *files=window.findChild<QTreeWidget *>("changedFiles");auto *diff=window.findChild<QPlainTextEdit *>("diffEditor");QTRY_COMPARE(files->topLevelItemCount(),60);QTRY_VERIFY(files->isEnabled());QTRY_VERIFY(diff->verticalScrollBar()->maximum()>100);QVERIFY(qobject_cast<CapsuleScrollBar *>(diff->verticalScrollBar()));
        files->verticalScrollBar()->setValue(200);diff->verticalScrollBar()->setValue(150);const auto fv=files->verticalScrollBar()->value(),dv=diff->verticalScrollBar()->value();const auto original=diff->toPlainText();window.findChild<QAction *>("tool-refresh")->trigger();QTest::qWait(350);QTRY_VERIFY(files->isEnabled());QCOMPARE(files->verticalScrollBar()->value(),fv);QCOMPARE(diff->verticalScrollBar()->value(),dv);QCOMPARE(diff->toPlainText(),original);
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))window.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/capsule-scroll.png");QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
    void generatedCommitIsDraftOnly_data(){QTest::addColumn<QString>("scenario");for(const auto &name:{"normal","large","stale","cancel"})QTest::newRow(name)<<QString(name);}
    void generatedCommitIsDraftOnly(){
        QFETCH(QString,scenario);TestEnvironment mode("GHE_FAKE_CODEX_MODE",(scenario=="normal"||scenario=="large")?QByteArray("normal"):QByteArray("slow"));
        QTemporaryDir configuration,repository;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());
        const auto executable=QCoreApplication::applicationDirPath()+"/fake-services"
#ifdef Q_OS_WIN
            ".exe"
#endif
            ;ToolRuntime::setExecutable("codex",executable);
        auto git=[&](const QStringList &args){QProcess p;p.setWorkingDirectory(repository.path());p.start("git",args);return p.waitForFinished(5000)&&p.exitCode()==0;};QVERIFY(git({"init","--initial-branch=main"}));
        QFile file(repository.path()+"/sample.txt");QVERIFY(file.open(QIODevice::WriteOnly));file.write(scenario=="large"?QByteArray(400000,'x')+"\n":QByteArray("sample\n"));file.close();QVERIFY(git({"add","--","sample.txt"}));
        MainWindow window;window.show();window.openRepository(repository.path());auto *files=window.findChild<QTreeWidget *>("changedFiles");QTRY_COMPARE(files->topLevelItemCount(),1);QTRY_VERIFY(files->isEnabled());
        auto *button=window.findChild<QPushButton *>("generateCommitMessage");auto *summary=window.findChild<QLineEdit *>("commitSummary");auto *status=window.findChild<QLabel *>("generationStatus");auto *codex=window.findChild<CodexClient *>();QVERIFY(button&&summary&&codex&&status);
        summary->setText("Brouillon conservé");QTRY_VERIFY(button->isEnabled());button->click();
        if(scenario=="normal"||scenario=="large")QTRY_COMPARE_WITH_TIMEOUT(summary->text(),"Ajouter le fichier de test",5000);
        else {QTRY_VERIFY(codex->generating());if(scenario=="stale"){QVERIFY(file.open(QIODevice::WriteOnly));file.write("different\n");file.close();QVERIFY(git({"add","--","sample.txt"}));QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("changé"),5000);}else{button->click();QTest::qWait(600);}QCOMPARE(summary->text(),"Brouillon conservé");}
        QVERIFY(!git({"rev-parse","--verify","HEAD"}));QTRY_VERIFY(files->isEnabled());QTRY_VERIFY(mainIdle(window));QVERIFY(window.close());
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
