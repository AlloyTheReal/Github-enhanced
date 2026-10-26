# Architecture et périmètre

GitHub Enhanced 0.1 est un premier client Git utilisable, indépendant de GitHub.
Il ne constitue pas encore un remplacement complet de GitHub Desktop.

- **C++20 / Qt Widgets** : fenêtre native, barre de menus, raccourcis système, panneaux `QDockWidget` déplaçables, détachables et masquables. Les fonds de dialogue peuvent utiliser les matériaux AppKit sur macOS ; le reste des contrôles conserve un rendu Qt personnalisé.
- **CommandRunner** : `QProcess` asynchrone, arguments séparés sans shell, file FIFO, répertoire capturé par commande, limite de sortie de 16 Mio et arrêt après 120 s pour les lectures, 30 minutes pour les transferts et l’ajout de fichiers. La fermeture attend les opérations Git. Une file distincte sert aux diffs ; les résultats devenus obsolètes sont ignorés.
- **Git** : exécutable du système, configuration et gestionnaire d’identifiants existants. La connexion GitHub configure le gestionnaire HTTPS de GitHub CLI ; les accès SSH restent ceux de votre configuration Git. `pull --ff-only` refuse les divergences. Aucun push forcé ni abandon de fichiers.
- **Fichiers** : statut porcelain v1 avec séparateur NUL, prise en charge des renommages et des chemins littéraux. Le commit prépare automatiquement toutes les modifications non ignorées ; les conflits suspendent cette préparation. Les fichiers modifiés à partir de 50 Mio sont suivis avec Git LFS. La préparation automatique ne réécrit pas l’historique ; une conversion explicite des commits non publiés peut être proposée avant envoi. Historique limité aux 150 derniers commits. Diffs externes et textconv désactivés ; aperçus limités.
- **Personnalisation** : couleurs validées en JSON, taille de texte, surfaces inspirées du verre, options d’accessibilité, éditeur visuel de disposition, profils locaux. Tous les panneaux (diff et historique compris) sont repositionnables, redimensionnables, détachables et regroupables en onglets. La barre d’outils permet de sélectionner les commandes, leur ordre, leur visibilité et leur présentation. Les catalogues HTTPS contiennent directement des objets thème, limités à 200 entrées et 1 Mio. Un thème ne contient ni script ni feuille CSS arbitraire. Pas encore de personnalisation de chaque contrôle, police ou raccourci.
- **Comptes** : `GitHubClient` délègue OAuth à GitHub CLI, puis utilise l’API pour l’identité, les dépôts paginés et la création de dépôts. Synchronisation manuelle d’un Gist secret ; aucun jeton enregistré dans les préférences de l’application. `CodexClient` dialogue en JSON-RPC avec l’app-server officiel, connecté à ChatGPT. Les processus et téléchargements sont asynchrones. Voir [les comptes](ACCOUNTS.md).
- **Génération** : statistiques et extraits de diff bornés, avec abréviation des gros commits, sortie structurée, session éphémère dans un répertoire temporaire, sans outils ni accès réseau du bac à sable. Les propositions sont ignorées si l’index ou le dépôt ont changé. Les modifications du brouillon nécessitent un choix avant remplacement ; aucun commit automatique.
- **Outils officiels** : exécutables locaux ou système ; installation facultative depuis les releases officielles avec vérification SHA-256 fournie par GitHub. Les chemins des outils restent locaux et ne font pas partie des profils synchronisés.

- **RepositorySync** : vérification asynchrone des branches et des écarts de commits, récupération des références distante sans modifier l’arborescence, état de vérification et échecs réseau conservés séparément. Les envois nécessitent une action utilisateur ; la branche et la révision affichées sont revérifiées. L’état local propre n’est plus présenté comme une preuve de synchronisation distante.
- **CapsuleScrollBar** : curseur peint et animé, interactions de glissement et de pagination, commandes clavier héritées. Les mises à jour du texte et des listes sont évitées lorsque les données n’ont pas changé.

- **PushPreflight** : analyse des objets volumineux des commits non publiés avant chaque push, conversion LFS explicite après revue, références distantes exclues, sauvegarde du commit original et des références, restauration des fichiers de travail, vérification des objets et nouvelle analyse avant envoi normal. Les erreurs Git affichent les lignes de rejet utiles plutôt que le début du journal de progression.

## Apple HIG

L’implémentation reprend les conventions documentées par Apple : cadre de fenêtre système, menus natifs, rôles À propos/Préférences/Quitter, raccourcis standard (Command sur Mac), barre d’outils, redimensionnement et plein écran. Les actions de la barre d’outils sont accessibles dans les menus. Les contrôles Qt exposent leurs noms accessibles. Les dialogues de réglages utilisent une coque arrondie personnalisée et un pont AppKit optionnel ; il ne s’agit pas d’une interface entièrement composée de contrôles natifs Apple. Voir [les matériaux et limites du rendu](GLASS.md).

Cela exprime une direction de conception, pas une certification de conformité. Une revue sur un vrai Mac avec VoiceOver, contraste, navigation clavier, réglages d’accessibilité et différentes tailles de fenêtres reste nécessaire. Les thèmes personnalisés peuvent produire un contraste insuffisant. La disposition par défaut suit un modèle de client Git desktop : fichiers et commit à gauche, diff à droite, sélecteurs de dépôt et de branche dans la barre d’outils. Les panneaux sont verrouillés et leurs poignées masquées en utilisation normale ; l’éditeur les déverrouille pendant la personnalisation.

## Étapes restantes pour la version complète

1. Gestion des forks et pull requests, comptes GitHub Enterprise et sélection de plusieurs comptes simultanés.
2. Fusion/rebase avec résolution de conflits, graphe de branches, sélection de lignes, annulation de commit, amend, cherry-pick, Git LFS et sous-modules.
3. Marketplace publique : dépôt des thèmes, profils auteurs, recherche, modération, versions et intégrité des téléchargements.
4. Synchronisation automatique, profils par dépôt, éditeur de raccourcis et personnalisation des champs à l’intérieur de chaque panneau.
5. Validation native macOS/Windows, signature et notarisation, mises à jour, Flatpak, tests d’accessibilité et performances sur de gros dépôts.

Références : [Qt deployment](https://doc.qt.io/qt-6/deployment.html), [Apple — Designing for macOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-macos), [GitHub Gists API](https://docs.github.com/en/rest/gists/gists).
