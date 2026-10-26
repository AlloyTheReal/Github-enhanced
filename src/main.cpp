#include "mainwindow.h"
#include "visuals.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QAction>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    Visuals::install();
    app.setOrganizationName("GitHubEnhanced"); app.setOrganizationDomain("githubenhanced.io");
    app.setApplicationName("GitHub Enhanced"); app.setApplicationVersion("0.1.0");
    QCommandLineParser parser; parser.setApplicationDescription("Client Git natif et personnalisable");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("repository", "Dépôt local à ouvrir", "[repository]");
    parser.addOption({"smoke-test", "Démarrer puis quitter après 2 secondes (validation de démarrage)."});
    parser.addOption({"screenshot", "Enregistrer une capture de la fenêtre après son démarrage.", "file"});
    parser.addOption({"layout-editor", "Ouvrir l’éditeur de disposition au démarrage."});
    parser.addOption({"preferences", "Ouvrir les réglages au démarrage."});
    parser.process(app);
    MainWindow window; window.show();
    if (parser.isSet("layout-editor")) QTimer::singleShot(180, &window, [&window] {
        if (auto *action = window.findChild<QAction *>("tool-layout")) action->trigger();
    });
    if (parser.isSet("preferences")) QTimer::singleShot(150, &window, [&window] {
        if (auto *action = window.findChild<QAction *>("preferencesAction")) action->trigger();
    });
    if (!parser.positionalArguments().isEmpty()) QTimer::singleShot(0, &window, [&] { window.openRepository(parser.positionalArguments().first()); });
    if (parser.isSet("screenshot")) QTimer::singleShot(1500, &window, [&] { window.grab().save(parser.value("screenshot")); });
    if (parser.isSet("smoke-test")) QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    return app.exec();
}
