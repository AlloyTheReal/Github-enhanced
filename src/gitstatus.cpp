#include "gitstatus.h"

QList<FileChange> parseStatus(const QByteArray &porcelain) {
    const auto records = porcelain.split('\0');
    QList<FileChange> changes;
    for (qsizetype i = 0; i < records.size(); ++i) {
        const auto &record = records[i];
        if (record.size() < 4) continue;
        FileChange change{QString::fromUtf8(record.mid(3)), {}, QString::fromLatin1(record.left(2))};
        // In porcelain v1 -z, renames are destination NUL source NUL.
        if (change.status.contains('R') || change.status.contains('C')) {
            if (i + 1 < records.size()) change.originalPath = QString::fromUtf8(records[++i]);
        }
        changes.append(change);
    }
    return changes;
}
