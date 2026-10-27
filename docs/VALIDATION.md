# Validation locale — 27 octobre 2026

Environnement : Linux x86_64 (CachyOS), glibc 2.44, Qt 6.12.0, GCC 16.2.1 et CMake 4.4.4.

- Compilation Release réussie.
- Trois suites CTest réussies : opérations Git réelles et scénarios Git et personnalisation dans l’interface (placement, dimensions, onglets, panneaux détachés, commandes, profils, annulation et restauration après redémarrage).
- Démarrage de l’exécutable vérifié avec Qt offscreen et dans la session graphique Wayland.
- Captures des vraies fenêtres : `preview.png`, `workspace.png`, `customization.png`, `settings-appearance.png`, `settings-colors.png` et `color-picker.png`.
- AppImage x86_64 générée ; démarrage validé en mode extraction sans FUSE.
- Archive binaire Linux et archive source générées avec CPack.
- Réglages : navigation, menus déroulants, sélecteur de couleur, validation HEX, création d’une variante locale, interrupteurs et restauration des options visuelles après Annuler.
- Syntaxe Bash des scripts de distribution vérifiée.

L’AppImage locale contient les bibliothèques de ce système récent : elle ne prouve pas une compatibilité avec d’anciennes distributions. Le workflow Linux compile sur Ubuntu 24.04 pour une base plus conservatrice.

Non exécutés ici : builds Windows/macOS et intégration AppKit (`NSGlassEffectView`/`NSVisualEffectView`), génération DEB/RPM/EXE/MSI/DMG, installation native de ces paquets, GitHub CLI authentifié, synchronisation Gist, catalogue HTTPS externe, signatures/notarisation et revue Apple HIG/VoiceOver. Les configurations correspondantes sont présentes ; ces validations restent à faire sur les plateformes et comptes concernés.

Les nouvelles intégrations sont testées avec un service local simulé : OAuth GitHub/Codex, réponse fragmentée de l’app-server, erreurs, refus des outils et des sessions API key, annulation, pagination des dépôts, création privée et préservation des brouillons. L’éditeur est testé par glissement des séparateurs et panneaux, annulation, aperçu des couleurs et restauration. Les réglages Git sont vérifiés sur un vrai dépôt temporaire, y compris la détection des modifications externes de `.gitignore`.

L’initialisation JSON-RPC du véritable Codex app-server installé est validée. La connexion OAuth réelle et la production d’un message par un compte ChatGPT restent à vérifier avec le consentement utilisateur dans le navigateur. Aucun dépôt distant réel n’a été créé et aucun quota de génération n’a été consommé.

## Préparation automatique, historique et gros commits

Tests ajoutés : préparation automatique sans commandes d’indexation, conflit préservé, règles d’exclusion respectées, consultation des contenus de fichiers dans un commit, contenu précédent d’un fichier supprimé et diff individuel, retrait des accès récents sans suppression du dossier, progression Git et persistance des erreurs, génération simulée d’un gros commit et abréviation d’un contexte de plus de 128 Kio.

Le test LFS utilise le composant officiel Git LFS 3.8.1 Linux x86_64, téléchargé avec empreinte SHA-256 vérifiée : fichier de 51 Mio converti en pointeur, règle `.gitattributes` indexée, nom contenant des crochets traité littéralement, contenu du fichier de travail conservé et préparation répétée stable. Aucun objet LFS n’a été envoyé à un serveur distant. Ce test est sauté si Git LFS n’est pas disponible ; la validation locale l’exécute avec `GHE_TEST_LFS`.

Dernier passage complet : **3 suites CTest réussies**, avec le test Git LFS réel activé et aucun envoi distant. Captures vérifiées : `commit-browser.png` et `operation-progress.png` (exemple de progression simulé).

## Suivi distant et défilement

Tests Git réels avec un dépôt bare et deux copies temporaires : égalité avec références en cache non vérifiées, fetch vérifié, commit local non envoyé malgré un état de travail propre, commit entrant, divergence, distant inaccessible et maintien de l’erreur, absence de suivi, dépôt local et HEAD détachée. Le scénario UI vérifie le compteur, l’envoi explicite au dépôt bare de test, le retour à un état synchronisé, le refus d’agir sur une branche modifiée entre-temps et les fichiers encore à commiter après un envoi.

Les interactions des capsules verticales/horizontales, la molette, le clavier et la réduction des animations sont vérifiées. Un test UI conserve le contenu et les positions de défilement des fichiers et du diff après une actualisation. Les captures `sync-ahead.png` et `capsule-scroll.png` sont réalisées sur les fenêtres Qt.

Le diagnostic du dépôt utilisateur a été fait en lecture seule (`status`, branches, compte des commits et `ls-remote`). Aucun commit du dépôt utilisateur n’a été envoyé, aucune fusion ni réécriture d’historique n’a été effectuée.

