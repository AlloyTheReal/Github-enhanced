# Thèmes locaux et communautaires

Dans **Fichier → Préférences → Couleurs**, modifiez une couleur par sa pastille ou sa valeur HEX. Le sélecteur propose saturation/luminosité, teinte et couleurs prédéfinies. L’aperçu est immédiat ; **Enregistrer** applique la palette et crée une variante locale. **Annuler** conserve le thème actif. L’import/export JSON se trouve dans **Bibliothèque de thèmes**. Un import ou une installation depuis un catalogue ajoute immédiatement le thème à la bibliothèque locale.

Un thème v1 contient `version: 1`, un `id` de 1 à 48 caractères minuscules/chiffres/tirets (commençant par une lettre ou un chiffre), `name` et `author` (1 à 80 caractères), puis huit couleurs `#RRGGBB` :

```json
{
  "version": 1,
  "id": "mon-theme",
  "name": "Mon thème",
  "author": "Votre nom",
  "background": "#10141e",
  "surface": "#171d29",
  "text": "#e9edf5",
  "muted": "#9ba9bf",
  "accent": "#8c9eff",
  "border": "#2b3548",
  "added": "#6ee7ab",
  "removed": "#ff8896"
}
```

Pour partager une collection, hébergez un tableau JSON de ces objets sur une URL HTTPS (par exemple un fichier brut GitHub). Collez l’URL dans **Bibliothèque de thèmes**, chargez le catalogue, sélectionnez un thème puis installez-le. Maximum : 200 thèmes et 1 Mio. Un identifiant identique remplace le thème installé correspondant. Il n’existe pas encore de marketplace publique centralisée.

La disposition courante et le thème peuvent être envoyés manuellement sur le compte GitHub vérifié dans l’onglet **Compte GitHub**. Prérequis : GitHub CLI récent, authentifié sur github.com et autorisé à lire/écrire les Gists (`gh auth login --scopes gist` ; pour une session existante : `gh auth refresh --hostname github.com --scopes gist`). Le profil est placé dans `github-enhanced.json` dans un Gist secret nommé par description `GitHub Enhanced preferences v1`.

Un Gist secret est non répertorié, accessible à quiconque possède son URL. La disposition inclut le placement des panneaux ; les positions de panneaux flottants peuvent devoir être réinitialisées sur un autre écran ou OS. Le compte vérifié s’applique à la session de préférences ; les réglages locaux restent ceux du profil système courant.

## Palettes de départ

Les palettes Graphite, Papier et Océan complètent les quatre thèmes existants. Dans **Préférences → Couleurs → Palettes de départ**, une sélection remplit tous les rôles de couleur. Elle devient une variante locale que vous pouvez modifier couleur par couleur. L’aperçu en direct et l’annulation restent disponibles. Les palettes ne contiennent aucun code à exécuter.

Accès rapide aux couleurs : bouton **Couleurs…** dans la barre de menus ou d’état, ou **Fichier → Couleurs et palettes…**. Le parcours **Fichier → Préférences… → Couleurs** reste disponible.
