# Autosu Game

Build privé de [neomod](https://github.com/neomodnet/neomod) (GPL-3.0) pour le live TikTok interactif **Autosu**.
Le serveur, l'overlay et la spec complète sont dans [Swaksm/Autosu](https://github.com/Swaksm/Autosu)
(voir `SPEC.md`, section « Le jeu »).

## État

Base : neomod **v44.01**, sans modification du jeu pour l'instant.
CI : `.github/workflows/autosu-win64.yml` compile Windows x64 (audio SoLoud seul) à chaque push sur `autosu`,
puis démarre le jeu sous Windows et lui fait jouer une map en Auto. Le zip est dans l'onglet Actions, conservé 3 jours.
Les workflows hérités de neomod ne se déclenchent que sur `master`, qu'on n'utilise pas.

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

## Spike T2 : où brancher le patch (neomod v44.01)

### `autosu_miss`

En Auto, chaque objet est validé dans sa propre `update()` au moment exact où il doit être cliqué :

| Objet | Endroit | Code actuel |
|---|---|---|
| Cercle | `Circle::update`, `src/App/Neomod/HitObjects.cpp:894` | `if(curPosMS >= m_clickTimeMS) onHit(LiveHitResult::HIT_300, 0);` |
| Tête de slider | `Slider::update`, `src/App/Neomod/HitObjects.cpp:1603` | `onHit(LiveHitResult::HIT_300, 0, false); m_judge->holding_slider = true;` |

Approche retenue : un miss **en attente**, consommé par le prochain cercle ou la prochaine tête de slider.

- `autosu_miss` pose le drapeau (sans effet hors partie).
- Aux deux endroits ci-dessus, l'Auto envoie `HIT_MISS` au lieu de `HIT_300` si un miss est en attente, puis
  efface le drapeau : **exactement un objet raté**. Pour un slider, seule la tête est ratée ; l'Auto suit le
  corps normalement.
- `HIT_MISS` passe par `GameplayJudge::addHitResult` (`GameplayJudge.cpp:545`), qui appelle déjà
  `listener->onComboBreak()` : combo remis à 0 et son de combo break, sans code en plus.
- Les spinners ne consomment pas le miss (il attend l'objet suivant).
- Le drapeau est effacé au restart et au changement de map, pour ne jamais rater un objet plus tard par surprise.

Repli si ça pose problème : retirer `Autoplay` le temps d'un objet (moins précis, gardé en réserve).

### `autosu_retry`

`BeatmapInterface::restart(true)` (`BeatmapInterface.cpp:752`) : le même chemin que le quick retry du jeu
(`Osu.cpp:1028`). Sans effet hors partie.

### Canal UDP et état

| Besoin | Ce que neomod a déjà |
|---|---|
| Exécuter une commande reçue | `Console::processCommand(std::string_view)` (`src/Engine/ConVars/Console.h:24`) |
| Point d'appel à chaque frame | `Osu::update()` (`src/App/Neomod/Osu.cpp:576`) |
| Déclarer une commande | `CONVAR(nom, CLIENT \| SERVER, CFUNC(fonction))` dans `OsuConVars/OsuConVarDefs.h` (ex. `spectate`, ligne 56) |
| Sockets | aucun code UDP dans neomod : à écrire (Winsock, déjà lié via curl : `mk/deps/curl.am:39`) |
| Compiler un nouveau fichier | l'ajouter à `src/Makefile.sources` |
| pp en direct | `src/App/Neomod/DiffCalc/LivePPCalc.cpp` |

### Lignes d'accroche prévues (le reste vit dans `src/App/Neomod/Autosu*.cpp/.h`)

1. `OsuConVarDefs.h` : `autosu_port`, `autosu_miss`, `autosu_retry`.
2. `Osu.cpp`, `Osu::update()` : un appel par frame (lire les commandes UDP, publier l'état).
3. `HitObjects.cpp` : les deux branches Auto ci-dessus.
4. `src/Makefile.sources` : les nouveaux fichiers.

Test de bout en bout possible en CI : `tests/sanity/run.py` démarre déjà le jeu sans écran et lui fait jouer une map
en Auto ; on pourra y ajouter l'envoi de commandes UDP.

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