## Gros objets déjà commités et refus GH001

Tests réels avec Git LFS 3.8.1 : objet de 51 Mio ajouté comme fichier Git classique puis supprimé dans un commit ultérieur, détection avant envoi, refus d’une conversion avec modifications locales, conversion de la partie non publiée, conservation du commit publié et d’une autre branche locale, référence de sauvegarde pointant sur le commit original, fichiers source conservés et push normal vers un dépôt bare temporaire.

L’interface est testée dans les deux cas : annulation sans modification des commits, puis conversion et envoi LFS acceptés. Le nouvel identifiant est envoyé à la place de l’ancien, la sauvegarde conserve l’ancien commit et le fichier de travail conserve sa taille et son empreinte Git après conversion. Le contrôle a révélé que `GIT_LITERAL_PATHSPECS` ne doit pas être imposé aux commandes internes de Git LFS ; cet environnement est corrigé pour permettre la restauration des fichiers complets.

Le dépôt utilisateur et le fichier AppImage incriminé ont été inspectés en lecture seule. Sa conversion et son envoi ne sont pas effectués par les tests ; l’action est disponible dans l’application lors du prochain envoi.

## Création de dépôt et releases

La création est testée avec un clic réel sur Nouveau dépôt, affichage du formulaire intégré, validation du compte, erreur GitHub 422, conservation du nom, nouvelle tentative et sélection du dépôt créé avec option README. Les tests de releases couvrent la détection du distant, le changement de dépôt avec réponses tardives, la consultation, l’édition, la publication d’une préversion, la suppression de release/pièce jointe avec confirmation, les droits en lecture seule et la reprise d’un transfert après échec sans duplication de release. Un nom de fichier contenant un espace et `#` est transmis intact.

Les écritures GitHub sont simulées dans les tests : aucun dépôt, release ou fichier joint réel n’est créé ou supprimé sur le compte utilisateur. Le compte connecté et l’accès en lecture aux métadonnées et aux releases du dépôt existant ont été vérifiés avec le composant GitHub CLI installé. Les captures `new-repository.png` et `releases.png` proviennent de l’interface Qt avec données de test.

## Comptes, organisations et noms de dépôt

Tests ajoutés : publication avec compte connecté et bouton de connexion masqué, sélecteur personnel/organisations, membres actifs et pagination des appartenances, organisation sans droit désactivée, droits des administrateurs, absence de bascule automatique vers Public, normalisation des espaces et espaces Unicode, requête vers `orgs/{organisation}/repos`, conservation de la visibilité choisie et absence de README lors de la publication d’un dépôt local. Les écritures restent simulées, sans création dans les organisations du compte utilisateur.

## Permission OAuth workflow

Les nouvelles connexions demandent `gist,workflow` en complément des permissions par défaut de GitHub CLI. Les tests vérifient la commande d’ajout de permission à une session existante (`auth refresh --hostname github.com --scopes workflow`), l’absence de jeton d’environnement transmis et le traitement d’un refus d’autorisation. Le dialogue distingue cette action de la connexion initiale.

Un serveur Git local avec hook de réception reproduit le refus OAuth exact concernant `.github/workflows/build.yml`. Les tests de l’interface vérifient le bouton de récupération, l’envoi réussi après autorisation simulée et l’absence d’envoi après refus. Une nouvelle opération efface toute ancienne action de récupération. La reprise contrôle de nouveau la branche et HEAD, puis repasse par la vérification des objets à envoyer.

Validation : compilation Release et trois suites CTest réussies, avec les tests LFS réels activés. Aucune permission d’un compte réel n’a été modifiée et aucun envoi vers GitHub n’a été effectué ; l’utilisateur doit terminer l’autorisation dans son navigateur.

## Erreurs lisibles et mode avancé

Vingt cas de diagnostic vérifiés : workflow OAuth, gros fichiers et quota LFS, DNS, TLS, branche désynchronisée, conflit, verrou, disque plein, identité manquante, dossier sans dépôt, connexion invalide, refus HTTPS 403, permissions GitHub, dépôt introuvable, nom déjà utilisé, règle de branche, limite de requêtes, contexte trop grand et délai dépassé. Un refus HTTPS n’est pas présenté comme une panne réseau. Les erreurs inconnues reçoivent une explication neutre ; les détails restent consultables.

Tests supplémentaires : conservation du message d’origine, échappement du texte technique, contexte d’un brouillon de release déjà créé, journal masqué en mode simple, bascule entre les modes, préférence mémorisée, synchronisation du menu Affichage et de la fenêtre d’activité, et absence de réapparition d’une ancienne erreur après un nouveau statut.

