#pragma once
#include "commandrunner.h"
#include "toolruntime.h"
class RepositoryIndex : public QObject {
  Q_OBJECT
public:
  explicit RepositoryIndex(CommandRunner *commands, QObject *parent = nullptr);
  void prepare(const QString &repository,
               std::function<void(QString)> callback);
  bool busy() const { return active; }
signals:
  void detail(const QString &message);

private:
  CommandRunner *runner;
  RuntimeInstaller installer;
  bool active = false;
  QString path;
  QStringList largeFiles;
  QStringList stagedCandidates;
  bool needsAdd = false;
  std::function<void(QString)> done;
  void inspectStaged();
  void afterInspection();
  void configureLfs();
  void trackNext();
  void stageAll();
  void finish(const QString &error = {});
};
