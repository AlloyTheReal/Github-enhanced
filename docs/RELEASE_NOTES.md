# GitHub Enhanced 0.2.6

- Corrige le paquet AppImage : embarque les bibliothèques OpenSSL compatibles pour rétablir les connexions HTTPS et la recherche de mises à jour.
- Corrige le texte tronqué dans la fenêtre de mise à jour : les détails d’erreur sont défilables et les commandes restent accessibles.
- Affiche la cause précise des échecs de recherche : limite de requêtes GitHub, problème TLS, délai dépassé ou erreur réseau.
- Ajoute au workflow une vérification du démarrage de l’AppImage et de la disponibilité de TLS.

L’AppImage ne crée aucun raccourci automatiquement ; son intégration reste gérée par Shelly ou votre gestionnaire habituel.

Windows et macOS restent en bêta test. Leurs installateurs ne sont pas signés.
Les sommes de contrôle des fichiers distribués figurent dans `SHA256SUMS`.
