#include "desktopidentity.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
QString appImageDesktopIdentity(const QString &image,const QStringList &dataDirectories){
    if(image.isEmpty())return "io.githubenhanced.GitHubEnhanced";
    QString fallback;
    for(const auto &directory:dataDirectories){
        const QDir applications(directory+"/applications");
        for(const auto &name:applications.entryList({"*.desktop"},QDir::Files)){
            QFile file(applications.filePath(name));if(!file.open(QIODevice::ReadOnly)||file.size()>65536)continue;
            const auto content=QString::fromUtf8(file.readAll());bool desktopEntry=false;
            for(const auto &line:content.split('\n')){
                if(line.startsWith('[')){desktopEntry=line.trimmed()=="[Desktop Entry]";continue;}
                if(!desktopEntry||!line.startsWith("Exec="))continue;
                auto command=line.mid(5);command.replace("\\\\","\\").replace("\\s"," ").replace("%%","%");
                const auto identity=QFileInfo(name).completeBaseName();
                if(command.contains(image))return identity;
                if(command.contains(QFileInfo(image).fileName())&&fallback.isEmpty())fallback=identity;
            }
        }
    }
    return fallback.isEmpty()?QString("io.githubenhanced.GitHubEnhanced"):fallback;
}
