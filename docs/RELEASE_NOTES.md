# GitHub Enhanced 0.2.8

- Harmonise les marges à gauche et arrondit les panneaux de l’historique et du commit.
- Retire la barre d’état et affiche un bouton Activité flottant en bas à droite uniquement pendant une opération ou après un échec, avec une couleur adaptée.
- Conserve les préférences dans le menu Fichier et ajoute leur accès au clic droit en haut de la fenêtre.
- Allège l’historique : titres sur une ligne, métadonnées en infobulle et présentation compacte de l’auteur, de la date et du SHA dans le détail du commit.
- Nettoie les diffs pour afficher le contenu du fichier sans les en-têtes techniques Git, avec des couleurs verte et rouge plus franches.
- Affiche les commandes de présentation et de recherche uniquement lorsqu’un fichier est ouvert.
- Ajoute des cases cochées par défaut pour choisir les fichiers du commit. Les exclusions sont respectées pendant l’actualisation, la création du commit et la génération Codex ; les fichiers restent sur disque.
- Masque le bouton Disposition par défaut et déplace la personnalisation de l’espace de travail dans Apparence.
- Réorganise l’éditeur de disposition en onglets Panneaux, Barre d’outils et Profils. L’ordre et la visibilité des commandes se modifient dans la même liste, avec glisser-déposer, boutons de déplacement et annulation.

L’AppImage conserve les bibliothèques OpenSSL compatibles pour les connexions HTTPS.
Windows et macOS restent en bêta test. Les installateurs ne sont pas signés.
Les sommes de contrôle des fichiers distribués figurent dans `SHA256SUMS`.
