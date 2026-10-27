#pragma once
#include <QJsonObject>
#include <QObject>
#include <QUrl>
class QNetworkAccessManager;
struct AppUpdate {
  QString version, notes, assetName, sha256;
  QUrl releaseUrl, assetUrl;
  qint64 bytes = 0;
  bool available() const { return !version.isEmpty(); }
};
Q_DECLARE_METATYPE(AppUpdate)
class AppUpdater : public QObject {
  Q_OBJECT
public:
  explicit AppUpdater(QObject *parent = nullptr,
                      QNetworkAccessManager *transport = nullptr);
  void check(bool manual = false);
  void download();
  bool busy() const { return active; }
  AppUpdate available() const { return update; }
  QString downloadedFile() const { return staged; }
  static bool newer(const QString &candidate, const QString &current);
  static AppUpdate select(const QJsonObject &release, const QString &current,
                          const QString &platform, const QString &architecture,
                          const QString &repository, QString *error = nullptr);
  static QString platform();
  static QString repository();
  static bool replaceAppImage(const QString &target, const QString &source,
                              const QString &sha256, QString *error = nullptr);
signals:
  void checked(const AppUpdate &update, const QString &error, bool manual);
  void progress(qint64 received, qint64 total);
  void downloadFinished(const QString &file, const QString &error);

private:
  QNetworkAccessManager *network;
  AppUpdate update;
  QString staged;
  bool active = false;
};
