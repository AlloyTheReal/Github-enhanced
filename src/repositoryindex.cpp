#include "repositoryindex.h"
#include "gitstatus.h"
#include <QtCore>
RepositoryIndex::RepositoryIndex(CommandRunner *commands, QObject *parent)
    : QObject(parent), runner(commands), installer(this) {
  connect(&installer, &RuntimeInstaller::progress, this,
          &RepositoryIndex::detail);
  connect(&installer, &RuntimeInstaller::finished, this,
          [this](const QString &, const QString &error) {
            if (error.isEmpty())
              configureLfs();
            else
              finish("Git LFS : " + error);
          });
}
void RepositoryIndex::prepare(const QString &repository,
                              std::function<void(QString)> callback) {
  if (active) {
    callback("Préparation du dépôt déjà en cours.");
    return;
  }
  active = true;
  path = repository;
  preparingExcluded=excludedFiles;
  done = std::move(callback);
  largeFiles.clear();
  stagedCandidates.clear();
  includedPaths.clear();
  needsAdd = false;
  runner->run("git",
              {"status", "--porcelain=v1", "-z", "--untracked-files=all"}, path,
              [this](const CommandResult &result) {
                if (!result.ok()) {
                  finish(result.message());
                  return;
                }
                const auto changes = parseStatus(result.output);
                if (changes.isEmpty()) {
                  finish();
                  return;
                }
                for (const auto &change : changes) {
                  if(preparingExcluded.contains(change.path)){if(!change.originalPath.isEmpty())preparingExcluded.insert(change.originalPath);continue;}
                  includedPaths.append(change.path);
                  if(!change.originalPath.isEmpty())includedPaths.append(change.originalPath);
                  if (change.status.contains('U') || change.status == "AA" ||
                      change.status == "DD") {
                    finish("Conflit à résoudre : " + change.path +
                           ". La préparation automatique est suspendue.");
                    return;
                  }
                  if (change.status.size() == 2 && change.status[1] != ' ')
                    needsAdd = true;
                  QFileInfo file(QDir(path).filePath(change.path));
                  if (!file.isSymLink() && file.isFile() &&
                      file.size() >= 50LL * 1024 * 1024 &&
                      change.path != ".gitattributes") {
                    if (change.status[1] != ' ')
                      largeFiles.append(change.path);
                    else
                      stagedCandidates.append(change.path);
                  }
                }
                inspectStaged();
              });
}
void RepositoryIndex::inspectStaged() {
  if (stagedCandidates.isEmpty()) {
    afterInspection();
    return;
  }
  const auto file = stagedCandidates.takeFirst();
  runner->run("git", {"cat-file", "-s", ":" + file}, path,
              [this, file](const CommandResult &r) {
                if (!r.ok()) {
                  finish(r.message());
                  return;
                }
                if (r.output.trimmed().toLongLong() >= 50LL * 1024 * 1024)
                  largeFiles.append(file);
                inspectStaged();
              });
}
void RepositoryIndex::afterInspection() {
  if (!largeFiles.isEmpty()) {
    if(preparingExcluded.contains(".gitattributes")){finish("Cochez .gitattributes pour inclure la configuration Git LFS des gros fichiers sélectionnés.");return;}
    emit detail("Préparation Git LFS pour " +
                QString::number(largeFiles.size()) + " gros fichier(s)…");
    if (ToolRuntime::executable("git-lfs").isEmpty())
      installer.install("git-lfs");
    else
      configureLfs();
  } else if (needsAdd)
    stageAll();
  else
    unstageExcluded();
}

void RepositoryIndex::configureLfs() {
  if (QFileInfo(QDir(path).filePath(".gitattributes")).isSymLink()) {
    finish("Git LFS : .gitattributes est un lien symbolique. Remplacez-le par "
           "un fichier local.");
    return;
  }
  if(!includedPaths.contains(".gitattributes"))includedPaths.append(".gitattributes");
  runner->run(ToolRuntime::executable("git-lfs"), {"install", "--local"}, path,
              [this](const CommandResult &r) {
                if (!r.ok())
                  finish("Git LFS : " + r.message());
                else
                  trackNext();
              });
}
void RepositoryIndex::trackNext() {
  if (largeFiles.isEmpty()) {
    stageAll();
    return;
  }
  auto file = largeFiles.takeFirst();
  emit detail("Git LFS · " + file);
  runner->run(ToolRuntime::executable("git-lfs"),
              {"track", "--filename", "--", file}, path,
              [this](const CommandResult &r) {
                if (!r.ok())
                  finish("Git LFS : " + r.message());
                else
                  trackNext();
              });
}
void RepositoryIndex::stageAll() {
  emit detail("Préparation automatique des fichiers modifiés…");
  QByteArray paths;
  for(const auto &file:includedPaths){paths+=file.toUtf8();paths+='\0';}
  if(paths.isEmpty()){unstageExcluded();return;}
  runner->run("git", {"add","--all","--pathspec-from-file=-","--pathspec-file-nul"}, path,
              [this](const CommandResult &r) {
                if(r.ok())unstageExcluded();else finish(r.message());
              },paths);
}
void RepositoryIndex::unstageExcluded() {
  if(preparingExcluded.isEmpty()){finish();return;}
  QByteArray paths;
  for(const auto &file:preparingExcluded){paths+=file.toUtf8();paths+='\0';}
  runner->run("git",{"reset","--quiet","--pathspec-from-file=-","--pathspec-file-nul"},path,
              [this](const CommandResult &r){finish(r.ok()?QString():r.message());},paths);
}
void RepositoryIndex::finish(const QString &error) {
  active = false;
  auto callback = std::move(done);
  done = {};
  if (callback)
    callback(error);
}
