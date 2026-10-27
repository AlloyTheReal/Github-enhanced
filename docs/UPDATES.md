# Mises à jour de l’application

Au lancement, l’application recherche une release stable sur le dépôt public `AlloyTheReal/Github-enhanced`. Cette source concerne l’application, pas le dépôt Git actuellement ouvert. La recherche est asynchrone, ne transmet aucun identifiant et ne nécessite pas de connexion GitHub. Les versions de développement, brouillons, préversions et versions plus anciennes sont exclus.

**Aide → Rechercher des mises à jour…** permet de consulter les notes, désactiver la vérification au lancement et télécharger la version compatible. L’asset doit provenir de ce dépôt, correspondre à la plateforme et fournir un digest `sha256` valide dans l’API GitHub. Le téléchargement est écrit progressivement dans le cache avec une limite de taille, puis vérifié avant de rendre l’installation possible. Sans asset vérifiable, le bouton ouvre la page de release.

Sous Linux, une AppImage lancée directement peut être remplacée après clic sur Installer. La copie se fait dans un thread de travail ; elle revérifie l’empreinte, sauvegarde la version courante en `.previous` et effectue un remplacement atomique. Le redémarrage est proposé ensuite. Les installations par DEB, RPM ou un gestionnaire de paquets restent mises à jour par ce gestionnaire. Le binaire d’une installation système n’est pas écrasé par l’application.

Sous Windows, le paquet EXE/MSI vérifié est ouvert pour lancer l’installation. Sous macOS, le DMG vérifié est ouvert ; l’utilisateur termine le remplacement de l’application avec les mécanismes habituels du système. Les signatures Windows et la notarisation macOS restent à configurer pour une distribution publique sans avertissement du système.

## Publier une version

Le workflow `release.yml` réutilise les builds Linux, Windows et macOS, récupère les paquets, produit `SHA256SUMS`, téléverse les assets dans une release de brouillon puis la publie seulement après le succès des trois builds. Il se déclenche lorsqu’un tag `vX.Y.Z` est poussé. La version CMake et la version embarquée suivent ce tag ; les builds locaux utilisent 0.2.1 par défaut. Aucun tag ni release distante n’a été créé pendant cette intervention.

Une release stable avec les fichiers appropriés doit être publiée pour proposer une mise à jour réelle. GitHub calcule le digest SHA-256 de chaque asset. Pour un fork, la clé locale `updates/repository` peut définir un autre `propriétaire/dépôt` ; cette source n’est pas importée depuis un thème ou une disposition.

Référence : [GitHub REST — assets de releases et digest](https://docs.github.com/en/rest/releases/assets).