Les trois suites CTest passent, avec Git LFS réel activé. Captures contrôlées : `error-simple.png`, `error-advanced.png` et `organization-menu.png`. Les listes déroulantes utilisent un composant commun avec vue compacte, surlignage arrondi, défilement et animation respectant l’option de réduction des mouvements. Le rendu natif Windows/macOS reste à vérifier sur ces plateformes.


## Refonte 0.2.0 — 27 octobre 2026

Compilation Release et trois suites CTest réussies, avec Git LFS réel activé. Les tests ajoutés couvrent :

- Création visible d’un panneau, annulation/rétablissement avec contenu et restauration après redémarrage.
- Sérialisation des tâches, cases cochées, état de dépôt et rejet des définitions de panneau invalides.
- Application des palettes de départ sans supprimer l’édition des rôles de couleur.
- Synchronisation Fetch/Push/Pull sur un distant nommé upstream, avec fetch initial et comparaison sans configuration de suivi. Les HEAD local et distant sont vérifiés après push et pull.
- Fusion réelle de deux branches, refus sur dépôt modifié, résolution locale/entrante de conflits et annulation conservant le commit précédent.
- Versions sémantiques, choix d’asset et d’architecture, rejet d’une URL extérieure au dépôt et d’une empreinte invalide, téléchargement simulé et rejet d’un fichier corrompu.
- Remplacement atomique d’une AppImage de test, sauvegarde de la version précédente et protection contre une modification du fichier après téléchargement. Aucun exécutable utilisateur n’a été remplacé.

La configuration sous un tag v2.3.4 produit bien une version embarquée 2.3.4. Les deux workflows sont syntaxiquement valides. Les captures workspace, custom-panels et palette-presets sont contrôlées visuellement ; le logo PNG/ICO/ICNS est dérivé du SVG fourni.

Aucune release distante n’a été créée, aucun tag poussé et aucun téléchargement/installation d’une vraie mise à jour n’a été effectué sur le compte utilisateur. Le workflow de publication et les installateurs Windows/macOS restent à exécuter sur leurs plateformes. La signature, notarisation et validation HIG/VoiceOver native restent à réaliser avant une distribution publique finalisée.

L’AppImage Linux 0.2.0 a été générée et son démarrage contrôlé avec Qt offscreen, en mode extraction et avec un profil isolé. Les archives binaire et source 0.2.0 sont également générées. La version embarquée affiche 0.2.0.

Accès aux réglages : restauration de la barre d’outils vérifiée avec celle-ci masquée, barre d’état masquée et profil rechargé ; ouverture directe de la page Couleurs vérifiée. Captures contrôlées : `toolbar-recovery.png` et `colors-shortcut.png`.

## Correction 0.2.1 — focus et disposition

Les trois suites CTest passent : tests du cœur, 27 cas UI et 53 cas d’intégration des comptes, avec Git LFS réel activé. Les opérations Git utilisent uniquement des dépôts temporaires ; les écritures GitHub restent simulées.

Les nouveaux scénarios vérifient le focus, la sélection et le brouillon lors des changements externes, l’absence de commandes de rafraîchissement global lorsque le dépôt est inchangé, l’absence de fenêtre de progression lors de la surveillance et la conservation des modifications saisies pendant un commit. Les tests de disposition couvrent le placement direct, les onglets, le détachement, l’ancrage sur les bords extérieurs, l’annulation, la saisie des dimensions sans écrasement et la recherche de commandes. Les hauteurs de ligne et l’absence de chevauchement sont contrôlées à 11, 16 et 22 points.

Les captures de l’éditeur ont été renouvelées. L’AppImage 0.2.1 est générée et son démarrage est vérifié en mode extraction/offscreen avec un profil isolé ; les archives Linux et source 0.2.1 sont générées. La version embarquée affiche 0.2.1.


## Version 0.2.8 — sélection des fichiers et interface

Compilation locale réussie et trois suites CTest validées après les modifications de l’interface. La suite UI compte 30 cas réussis et deux cas LFS ignorés, Git LFS n’étant pas installé dans cet environnement. Les cas d’intégration nécessitant Git LFS sont également ignorés ; les validations LFS des versions précédentes ne constituent pas une nouvelle validation de cette version.

Les nouveaux tests vérifient l’exclusion effective d’un fichier du premier commit, sa conservation sur disque et l’état décoché après actualisation. Un test d’index couvre les noms contenant des crochets et plusieurs préparations successives. Les tests de diff contrôlent l’absence des en-têtes Git et le masquage des outils sans fichier.

Captures Qt contrôlées : historique, modifications avec cases, diff unifié/côte à côte, Apparence et éditeur de barre d’outils, y compris à 22 points. Les builds et paquets Windows/macOS sont confiés au workflow de release.
