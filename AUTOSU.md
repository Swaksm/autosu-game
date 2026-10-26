# Autosu Game

Build privé de [neomod](https://github.com/neomodnet/neomod) (GPL-3.0) pour le live TikTok interactif **Autosu**.
Le serveur, l'overlay et la spec complète sont dans [Swaksm/Autosu](https://github.com/Swaksm/Autosu)
(voir `SPEC.md`, section « Le jeu »).

## État

Base : neomod **v44.01**, sans aucune modification pour l'instant.
GitHub Actions est désactivé sur ce dépôt jusqu'à l'ajout de notre workflow `autosu-win64.yml` (tâche T1) :
les workflows hérités de neomod ne se déclenchent que sur `master`, qu'on n'utilise pas.

## Branches et remotes

| Nom | Rôle |
|---|---|
| `autosu` (branche par défaut) | release neomod figée + nos commits |
| tag `v44.01` | base neomod actuelle |
| remote `origin` | ce dépôt privé |
| remote `upstream` | neomod, en lecture seule (push désactivé) |

## Ce que le patch ajoutera (à venir)

- Canal UDP local : commandes reçues sur `127.0.0.1:13370`, liste autorisée uniquement.
- Commandes `autosu_miss` et `autosu_retry`.
- État du jeu publié en JSON sur `127.0.0.1:13371`, avec un numéro de version de protocole (`"protocol": 1`).

Règles : code Autosu isolé dans `src/App/Neomod/Autosu*`, préfixe `autosu_` pour toute convar ou commande
ajoutée, style `.clang-format` de neomod.

## Mettre à jour neomod

```bash
git fetch upstream --tags
git rebase --onto <nouveau-tag> v44.01 autosu
```

Puis : CI verte, checklist des commandes rejouée sur le jeu, et seulement ensuite utilisation en live.
Mettre à jour la ligne « Base » ci-dessus.

## Règles

- **Jamais** connecter ce jeu modifié à un serveur osu! en ligne (bancho officiel ou privé).
- Dépôt privé : légal sous GPL-3.0 tant que le jeu compilé n'est donné à personne. S'il l'est un jour,
  le code source doit être fourni avec.
