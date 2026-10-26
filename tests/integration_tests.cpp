#include "codexclient.h"
#include "githubclient.h"
#include "reposettings.h"
#include "repobrowser.h"
#include "repositorycreateform.h"
#include "releasesdialog.h"
#include "theme.h"
#include "toolruntime.h"
#include "repositoryindex.h"
#include "operationdialog.h"
#include "repositorysync.h"
#include "pushpreflight.h"
#include "capsulescrollbar.h"
#include <QtTest>
#include <QtWidgets>

class Env {
    QByteArray key, old;
    bool existed;

  public:
    Env(const char *name, const QByteArray &value)
        : key(name), old(qgetenv(name)), existed(qEnvironmentVariableIsSet(name)) {
        qputenv(name, value);
    }
    ~Env() {
        if (existed)
            qputenv(key, old);
        else
            qunsetenv(key);
    }
};
static QString fixture() {
    return QCoreApplication::applicationDirPath() + "/fake-services"
#ifdef Q_OS_WIN
                                                    ".exe"
#endif
        ;
}
static QList<QJsonObject> records(const QString &path) {
    QFile file(path);
    QList<QJsonObject> result;
    if (file.open(QIODevice::ReadOnly))
        for (const auto &line : file.readAll().split('\n')) {
            auto doc = QJsonDocument::fromJson(line);
            if (doc.isObject())
                result.append(doc.object());
        }
    return result;
}

class IntegrationTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("GitHubEnhancedTests");
        QCoreApplication::setApplicationName("Integration");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QVERIFY(QFileInfo::exists(fixture()));
    }
    void committedLargeFilesAreMigratedWithoutRewritingPublishedHistory(){
        auto lfs=qEnvironmentVariable("GHE_TEST_LFS");if(lfs.isEmpty())lfs=QStandardPaths::findExecutable("git-lfs");if(lfs.isEmpty())QSKIP("Git LFS indisponible");
        QTemporaryDir data,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());ToolRuntime::setExecutable("git-lfs",lfs);const auto remote=data.filePath("remote.git"),local=data.filePath("local");QDir().mkpath(local);
        auto git=[&](QStringList args){QProcess p;auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",QFileInfo(lfs).absolutePath()+QDir::listSeparator()+env.value("PATH"));p.setProcessEnvironment(env);p.setWorkingDirectory(local);p.start("git",args);p.waitForFinished(20000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        QVERIFY(git({"init","--bare","--initial-branch=main",remote}).ok());QVERIFY(git({"init","--initial-branch=main"}).ok());QVERIFY(git({"config","user.name","LFS Migration"}).ok());QVERIFY(git({"config","user.email","lfs@example.invalid"}).ok());QVERIFY(git({"config","commit.gpgsign","false"}).ok());
        QFile source(local+"/source.cpp");QVERIFY(source.open(QIODevice::WriteOnly));source.write("int example = 1;\n");source.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","published"}).ok());QVERIFY(git({"remote","add","origin",remote}).ok());QVERIFY(git({"push","-u","origin","main"}).ok());const auto published=git({"rev-parse","HEAD"}).output.trimmed();
        QFile large(local+"/artifact[1].AppImage");QVERIFY(large.open(QIODevice::WriteOnly));QVERIFY(large.resize(51LL*1024*1024));large.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","raw big artifact"}).ok());const auto rawBlob=git({"rev-parse","HEAD:artifact[1].AppImage"}).output.trimmed();
        // Another outgoing commit deletes the large file: scanning HEAD alone would miss it.
        QVERIFY(large.remove());QVERIFY(source.open(QIODevice::Append));source.write("// retained source\n");source.close();QVERIFY(git({"add","--all"}).ok());QVERIFY(git({"commit","-m","remove old artifact"}).ok());const auto oldHead=git({"rev-parse","HEAD"}).output.trimmed();QVERIFY(git({"branch","other-local-branch"}).ok());
        PushPreflight check;PushInspection reviewed;bool done=false;QString error;check.inspect(local,[&](PushInspection r,QString e){reviewed=r;error=e;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,10000);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(reviewed.largeObjects.size(),1);QCOMPARE(reviewed.largeObjects[0].oid.toLatin1(),rawBlob);
        // Dirty content prevents migration and remains untouched.
        QVERIFY(source.open(QIODevice::Append));source.write("// dirty\n");source.close();QString newHead,backup;done=false;check.convert(reviewed,[&](QString h,QString b,QString e){newHead=h;backup=b;error=e;done=true;});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());QVERIFY(backup.isEmpty());QCOMPARE(git({"rev-parse","HEAD"}).output.trimmed(),oldHead);QVERIFY(git({"restore","--worktree","--","source.cpp"}).ok());
        done=false;check.convert(reviewed,[&](QString h,QString b,QString e){newHead=h;backup=b;error=e;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,20000);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(newHead.toLatin1()!=oldHead);QVERIFY(!backup.isEmpty());QCOMPARE(git({"rev-parse",backup}).output.trimmed(),oldHead);QCOMPARE(git({"rev-parse","other-local-branch"}).output.trimmed(),oldHead);QCOMPARE(git({"rev-parse","origin/main"}).output.trimmed(),published);QVERIFY(git({"merge-base","--is-ancestor",QString::fromLatin1(published),"HEAD"}).ok());QVERIFY(git({"status","--porcelain"}).output.isEmpty());QVERIFY(!QFileInfo::exists(large.fileName()));
        const auto pointer=git({"show","HEAD^:artifact[1].AppImage"}).output;QVERIFY(pointer.startsWith("version https://git-lfs.github.com/spec/v1"));QVERIFY(pointer.contains("size 53477376"));QVERIFY(git({"show","HEAD:source.cpp"}).output.contains("retained source"));
        done=false;check.inspect(local,[&](PushInspection r,QString e){reviewed=r;error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(reviewed.largeObjects.isEmpty());
        const auto pushed=git({"push","origin","main"});QVERIFY2(pushed.ok(),qPrintable(pushed.message()));QCOMPARE(git({"rev-parse","origin/main"}).output.trimmed(),newHead.toLatin1());
    }
    void remoteSynchronizationStates(){
        QTemporaryDir data;const auto remote=data.filePath("remote.git"),local=data.filePath("local"),peer=data.filePath("peer");QDir().mkpath(local);
        auto git=[&](const QString &path,QStringList args){QProcess p;p.setWorkingDirectory(path);p.start("git",args);p.waitForFinished(10000);return CommandResult{p.exitCode(),p.readAllStandardOutput(),p.readAllStandardError()};};
        auto write=[&](QString path,QByteArray text){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(text)==text.size();};
        QVERIFY(git(data.path(),{"init","--bare","--initial-branch=main",remote}).ok());QVERIFY(git(local,{"init","--initial-branch=main"}).ok());
        auto configure=[&](QString path){return git(path,{"config","user.name","Sync Test"}).ok()&&git(path,{"config","user.email","sync@example.invalid"}).ok()&&git(path,{"config","commit.gpgsign","false"}).ok();};QVERIFY(configure(local));
        QVERIFY(write(local+"/base.txt","base\n"));QVERIFY(git(local,{"add","--all"}).ok());QVERIFY(git(local,{"commit","-m","base"}).ok());QVERIFY(git(local,{"remote","add","origin",remote}).ok());QVERIFY(git(local,{"push","-u","origin","main"}).ok());
        RepositorySync sync;QSignalSpy updates(&sync,&RepositorySync::updated);RepositorySyncState state;
        auto inspect=[&](bool fetch){updates.clear();sync.inspect(local,fetch);};
        inspect(false);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"synced");QVERIFY(!state.checkedAt.isValid());
        inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"synced");QVERIFY(state.checkedAt.isValid());QCOMPARE(state.upstream,"origin/main");
        QVERIFY(write(local+"/local.txt","local\n"));QVERIFY(git(local,{"add","--all"}).ok());QVERIFY(git(local,{"commit","-m","local"}).ok());QVERIFY(git(local,{"status","--porcelain"}).output.isEmpty());
        inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"ahead");QCOMPARE(state.ahead,1);QCOMPARE(state.behind,0);
        QVERIFY(git(data.path(),{"clone",remote,peer}).ok());QVERIFY(configure(peer));QVERIFY(write(peer+"/peer.txt","peer\n"));QVERIFY(git(peer,{"add","--all"}).ok());QVERIFY(git(peer,{"commit","-m","peer"}).ok());QVERIFY(git(peer,{"push"}).ok());
        inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"diverged");QCOMPARE(state.ahead,1);QCOMPARE(state.behind,1);
        // Temporary test checkout only: align it to exercise the incoming-only case.
        QVERIFY(git(local,{"reset","--hard","origin/main"}).ok());QVERIFY(write(peer+"/peer.txt","peer updated\n"));QVERIFY(git(peer,{"commit","-am","peer updated"}).ok());QVERIFY(git(peer,{"push"}).ok());
        inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"behind");QCOMPARE(state.behind,1);
        QVERIFY(QDir().rename(remote,remote+".offline"));inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QVERIFY(!state.error.isEmpty());
        inspect(false);QTRY_VERIFY(!updates.isEmpty());QVERIFY(!qvariant_cast<RepositorySyncState>(updates.last()[0]).error.isEmpty());QVERIFY(QDir().rename(remote+".offline",remote));
        QVERIFY(git(local,{"branch","--unset-upstream"}).ok());inspect(true);QTRY_VERIFY(!updates.isEmpty());state=qvariant_cast<RepositorySyncState>(updates.last()[0]);QCOMPARE(state.kind,"noUpstream");QVERIFY(state.error.isEmpty());
        QVERIFY(git(local,{"remote","remove","origin"}).ok());inspect(false);QTRY_VERIFY(!updates.isEmpty());QCOMPARE(qvariant_cast<RepositorySyncState>(updates.last()[0]).kind,"localOnly");
        QVERIFY(git(local,{"switch","--detach"}).ok());inspect(false);QTRY_VERIFY(!updates.isEmpty());QCOMPARE(qvariant_cast<RepositorySyncState>(updates.last()[0]).kind,"detached");
    }
    void capsuleScrollInteraction(){
        Visuals::install();QScrollArea area;area.resize(360,280);auto *content=new QWidget;content->setFixedSize(800,2400);area.setWidget(content);area.show();QTest::qWait(40);
        auto *vertical=qobject_cast<CapsuleScrollBar *>(area.verticalScrollBar());auto *horizontal=qobject_cast<CapsuleScrollBar *>(area.horizontalScrollBar());QVERIFY(vertical&&horizontal);QVERIFY(vertical->maximum()>0);QVERIFY(horizontal->maximum()>0);
        vertical->setValue(0);auto thumb=vertical->thumbRect();QTest::mousePress(vertical,Qt::LeftButton,{},thumb.center().toPoint());QTest::mouseMove(vertical,QPoint(vertical->width()/2,vertical->height()-4));QTest::mouseRelease(vertical,Qt::LeftButton,{},QPoint(vertical->width()/2,vertical->height()-4));QVERIFY(vertical->value()>vertical->maximum()*0.9);
        horizontal->setValue(0);auto h=horizontal->thumbRect();QTest::mousePress(horizontal,Qt::LeftButton,{},h.center().toPoint());QTest::mouseMove(horizontal,QPoint(horizontal->width()-4,horizontal->height()/2));QTest::mouseRelease(horizontal,Qt::LeftButton,{},QPoint(horizontal->width()-4,horizontal->height()/2));QVERIFY(horizontal->value()>horizontal->maximum()*0.9);
        vertical->setValue(0);QWheelEvent wheel(QPointF(20,20),QPointF(area.viewport()->mapToGlobal(QPoint(20,20))),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(area.viewport(),&wheel);QTRY_VERIFY(vertical->value()>0);QTest::qWait(180);const auto before=vertical->value();QTest::keyClick(vertical,Qt::Key_PageDown);QVERIFY(vertical->value()>before);
        Visuals::configure(true,false);vertical->setValue(0);QWheelEvent reduced(QPointF(20,20),QPointF(area.viewport()->mapToGlobal(QPoint(20,20))),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(area.viewport(),&reduced);QVERIFY(vertical->value()>0);Visuals::configure(false,false);
    }
    void largeCommitContextIsAbbreviated() {
        QTemporaryDir data;Env log("GHE_TEST_LOG",(data.path()+"/protocol.jsonl").toUtf8());Env mode("GHE_FAKE_CODEX_MODE","normal");
        CodexClient client(nullptr,fixture());QSignalSpy generated(&client,&CodexClient::messageGenerated),errors(&client,&CodexClient::errorOccurred);
        client.generateCommit(QString(400000,'x'));QTRY_COMPARE_WITH_TIMEOUT(generated.count(),1,5000);QCOMPARE(errors.count(),0);
        bool found=false;for(const auto &r:records(data.path()+"/protocol.jsonl"))if(r["method"]=="turn/start"){
            const auto input=r["params"].toObject()["input"].toArray()[0].toObject()["text"].toString();QVERIFY(input.contains("Contexte abrégé"));QVERIFY(input.toUtf8().size()<140000);found=true;
        }QVERIFY(found);
    }
    void operationProgressAndErrors(){
        OperationDialog dialog;dialog.begin("Transfert du dépôt");dialog.step("Réception");dialog.append("Receiving objects: 42% (42/100)\r");
        auto *bar=dialog.findChild<QProgressBar *>("operationProgress");QVERIFY(bar);QCOMPARE(bar->value(),42);dialog.finish(true);dialog.append("Receiving objects: 78% (78/100)");dialog.finish(false,"Échec du transfert");QVERIFY(dialog.isVisible());
        QTest::qWait(1100);QVERIFY(dialog.isVisible());QVERIFY(dialog.findChild<QLabel *>("operationDetail")->text().contains("Échec"));dialog.hide();
    }
    void automaticIndexPreservesConflictsAndIgnores(){
        QTemporaryDir repository;auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(repository.path());p.start("git",args);p.waitForFinished(5000);return qMakePair(p.exitCode(),p.readAllStandardOutput());};
        QCOMPARE(git({"init","--initial-branch=main"}).first,0);QCOMPARE(git({"config","user.name","Index Test"}).first,0);QCOMPARE(git({"config","user.email","index@example.invalid"}).first,0);QCOMPARE(git({"config","commit.gpgsign","false"}).first,0);
        auto write=[&](QString name,QByteArray text){QFile file(repository.filePath(name));if(!file.open(QIODevice::WriteOnly))return false;return file.write(text)==text.size();};
        QVERIFY(write(".gitignore","ignored.bin\n"));QVERIFY(write("ignored.bin","ignored"));QVERIFY(write("file.txt","base\n"));
        CommandRunner commands;RepositoryIndex index(&commands);bool done=false;QString error;auto prepare=[&]{done=false;index.prepare(repository.path(),[&](QString e){error=e;done=true;});};prepare();QTRY_VERIFY(done);QTRY_VERIFY(!commands.busy());QVERIFY(error.isEmpty());QVERIFY(!git({"ls-files"}).second.contains("ignored.bin"));
        QCOMPARE(git({"commit","-m","base"}).first,0);QCOMPARE(git({"switch","-c","feature"}).first,0);QVERIFY(write("file.txt","feature\n"));QCOMPARE(git({"commit","-am","feature"}).first,0);
        QCOMPARE(git({"switch","main"}).first,0);QVERIFY(write("file.txt","main\n"));QCOMPARE(git({"commit","-am","main"}).first,0);QVERIFY(git({"merge","feature"}).first!=0);
        auto conflict=git({"ls-files","-u"}).second;QVERIFY(!conflict.isEmpty());prepare();QTRY_VERIFY(done);QTRY_VERIFY(!commands.busy());QVERIFY(error.contains("Conflit"));QCOMPARE(git({"ls-files","-u"}).second,conflict);
    }
    void realLfsAndAutomaticIndex(){
        auto lfs=qEnvironmentVariable("GHE_TEST_LFS");if(lfs.isEmpty())lfs=QStandardPaths::findExecutable("git-lfs");if(lfs.isEmpty())QSKIP("Git LFS indisponible sur cette machine");
        QTemporaryDir repository,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());ToolRuntime::setExecutable("git-lfs",lfs);
        auto git=[&](QStringList args){QProcess p;auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",QFileInfo(lfs).absolutePath()+QDir::listSeparator()+env.value("PATH"));p.setProcessEnvironment(env);p.setWorkingDirectory(repository.path());p.start("git",args);p.waitForFinished(15000);return p.exitCode()==0?p.readAllStandardOutput():QByteArray("ERROR: ")+p.readAllStandardError();};
        QVERIFY(!git({"init","--initial-branch=main"}).startsWith("ERROR"));
        QFile huge(repository.filePath("asset[1].bin"));QVERIFY(huge.open(QIODevice::WriteOnly));QVERIFY(huge.resize(51LL*1024*1024));huge.close();
        QFile small(repository.filePath("asset1.bin"));QVERIFY(small.open(QIODevice::WriteOnly));small.write("small");small.close();
        CommandRunner commands;RepositoryIndex index(&commands);bool done=false;QString error;index.prepare(repository.path(),[&](QString e){error=e;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,20000);QVERIFY2(error.isEmpty(),qPrintable(error));QTRY_VERIFY(!commands.busy());
        const auto pointer=git({"show",":asset[1].bin"});QVERIFY(pointer.startsWith("version https://git-lfs.github.com/spec/v1\n"));QVERIFY(pointer.contains("size 53477376"));QCOMPARE(git({"show",":asset1.bin"}),QByteArray("small"));
        QVERIFY(git({"show",":.gitattributes"}).contains("filter=lfs"));QCOMPARE(QFileInfo(huge.fileName()).size(),51LL*1024*1024);
        done=false;index.prepare(repository.path(),[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QTRY_VERIFY(!commands.busy());QVERIFY(error.isEmpty());QCOMPARE(git({"show",":asset[1].bin"}),pointer);
    }
    void codexManagedOAuthAndGeneration() {
        QTemporaryDir data;
        Env log("GHE_TEST_LOG", (data.path() + "/protocol.jsonl").toUtf8());
        Env mode("GHE_FAKE_CODEX_MODE", "normal");
        CodexClient client(nullptr, fixture());
        QSignalSpy account(&client, &CodexClient::accountChanged), login(&client, &CodexClient::loginStarted),
            complete(&client, &CodexClient::loginFinished),
            generated(&client, &CodexClient::messageGenerated), error(&client, &CodexClient::errorOccurred);
        client.checkAccount();
        QTRY_COMPARE(account.count(), 1);
        QCOMPARE(account.first().first().toJsonObject()["type"].toString(), "chatgpt");
        client.login();
        QTRY_COMPARE(complete.count(), 1);
        QCOMPARE(login.count(), 1);
        QVERIFY(complete.first().first().toBool());
        client.generateCommit("diff --git a/test b/test\n+test\n");
        QTRY_COMPARE_WITH_TIMEOUT(generated.count(), 1, 5000);
        QCOMPARE(generated.first()[0].toString(), "Ajouter le fichier de test");
        QCOMPARE(error.count(), 0);
        bool isolated = false, structured = false;
        for (const auto &record : records(data.path() + "/protocol.jsonl")) {
            if (record["method"].toString() == "thread/start") {
                auto p = record["params"].toObject();
                QVERIFY(p["ephemeral"].toBool());
                QCOMPARE(p["sandbox"].toString(), "read-only");
                QCOMPARE(p["approvalPolicy"].toString(), "never");
                QVERIFY(p["config"].toObject()["mcp_servers"].toObject().isEmpty());
                QVERIFY(p["cwd"].toString() != QDir::currentPath());
                isolated = true;
            }
            if (record["method"].toString() == "turn/start") {
                auto p = record["params"].toObject();
                QVERIFY(p["outputSchema"].isObject());
                QVERIFY(!p["sandboxPolicy"].toObject()["networkAccess"].toBool());
                structured = true;
            }
        }
        QVERIFY(isolated && structured);
    }
    void codexRejectsToolsAndMalformedResults_data() {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : {"tool", "malformed", "error"})
            QTest::newRow(mode) << QString(mode);
    }
    void codexRejectsToolsAndMalformedResults() {
        QFETCH(QString, mode);
        Env option("GHE_FAKE_CODEX_MODE", mode.toUtf8());
        CodexClient client(nullptr, fixture());
        QSignalSpy errors(&client, &CodexClient::errorOccurred),
            generated(&client, &CodexClient::messageGenerated);
        client.generateCommit("+data");
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);
        QCOMPARE(generated.count(), 0);
        QVERIFY(!client.generating());
    }
    void codexCancelAndNoApiKeyFallback() {
        {
            Env mode("GHE_FAKE_CODEX_MODE", "pending_auth");
            CodexClient client(nullptr, fixture());
            QSignalSpy started(&client, &CodexClient::loginStarted),
                finished(&client, &CodexClient::loginFinished);
            client.login(true);
            QTRY_COMPARE(started.count(), 1);
            QCOMPARE(started.first()[1].toString(), "ABCD-EFGH");
            client.cancelLogin();
            QTest::qWait(80);
            QCOMPARE(finished.count(), 0);
        }
        {
            Env account("GHE_FAKE_CODEX_ACCOUNT", "apiKey");
            CodexClient client(nullptr, fixture());
            QSignalSpy errors(&client, &CodexClient::errorOccurred),
                generated(&client, &CodexClient::messageGenerated);
            client.generateCommit("+data");
            QTRY_VERIFY(!errors.isEmpty());
            QCOMPARE(generated.count(), 0);
        }
        {
            Env mode("GHE_FAKE_CODEX_MODE", "slow");
            CodexClient client(nullptr, fixture());
            QSignalSpy generated(&client, &CodexClient::messageGenerated);
            client.generateCommit("+data");
            QTest::qWait(100);
            client.cancelGeneration();
            QTest::qWait(550);
            QCOMPARE(generated.count(), 0);
        }
    }
    void newRepositoryButtonShowsFormAndPreservesErrors(){
        QTemporaryDir data,configuration;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configuration.path());Env log("GHE_TEST_LOG",(data.path()+"/requests.jsonl").toUtf8());Env creationError("GHE_FAKE_CREATE_ERROR","1");Theme::apply(Theme::read(QFINDTESTDATA("../themes/obsidian.json")),false,11);
        GitHubClient client(nullptr,fixture());RepoBrowser browser(&client);browser.show();auto *list=browser.findChild<QTreeWidget *>("githubRepositories");QTRY_COMPARE(list->topLevelItemCount(),100);auto *button=browser.findChild<QPushButton *>("newGithubRepository");QVERIFY(button);QTest::mouseClick(button,Qt::LeftButton);auto *form=browser.findChild<RepositoryCreateForm *>();QVERIFY(form);QTRY_VERIFY(form->isVisible());auto *name=form->findChild<QLineEdit *>("newRepositoryName");auto *submit=form->findChild<QPushButton *>("createRepositorySubmit");name->setText("new-project");QTRY_VERIFY(submit->isEnabled());
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR")){QDir().mkpath(qEnvironmentVariable("GHE_CAPTURE_DIR"));browser.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/new-repository.png");}
        submit->click();auto *status=form->findChild<QLabel *>("repositoryCreationStatus");QTRY_VERIFY(status->text().contains("existe déjà"));QVERIFY(form->isVisible());QCOMPARE(name->text(),"new-project");qputenv("GHE_FAKE_CREATE_ERROR","0");submit->click();QTRY_VERIFY(form->isHidden());QCOMPARE(list->topLevelItem(0)->text(0),"octocat/new-project");QVERIFY(browser.findChild<QLabel *>("repositoryBrowserStatus")->text().contains("Dépôt créé"));
        bool initialized=false;for(const auto &r:records(data.path()+"/requests.jsonl"))if(r.contains("payload"))initialized|=r["payload"].toObject()["auto_init"].toBool();QVERIFY(initialized);browser.reject();
    }
    void releaseRepositoryParsingAndIsolation(){
        QCOMPARE(GitHubClient::repositoryFromRemote("git@github.com:octocat/project.git"),"octocat/project");QCOMPARE(GitHubClient::repositoryFromRemote("ssh://git@github.com/octocat/project.git"),"octocat/project");QCOMPARE(GitHubClient::repositoryFromRemote("https://github.com/octocat/project.git"),"octocat/project");QVERIFY(GitHubClient::repositoryFromRemote("https://example.com/octocat/project.git").isEmpty());QVERIFY(GitHubClient::repositoryFromRemote("https://github.com/octocat/../wrong").isEmpty());
        QTemporaryDir data;Env delay("GHE_FAKE_API_DELAY","50");auto git=[&](QStringList args){QProcess p;p.setWorkingDirectory(data.path());p.start("git",args);return p.waitForFinished(5000)&&p.exitCode()==0;};QVERIFY(git({"init"}));QVERIFY(git({"remote","add","origin","git@github.com:octocat/alpha.git"}));QVERIFY(git({"remote","add","upstream","https://github.com/another/beta.git"}));GitHubClient client(nullptr,fixture());ReleasesDialog dialog(data.path(),&client);dialog.show();auto *remotes=dialog.findChild<QComboBox *>("releaseRemote");QTRY_COMPARE(remotes->count(),2);remotes->setCurrentIndex(1);emit remotes->activated(1);auto *label=dialog.findChild<QLabel *>("releaseRepository");auto *notes=dialog.findChild<QPlainTextEdit *>("releaseBody");QTRY_COMPARE(label->text(),"another/beta");QTRY_VERIFY(notes->toPlainText().contains("another/beta"));QVERIFY(!notes->toPlainText().contains("octocat/alpha"));dialog.reject();
    }
    void releaseCreateUploadRetryAndPublish(){
        QTemporaryDir data;Env log("GHE_TEST_LOG",(data.path()+"/requests.jsonl").toUtf8());Env state("GHE_FAKE_SERVICE_STORE",(data.path()+"/state.json").toUtf8());Env uploadError("GHE_FAKE_UPLOAD_ERROR","1");GitHubClient client(nullptr,fixture());ReleaseEditor editor(&client,"octocat/project","main");editor.show();editor.findChild<QLineEdit *>("releaseTag")->setText("v2.0.0");editor.findChild<QLineEdit *>("releaseTitle")->setText("Version two");editor.findChild<QPlainTextEdit *>("releaseNotes")->setPlainText("Release notes");editor.findChild<QCheckBox *>("releaseDraft")->setChecked(false);QFile file(data.filePath("demo #1.AppImage"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("binary asset fixture");file.close();auto *assets=editor.findChild<QListWidget *>("releaseAttachments");auto *item=new QListWidgetItem("demo #1.AppImage",assets);item->setData(Qt::UserRole,file.fileName());QSignalSpy saved(&editor,&ReleaseEditor::saved);auto *submit=editor.findChild<QPushButton *>("saveRelease");submit->click();auto *status=editor.findChild<QLabel *>("releaseEditorStatus");QTRY_VERIFY(status->text().contains("Le brouillon existe"));QVERIFY(editor.isVisible());QVERIFY(submit->isEnabled());qputenv("GHE_FAKE_UPLOAD_ERROR","0");submit->click();QTRY_COMPARE(saved.count(),1);QVERIFY(!saved[0][0].toJsonObject()["draft"].toBool());
        int creates=0,uploads=0;bool published=false,correctName=false;for(const auto &r:records(data.path()+"/requests.jsonl")){auto args=r["arguments"].toArray();if(args.contains("repos/octocat/project/releases")&&args.contains("POST"))++creates;if(r.contains("uploadFile")){++uploads;correctName|=r["uploadName"].toString()=="demo #1.AppImage"&&r["uploadFile"].toString()==file.fileName();}if(r.contains("payload")&&r["payload"].toObject().size()==1&&r["payload"].toObject().contains("draft"))published|=!r["payload"].toObject()["draft"].toBool();}QCOMPARE(creates,1);QCOMPARE(uploads,2);QVERIFY(published&&correctName);
    }
    void releaseManagementAndReadOnly(){
        QTemporaryDir data;Env state("GHE_FAKE_SERVICE_STORE",(data.path()+"/state.json").toUtf8());GitHubClient client(nullptr,fixture());ReleasesDialog dialog({},&client,nullptr,"octocat/project");dialog.show();auto *list=dialog.findChild<QTreeWidget *>("repositoryReleases");auto *assets=dialog.findChild<QTreeWidget *>("releaseAssets");QTRY_COMPARE(list->topLevelItemCount(),2);QTRY_COMPARE(assets->topLevelItemCount(),1);QTRY_VERIFY(dialog.findChild<QPushButton *>("newRelease")->isEnabled());
        if(!qEnvironmentVariableIsEmpty("GHE_CAPTURE_DIR"))dialog.grab().save(qEnvironmentVariable("GHE_CAPTURE_DIR")+"/releases.png");
        QTimer::singleShot(40,&dialog,[]{auto *form=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!form)return;auto *name=form->findChild<QLineEdit *>("releaseTitle");auto *submit=form->findChild<QPushButton *>("saveRelease");if(name&&submit){name->setText("Updated title");submit->click();}else form->reject();});dialog.findChild<QPushButton *>("editRelease")->click();QTRY_COMPARE(list->topLevelItem(0)->text(0),"Updated title");
        list->setCurrentItem(list->topLevelItem(1));auto *publish=dialog.findChild<QPushButton *>("publishRelease");QVERIFY(publish->isEnabled());publish->click();QTRY_COMPARE(list->topLevelItem(1)->text(1),"Préversion");list->setCurrentItem(list->topLevelItem(0));QTRY_COMPARE(assets->topLevelItemCount(),1);
        QTimer::singleShot(40,&dialog,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});dialog.findChild<QPushButton *>("deleteReleaseAsset")->click();QTRY_COMPARE(assets->topLevelItemCount(),0);
        QTimer::singleShot(40,&dialog,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});dialog.findChild<QPushButton *>("deleteRelease")->click();QTRY_COMPARE(list->topLevelItemCount(),1);dialog.reject();
        Env readOnly("GHE_FAKE_READ_ONLY","1");ReleasesDialog readonly({},&client,nullptr,"octocat/read-only");readonly.show();QTRY_COMPARE(readonly.findChild<QTreeWidget *>("repositoryReleases")->topLevelItemCount(),2);QVERIFY(!readonly.findChild<QPushButton *>("newRelease")->isEnabled());QVERIFY(!readonly.findChild<QPushButton *>("deleteRelease")->isEnabled());readonly.reject();
    }
    void githubOAuthReposAndCreation() {
        QTemporaryDir data;
        Env log("GHE_TEST_LOG", (data.path() + "/github.jsonl").toUtf8());
        Env token("GH_TOKEN", "must-not-leak");
        GitHubClient client(nullptr, fixture());
        QSignalSpy account(&client, &GitHubClient::accountChanged),
            login(&client, &GitHubClient::loginFinished), repos(&client, &GitHubClient::repositoriesLoaded);
        client.login();
        QTRY_COMPARE(login.count(), 1);
        QVERIFY(login.first()[0].toBool());
        QTRY_VERIFY(!account.isEmpty());
        QCOMPARE(client.account()["login"].toString(), "octocat");
        client.repositories();
        QTRY_COMPARE(repos.count(), 1);
        QCOMPARE(repos.first()[0].toJsonArray().size(), 100);
        QVERIFY(repos.first()[2].toBool());
        client.repositories(2);
        QTRY_COMPARE(repos.count(), 2);
        QCOMPARE(repos.last()[0].toJsonArray().size(), 1);
        QVERIFY(!repos.last()[2].toBool());
        bool created = false;
        client.createRepository("created", "Description", true, [&](QJsonValue value, QString error) {
            created = error.isEmpty() && value.toObject()["clone_url"].toString().endsWith(".git");
        });
        QTRY_VERIFY(created);
        bool helper = false, payload = false;
        for (const auto &record : records(data.path() + "/github.jsonl")) {
            if (record.contains("arguments")) {
                QVERIFY(!record["tokenPresent"].toBool());
                if (record["arguments"].toArray().contains("setup-git"))
                    helper = true;
            }
            if (record.contains("payload")) {
                QVERIFY(record["payload"].toObject()["private"].toBool());
                payload = true;
            }
        }
        QVERIFY(helper && payload);
        bool rejected = false;
        client.createRepository("invalid/name", {}, true,
                                [&](QJsonValue, QString error) { rejected = !error.isEmpty(); });
        QVERIFY(rejected);
    }
    void repoBrowserAndLocalSettings() {
        QTemporaryDir configuration;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configuration.path());
        GitHubClient client(nullptr, fixture());
        RepoBrowser browser(&client);
        browser.show();
        auto *list = browser.findChild<QTreeWidget *>("githubRepositories");
        QTRY_COMPARE(list->topLevelItemCount(), 100);
        auto *search = browser.findChild<QLineEdit *>("repositorySearch");
        search->setText("project-42");
        QCOMPARE(list->topLevelItemCount(), 1);
        browser.reject();
        QTemporaryDir repository;
        auto git = [&](const QStringList &args) {
            QProcess p;
            p.setWorkingDirectory(repository.path());
            p.start("git", args);
            if (!p.waitForFinished(5000) || p.exitCode() != 0)
                return QByteArray();
            return p.readAllStandardOutput();
        };
        git({"init", "--initial-branch=main"});
        QVERIFY(QDir(repository.path() + "/.git").exists());
        QFile original(repository.path() + "/.gitignore");
        QVERIFY(original.open(QIODevice::WriteOnly));
        original.write("# custom\nsecret.txt\n");
        original.close();
        RepoSettings settings(repository.path());
        settings.show();
        auto *box = settings.findChild<QDialogButtonBox *>();
        QTRY_VERIFY(box->button(QDialogButtonBox::Save)->isEnabled());
        settings.findChild<QLineEdit *>("gitUserName")->setText("Integration User");
        settings.findChild<QLineEdit *>("gitUserEmail")->setText("integration@example.invalid");
        settings.findChild<QLineEdit *>("gitOrigin")->setText("https://github.com/octocat/test.git");
        for (auto *button : settings.findChildren<QPushButton *>())
            if (button->text() == "Node.js") {
                button->click();
                button->click();
            }
        box->button(QDialogButtonBox::Save)->click();
        QTRY_VERIFY(!settings.isVisible());
        QCOMPARE(git({"config", "--local", "--get", "user.name"}).trimmed(), "Integration User");
        QCOMPARE(git({"remote", "get-url", "origin"}).trimmed(), "https://github.com/octocat/test.git");
        QVERIFY(original.open(QIODevice::ReadOnly));
        auto text = original.readAll();
        original.close();
        QVERIFY(text.startsWith("# custom\nsecret.txt\n"));
        QCOMPARE(text.count("node_modules/"), 1);
        RepoSettings conflict(repository.path());
        conflict.show();
        auto *conflictBox = conflict.findChild<QDialogButtonBox *>();
        QTRY_VERIFY(conflictBox->button(QDialogButtonBox::Save)->isEnabled());
        QVERIFY(original.open(QIODevice::WriteOnly));
        original.write("# external edit\n");
        original.close();
        conflict.findChild<QPlainTextEdit *>("gitIgnoreEditor")->setPlainText("overwrite");
        conflictBox->button(QDialogButtonBox::Save)->click();
        QVERIFY(conflict.isVisible());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), "# external edit\n");
        conflict.reject();
    }
};
QTEST_MAIN(IntegrationTests)
#include "integration_tests.moc"
