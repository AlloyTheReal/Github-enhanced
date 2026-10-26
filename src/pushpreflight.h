#pragma once
#include "commandrunner.h"
#include "toolruntime.h"
#include <QHash>
#include <QMap>
struct LargeGitObject {
  QString oid, path;
  qint64 bytes = 0;
};
struct PushInspection {
  QString repository, branchRef, head;
  QStringList remoteRefs;
  QList<LargeGitObject> largeObjects;
};
class PushPreflight : public QObject {
  Q_OBJECT
public:
  using Checked = std::function<void(PushInspection, QString)>;
  using Converted =
      std::function<void(QString newHead, QString backupRef, QString error)>;
  explicit PushPreflight(QObject *parent = nullptr);
  void inspect(const QString &repository, Checked callback);
  void convert(const PushInspection &reviewed, Converted callback);
  bool busy() const { return active; }
signals:
  void detail(const QString &message);
  void output(const QString &text);
  void busyChanged(bool busy);

private:
  CommandRunner runner;
  RuntimeInstaller installer;
  PushInspection inspection;
  Checked checked;
  Converted converted;
  QString backup;
  QMap<QString, QString> originalRefs;
  QHash<QString, QString> rewritten;
  QList<QStringList> restoreJobs;
  QString restorationError;
  QString migrationError;
  std::unique_ptr<QTemporaryDir> recoveryDirectory;
  bool active = false;
  void collectRefs();
  void scan();
  void finishInspection(const QString &error = {});
  void startConversion();
  void migrate();
  void performMigration();
  void snapshotReferences();
  void restoreReferences();
  void restoreNext();
  void hydrateAndVerify();
  void finishConversion(const QString &newHead, const QString &error = {});
};
