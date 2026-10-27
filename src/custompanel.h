#pragma once
#include "repositorysync.h"
#include <QDockWidget>
#include <QJsonObject>
class QPlainTextEdit;
class QListWidget;
class QLabel;
class CustomPanel : public QDockWidget {
  Q_OBJECT
public:
  explicit CustomPanel(const QJsonObject &definition,
                       QWidget *parent = nullptr);
  QJsonObject definition() const;
  void updateRepository(const RepositorySyncState &state);
  static bool validate(const QJsonArray &definitions);
  static QJsonObject edit(QWidget *parent, const QJsonObject &initial = {});
signals:
  void contentChanged();

private:
  QJsonObject config;
  QPlainTextEdit *notes = nullptr;
  QListWidget *tasks = nullptr;
  QLabel *repositoryInfo = nullptr;
};
