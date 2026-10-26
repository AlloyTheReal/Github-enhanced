#include "diagnostics.h"
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>

Diagnostics &Diagnostics::instance() {
  static Diagnostics diagnostics;
  return diagnostics;
}
bool Diagnostics::advanced() {
  return QSettings().value("diagnostics/advanced", false).toBool();
}
void Diagnostics::setAdvanced(bool enabled) {
  QSettings().setValue("diagnostics/advanced", enabled);
  emit advancedChanged(enabled);
}
ErrorExplanation Diagnostics::explain(const QString &raw) {
  auto clean = raw;
  clean.remove(QRegularExpression("\x1b\\[[0-?]*[ -/]*[@-~]"));
  if (clean.startsWith("Le brouillon existe sur GitHub") &&
      clean.contains('\n')) {
    const auto split = clean.indexOf('\n');
    return {clean.left(split), explain(clean.mid(split + 1)).text(), true};
  }
  const auto text = clean.toLower();
  auto has = [&](const char *pattern) {
    return QRegularExpression(QString::fromUtf8(pattern))
        .match(text)
        .hasMatch();
  };
  auto known = [](const char *title, const char *action) {
    return ErrorExplanation{QString::fromUtf8(title), QString::fromUtf8(action),
                            true};
  };
  if (has("workflow") && has("scope|autorisation"))
    return known("L’autorisation workflow manque pour cet envoi.",
                 "Utilisez « Autoriser les workflows GitHub… » dans le menu "
                 "Dépôt, puis validez dans le navigateur.");
  if (has("gh001|exceeds github.s file size limit|large files detected|file "
          "size limit of 100|larger than 100"))
    return known(
        "Un fichier dépasse la taille acceptée par GitHub.",
        "Utilisez Git LFS. Si le fichier est déjà dans un commit à envoyer, "
        "relancez l’envoi et acceptez la conversion proposée.");
  if (has("lfs.*(quota|budget|bandwidth|storage)|exceeded.*(quota|budget).*"
          "lfs"))
    return known("Le stockage ou le transfert Git LFS du dépôt est épuisé.",
                 "Vérifiez les limites LFS du propriétaire sur GitHub avant de "
                 "réessayer.");
  if (has("certificate|ssl peer|tls handshake|certificat"))
    return known("La connexion sécurisée n’a pas pu être vérifiée.",
                 "Vérifiez l’heure du système, les certificats et la "
                 "configuration du proxy, puis réessayez.");
  if (has("could not resolve|couldn't resolve|unable to resolve|network is "
          "unreachable|connection refused|failed to connect|réseau.*indisponible"))
    return known("Le serveur distant est inaccessible.",
                 "Vérifiez votre connexion, l’URL du dépôt et votre proxy ou "
                 "VPN, puis réessayez.");
  if (has("timed? out|timeout|délai.*dépass|expiré"))
    return known("L’opération a pris trop de temps.",
                 "Vérifiez la connexion et réessayez. Pour un envoi, vérifiez "
                 "d’abord l’état du dépôt distant.");
  if (has("non-fast-forward|fetch first|tip of your current branch is "
          "behind|branches.*diverg|branches.*different"))
    return known(
        "La branche locale et la branche distante ne sont plus alignées.",
        "Cliquez sur « Vérifier le distant », puis récupérez les commits "
        "distants. Si les branches divergent, consultez les écarts avant de "
        "fusionner.");
  if (has("unmerged|merge conflict|resolve.*conflict|conflit"))
    return known("Des conflits Git empêchent de continuer.",
                 "Résolvez les fichiers en conflit, enregistrez-les puis "
                 "terminez l’opération Git en cours.");
  if (has("would be overwritten|local changes.*overwritten"))
    return known("Des modifications locales seraient écrasées.",
                 "Créez un commit ou utilisez « Mettre de côté (stash) » avant "
                 "de recommencer.");
  if (has("index.lock|another git process|unable to create.*lock|cannot lock "
          "ref"))
    return known(
        "Git ne peut pas verrouiller le dépôt.",
        "Attendez la fin des autres opérations Git et fermez les outils qui "
        "utilisent ce dépôt. Si le problème persiste, consultez les détails "
        "avant de toucher aux fichiers de verrouillage.");
  if (has("no space left|disk full|espace.*insuffisant"))
    return known("Le disque n’a plus assez d’espace.",
                 "Libérez de l’espace sur le disque du dépôt et dans le "
                 "dossier temporaire, puis réessayez.");
  if (has("author identity unknown|unable to auto-detect email|please tell me "
          "who you are"))
    return known("L’identité de l’auteur des commits n’est pas configurée.",
                 "Renseignez votre nom et votre adresse e-mail dans Dépôt → "
                 "Réglages du dépôt…");
  if (has("not a git repository|not in a git directory"))
    return known("Ce dossier n’est pas un dépôt Git accessible.",
                 "Ouvrez le dossier qui contient le dépôt, clonez un projet ou "
                 "créez un dépôt local.");
  if (has("dubious ownership|unsafe repository"))
    return known("Git ne fait pas confiance au propriétaire de ce dossier.",
                 "Vérifiez les droits et le propriétaire du dépôt avant de "
                 "déclarer ce dossier fiable dans Git.");
  if (has("no configured push destination|no upstream branch|does not appear "
          "to be a git repository|no such remote|unknown remote"))
    return known(
        "La destination de l’envoi n’est pas configurée correctement.",
        "Vérifiez l’URL d’origin dans les réglages du dépôt, puis publiez la "
        "branche si elle n’existe pas encore sur le distant.");
  if (has("src refspec.*does not match|not a valid object name|ambiguous "
          "argument.*head"))
    return known("La branche ou le commit demandé n’existe pas.",
                 "Vérifiez la branche sélectionnée. Dans un nouveau dépôt, "
                 "créez un premier commit avant de l’envoyer.");
  if (has("already_exists|name already exists|already exists on this "
          "account|name.*already been taken|ce nom existe déjà"))
    return known("Un élément portant ce nom existe déjà.",
                 "Choisissez un autre nom ou ouvrez l’élément existant (dépôt, "
                 "tag ou release).");
  if (has("protected branch|gh006|gh013|repository rule|rule "
          "violations|pre-receive hook declined"))
    return known("Une règle du dépôt bloque l’envoi.",
                 "Vérifiez les règles de la branche sur GitHub. Vous devrez "
                 "peut-être envoyer une autre branche et ouvrir une pull "
                 "request ; les détails précisent le refus.");
  if (has("rate limit|too many requests|http 429"))
    return known("Le service limite temporairement les requêtes.",
                 "Patientez avant de réessayer et vérifiez les limites de "
                 "votre compte si le refus persiste.");
  if (has("usage limit|quota|insufficient_quota|credit "
          "balance|exceeded.*limit"))
    return known("La limite d’utilisation du service est atteinte.",
                 "Vérifiez les limites du compte connecté ou attendez leur "
                 "renouvellement avant de réessayer.");
  if (has("context.*(length|window)|maximum context|too many tokens|diff.*128 "
          "kio"))
    return known(
        "Les modifications sont trop volumineuses pour cette génération.",
        "Réduisez la portée des changements ou rédigez le résumé manuellement, "
        "puis réessayez.");
  if (has("authentication failed|bad credentials|(http |returned error: )401|not logged|not "
          "authenticated|permission denied \\(publickey|could not read "
          "username|oauth.*(denied|expired)|login.*required"))
    return known("La connexion au compte n’est pas valide.",
                 "Reconnectez le compte concerné dans les préférences. Pour "
                 "une URL SSH, vérifiez aussi votre clé SSH.");
  if (has("repository not found|(http |returned error: )404|^not found$|could not resolve to a "
          "repository"))
    return known("Ce dépôt est introuvable ou inaccessible à ce compte.",
                 "Vérifiez l’URL et le compte connecté. Un dépôt privé "
                 "nécessite un accès accordé par son propriétaire.");
  if (has("cannot open|unable to unlink|read-only file system|permission "
          "denied"))
    return known(
        "Un fichier local n’est pas accessible en écriture ou en lecture.",
        "Vérifiez les droits du dossier et si un autre programme verrouille ce "
        "fichier, puis réessayez.");
  if (has("saml|sso.*(required|enforcement)"))
    return known("L’organisation exige une validation de connexion SSO.",
                 "Autorisez l’accès de GitHub CLI auprès de l’organisation "
                 "dans votre navigateur, puis réessayez.");
  if (has("http 50[0-9]|service unavailable|internal server error|bad gateway"))
    return known("Le service distant rencontre un problème.",
                 "Patientez puis vérifiez l’état du dépôt distant avant de "
                 "réessayer l’opération.");
  if (has("(http |returned error: )403|resource not accessible|permission.*denied|write "
          "access.*not granted|not permitted|insufficient.*scope"))
    return known("Votre compte n’a pas l’autorisation nécessaire.",
                 "Vérifiez le compte connecté et ses droits sur ce dépôt ou "
                 "cette organisation. Certaines organisations exigent aussi "
                 "une validation SSO.");
  if (has("failed to start|no such file or directory|command not "
          "found|executable.*not found"))
    return known("Un fichier ou un composant nécessaire est introuvable.",
                 "Vérifiez le chemin du dépôt et les composants Git, GitHub "
                 "CLI ou Codex dans les préférences.");
  // Keep application-written French guidance, but never pass an unknown raw log
  // off as an explanation.
  if (clean.size() <= 900 &&
      !has("fatal:|error:|erreur\\s*:|http [45][0-9][0-9]|\\n") &&
      QRegularExpression(
          "^(Le |La |Les |L[’']|Un |Une "
          "|Aucun|Impossible|Installez|Vérifiez|Choisissez|Configurez|GitHub "
          "[an]|Ce |Cette |Connexion|Compte|Dépôt|Autorisation|Échec|Les "
          "commits)")
          .match(clean)
          .hasMatch())
    return {clean, {}, true};
  return {"L’opération n’a pas pu aboutir.",
          "Activez le mode avancé pour consulter le message d’origine. "
          "Vérifiez l’état du dépôt avant de réessayer.",
          false};
}
void Diagnostics::showError(QWidget *parent, const QString &title,
                            const QString &raw) {
  const auto explanation = explain(raw);
  QMessageBox box(QMessageBox::Warning, title, explanation.text(),
                  QMessageBox::Ok, parent);
  box.setTextFormat(Qt::PlainText);
  box.setDetailedText(raw);
  if (advanced())
    box.setInformativeText(
        "Mode avancé : le message d’origine est disponible dans les détails.");
  box.exec();
}
DiagnosticLabel::DiagnosticLabel(QWidget *parent) : QLabel(parent) {
  setTextFormat(Qt::PlainText);
  setWordWrap(true);
  setTextInteractionFlags(Qt::TextBrowserInteraction);
  connect(this, &QLabel::linkActivated, this, [](const QString &) {
    Diagnostics::instance().setAdvanced(!Diagnostics::advanced());
  });
  connect(&Diagnostics::instance(), &Diagnostics::advancedChanged, this,
          [this] { refresh(); });
}
void DiagnosticLabel::setText(const QString &text) {
  error.clear();
  setTextFormat(Qt::PlainText);
  QLabel::setText(text);
}
void DiagnosticLabel::setError(const QString &raw) {
  if (raw.isEmpty()) {
    setText({});
    return;
  }
  error = raw;
  refresh();
}
void DiagnosticLabel::refresh() {
  if (error.isEmpty())
    return;
  const auto simple = Diagnostics::explain(error).text();
  auto content = simple.toHtmlEscaped();
  if (Diagnostics::advanced())
    content += "\n\nDétails techniques :\n" + error.left(6000).toHtmlEscaped();
  content.replace('\n', "<br>");
  content += "<br><a href=\"details\">" +
             QString(Diagnostics::advanced() ? "Masquer les détails techniques"
                                             : "Détails techniques…") +
             "</a>";
  setTextFormat(Qt::RichText);
  QLabel::setText(content);
}
