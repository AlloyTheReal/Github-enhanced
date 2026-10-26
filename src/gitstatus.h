#pragma once
#include <QString>
#include <QList>

struct FileChange {
    QString path;
    QString originalPath;
    QString status;
    bool operator==(const FileChange &) const = default;
    bool staged() const { return status.size() == 2 && status[0] != ' ' && status[0] != '?'; }
    bool untracked() const { return status == "??"; }
};
QList<FileChange> parseStatus(const QByteArray &porcelain);
