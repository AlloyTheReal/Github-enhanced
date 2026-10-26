# Comprendre les erreurs

Le mode simple affiche une explication en français et une action à effectuer. L’interprétation couvre notamment les connexions et permissions GitHub, la permission `workflow`, les limites de fichiers et Git LFS, les problèmes réseau, les conflits, les branches désynchronisées, les verrous Git, les droits locaux et l’espace disque, ainsi que les limites du service de génération.

Une erreur inconnue reçoit une explication neutre et propose les détails ; sa cause n’est pas inventée. Les messages déjà rédigés par l’application restent disponibles. Les réponses originales sont conservées pour le diagnostic, sans modifier le résultat ni les règles de sécurité des opérations Git.

Activez **Affichage → Mode avancé · détails techniques**, la case de la fenêtre d’activité ou le lien **Détails techniques…** d’une erreur. Le choix est conservé entre les lancements. Le mode avancé affiche le journal des opérations et le message d’origine. Le lien permet également de revenir au mode simple. Les boîtes d’erreur disposent d’un bouton de détails.

Cette interprétation utilise des règles locales : aucun journal n’est envoyé à une IA pour expliquer une erreur. Elle ne relance pas une opération et ne modifie pas un dépôt automatiquement. Les actions de récupération explicites, comme l’autorisation des workflows, restent à déclencher.
