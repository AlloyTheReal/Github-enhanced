#pragma once
#include "commandrunner.h"
#include <QDateTime>
#include <QHash>

struct RepositorySyncState {
  QString repository, branch, head, upstream, remote, remoteRef, error;
  QString kind = "unknown";
  int ahead = 0, behind = 0;
  QDateTime checkedAt;
};
Q_DECLARE_METATYPE(RepositorySyncState)

class RepositorySync : public QObject {
  Q_OBJECT
public:
  explicit RepositorySync(QObject *parent = nullptr);
  void inspect(const QString &repository, bool fetchRemote = false);
  bool busy() const { return active; }
signals:
  void updated(const RepositorySyncState &state);
  void busyChanged(bool busy);

private:
  CommandRunner runner;
  RepositorySyncState state;
  QHash<QString, QDateTime> verified;
  QHash<QString, QString> failures;
  QString pendingPath;
  bool pendingFetch = false, active = false;
  void findRemote(bool fetch);
  void compare();
  void finish();
  QString key() const { return state.repository + '\n' + state.remote; }
};
