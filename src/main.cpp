#include "mainwindow.h"
#include "appversion.h"
#include "visuals.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QAction>
#include <QIcon>
#include <QSettings>
#include <QSslSocket>
#include <QTextStream>

int main(int argc, char *argv[]) {
#ifndef Q_OS_MACOS
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif
    QApplication app(argc, argv);
    Visuals::install();
    app.setOrganizationName("GitHubEnhanced"); app.setOrganizationDomain("githubenhanced.io");
    app.setApplicationName("GitHub Enhanced"); app.setApplicationVersion(GHE_APP_VERSION);
    app.setDesktopFileName("io.githubenhanced.GitHubEnhanced");
    QCommandLineParser parser; parser.setApplicationDescription("Client Git natif et personnalisable");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("repository", "Dépôt local à ouvrir", "[repository]");
    parser.addOption({"smoke-test", "Démarrer puis quitter après 2 secondes (validation de démarrage)."});
    parser.addOption({"screenshot", "Enregistrer une capture de la fenêtre après son démarrage.", "file"});
    parser.addOption({"layout-editor", "Ouvrir l’éditeur de disposition au démarrage."});
    parser.addOption({"preferences", "Ouvrir les réglages au démarrage."});
    parser.process(app);
    if (parser.isSet("smoke-test") && !QSslSocket::supportsSsl()) {
        QTextStream(stderr) << "Le composant HTTPS/TLS de l’application est indisponible.\n";
        return 1;
    }
    app.setWindowIcon(QIcon(":/packaging/icons/github-enhanced.png"));
    MainWindow window; window.show();
    if (parser.isSet("layout-editor")) QTimer::singleShot(180, &window, [&window] {
        if (auto *action = window.findChild<QAction *>("tool-layout")) action->trigger();
    });
    if (parser.isSet("preferences")) QTimer::singleShot(150, &window, [&window] {
        if (auto *action = window.findChild<QAction *>("preferencesAction")) action->trigger();
    });
    if (!parser.positionalArguments().isEmpty()) QTimer::singleShot(0, &window, [&] { window.openRepository(parser.positionalArguments().first()); });
    if (parser.isSet("screenshot")) QTimer::singleShot(1500, &window, [&] { window.grab().save(parser.value("screenshot")); });
    if(!parser.isSet("smoke-test") && QSettings().value("updates/checkAtLaunch",true).toBool())QTimer::singleShot(1500,&window,[&window]{window.checkUpdates(false);});
    if (parser.isSet("smoke-test")) QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    return app.exec();
}
