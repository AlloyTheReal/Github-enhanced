#pragma once
#include <QJsonObject>
#include <QString>
#include <QList>

class Theme {
public:
    static bool validate(const QJsonObject &object, QString *error = nullptr);
    static QJsonObject read(const QString &path);
    static QList<QJsonObject> installed();
    static QString directory();
    static bool install(const QJsonObject &object, QString *error = nullptr);
    static void apply(const QJsonObject &object, bool native = false, int fontSize = 13);
};
