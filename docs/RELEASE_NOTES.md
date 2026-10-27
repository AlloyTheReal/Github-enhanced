# GitHub Enhanced 0.2.2

- **Linux : fonctionnel.** Les builds, tests et paquets Linux sont validés par la CI.
- **Windows : bêta test.** Les installateurs EXE et MSI sont destinés aux tests et aux retours utilisateurs.
- **macOS : bêta test.** Le DMG universel Intel / Apple Silicon est destiné aux tests et aux retours utilisateurs.

## Correctifs

- Évite un blocage lorsque le lancement d’une commande échoue immédiatement, notamment sous Windows.
- Rend les sorties des tests Windows visibles et conserve les rapports de tests sur les trois systèmes.
- Réessaie la création du DMG en cas de verrouillage temporaire du système et corrige sa collecte dans les artefacts.

Les validations automatiques ne remplacent pas les essais des installateurs sur des machines réelles. Les installateurs Windows et macOS ne sont pas signés.

Les sommes de contrôle des fichiers distribués figurent dans `SHA256SUMS`.
