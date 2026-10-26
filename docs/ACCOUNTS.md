# Comptes GitHub et Codex

## Connexion

Ouvrez **Fichier → Préférences → Compte GitHub** ou **Compte Codex**. La connexion se termine dans le navigateur ; Codex propose aussi un code d’appareil. Le code peut être copié depuis le dialogue.

GitHub utilise le flux web de **GitHub CLI**, puis configure son gestionnaire d’identifiants pour Git HTTPS. Codex utilise **Codex app-server** et son authentification ChatGPT officielle. L’application ne demande pas de clé API et refuse une session Codex utilisant uniquement une clé API pour cette fonctionnalité.

Si un composant manque, le dialogue peut télécharger la version officielle adaptée à l’architecture et vérifier son empreinte SHA-256 avant installation dans le profil utilisateur. Vous pouvez aussi sélectionner un exécutable déjà installé. Aucun droit administrateur n’est nécessaire. Une connexion réseau et `tar` sont nécessaires à l’installation automatique.

Les sessions sont partagées avec les outils CLI correspondants. Se déconnecter depuis l’application affecte donc cette session partagée. Les outils officiels gèrent leurs identifiants selon leur propre configuration ; les jetons ne sont copiés ni dans les préférences de l’application ni dans les Gists synchronisés.

## Dépôts et configuration Git

**Mes dépôts GitHub…** affiche les dépôts personnels et accessibles, avec recherche dans les résultats chargés et pagination. Sélectionnez un dépôt pour le cloner ; un clone crée son répertoire `.git` normalement. L’accueil permet aussi de créer un dépôt Git local.

Le navigateur permet de créer un dépôt GitHub privé par défaut, avec un README optionnel, dans un formulaire intégré qui conserve les erreurs et les champs. **Dépôt → Publier ce dépôt sur GitHub…** crée un dépôt distant et ajoute `origin` ; l’envoi des commits existants reste une option explicite. Un dépôt possédant déjà `origin` doit être configuré dans les réglages.

**Dépôt → Réglages du dépôt…** permet de modifier l’identité Git locale, l’URL d’origin et `.gitignore`, avec modèles C++, Node.js, Python, Rust et système. L’identité du compte GitHub peut remplir les champs à votre demande. Une modification externe de `.gitignore` pendant l’édition empêche son écrasement silencieux.

## Générer un message de commit

Cliquez sur **Générer avec Codex** dans le panneau Commit. Les fichiers modifiés sont préparés automatiquement. Le service reçoit un résumé des changements et des extraits de diff, avec une consigne de rédaction. Les gros commits ne sont plus refusés à cause de l’ancienne limite de 128 Kio : le contexte transmis est abrégé, avec un signalement explicite au modèle. Une synthèse abrégée peut omettre des détails ; relisez la proposition. Les contenus des fichiers modifiés peuvent faire partie du texte transmis. L’accès dépend du compte ChatGPT et de ses limites Codex.

La session de génération est éphémère, lancée dans un dossier temporaire, sans outils ni approbations, avec bac à sable en lecture seule et réseau désactivé. Le service de génération reste évidemment accessible par le composant officiel. La proposition remplit un brouillon modifiable ; elle ne crée aucun commit. L’application vérifie que le dépôt et l’index sont toujours identiques avant de l’appliquer et propose un choix si vous avez modifié le brouillon entre-temps. Le bouton permet d’annuler une génération.

## Validation

Les tests utilisent des dépôts Git temporaires et des services simulés : authentification, protocoles, pagination, erreurs, annulation, création privée et protection du brouillon. L’initialisation du véritable app-server installé a également été vérifiée. Aucune connexion OAuth réelle, génération facturée, création de dépôt distant ou publication sur un compte réel n’a été effectuée pendant ces tests.

Sources : [GitHub CLI — connexion](https://cli.github.com/manual/gh_auth_login), [gestionnaire Git](https://cli.github.com/manual/gh_auth_setup-git), [OpenAI — app-server](https://learn.chatgpt.com/docs/app-server), [authentification Codex](https://learn.chatgpt.com/docs/auth).

Le contexte comprend jusqu’à 24 Kio de statistiques et 96 Kio d’extraits. Aucun fichier complet de plusieurs centaines de Mio n’est envoyé à Codex. Référence du protocole : [OpenAI Docs — app-server](https://learn.chatgpt.com/docs/app-server).
