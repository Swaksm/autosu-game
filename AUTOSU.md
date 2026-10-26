# Autosu Game

Build privé de [neomod](https://github.com/neomodnet/neomod) (GPL-3.0) pour le live TikTok interactif **Autosu**.
Le serveur, l'overlay et la spec complète sont dans [Swaksm/Autosu](https://github.com/Swaksm/Autosu)
(voir `SPEC.md`, section « Le jeu »).

## État

Base : neomod **v44.01** + patch Autosu (canal UDP, `autosu_miss`, `autosu_retry`, état publié).
CI : `.github/workflows/autosu-win64.yml`, à chaque push sur `autosu` :
1. compile Windows x64 (audio SoLoud seul) ;
2. démarre le jeu sous Windows et lui fait jouer une map en Auto (`tests/sanity/run.py`) ;
3. test de bout en bout Autosu (`tests/autosu/e2e.py`) : vraies commandes UDP sur le build, échec si le miss, la
   vitesse, le CS, le retour à la normale, le retry ou le refus d'une commande hors liste ne se produit pas ;
4. publie le zip en release `build-<n>` (les 5 dernières sont gardées).

Télécharger le dernier build : `gh release download -R Swaksm/autosu-game -p autosu-game-win64.zip`.
En local : `python tests/autosu/e2e.py <chemin>/neomod.exe` (ports 13370 et 13371 libres).
Les workflows hérités de neomod ne se déclenchent que sur `master`, qu'on n'utilise pas.

## Branches et remotes

| Nom | Rôle |
|---|---|
| `autosu` (branche par défaut) | release neomod figée + nos commits |
| tag `v44.01` | base neomod actuelle |
| remote `origin` | ce dépôt privé |
| remote `upstream` | neomod, en lecture seule (push désactivé) |

## Le patch

| Fichier | Rôle |
|---|---|
| `src/App/Neomod/Autosu.cpp/.h` | tout le code Autosu : sockets UDP, liste autorisée, `autosu_miss`, `autosu_retry`, état JSON |
| `src/App/Neomod/OsuConVars/OsuConVarDefs.h` | convars `autosu_port` (13370), `autosu_state_port` (13371), commandes `autosu_miss`, `autosu_retry` |
| `src/App/Neomod/Osu.cpp` | `autosu::update()` une fois par frame dans `Osu::update()` |
| `src/App/Neomod/HitObjects.cpp` | branches Auto de `Circle::update` et de la tête de `Slider::update` : `autosu::consumeMiss()` |
| `src/App/Neomod/BeatmapInterface.cpp` | `drawSmoke` : fumée aussi quand `autosu_smoke` vaut 1 |
| `src/Makefile.sources` | ajout de `Autosu.cpp` (regénéré de toute façon par `autogen.sh`) |

- **Commandes** : un datagramme UDP = une commande console, reçue sur `127.0.0.1:autosu_port`. Seules
  `autosu_miss`, `autosu_retry`, `speed_override`, `cs_override`, `mod_autoplay`, `mod_nofail`, `mod_endless`,
  `pause_on_focus_loss`, `mod_hardrock`, `mod_flashlight`, `mod_hidden`, `autosu_smoke` (le curseur dessine de la fumée
  comme si la touche smoke était tenue, `BeatmapInterface::drawSmoke`), sa durée à l'écran
  `smoke_trail_duration` et `smoke_trail_opaque_duration`, `language` (le live est en anglais, neomod
  prend sinon la langue de Windows), et les réglages d'affichage `draw_hud`, `draw_fps`, `draw_playfield_border` (HUD masqué
  en live : l'overlay affiche déjà score, combo et pp) et `draw_statistics_pp` (neomod ne calcule les pp en direct
  que s'il est activé, `BeatmapInterface.cpp:2020`), ainsi que les volumes `volume_master`,
  `volume_master_inactive` (sinon le son tombe à 25 % dès que la fenêtre perd le focus), `volume_music` et
  `volume_effects` sont exécutées ; 16 au plus par frame ; tout datagramme qui ne vient pas de 127.0.0.1 est ignoré.
- **Mods en pleine partie** : neomod fige les mods au lancement de la map. Après `speed_override`, `cs_override`,
  `mod_autoplay`, `mod_nofail`, `mod_hardrock`, `mod_flashlight` ou `mod_hidden`, le patch appelle `Osu::updateMods()` (comme le sélecteur de mods) : vitesse et
  taille des cercles changent immédiatement.
- **État** : JSON envoyé à `127.0.0.1:autosu_state_port` toutes les 250 ms et juste après un miss, un retry ou un
  changement de map :
  `{"protocol":1,"playing":true,"combo":1243,"maxCombo":1500,"misses":3,"score":8812345,"acc":99.12,"pp":412.5,"progress":0.4200,"speed":1.00,"cs":4.30,"ppFc":530.2,"stars":9.11,"map":"Artist - Title [Diff]"}`
  (hors partie : `playing:false`, sans pp ni progression). `protocol` s'incrémente ici et dans le serveur à chaque
  changement incompatible.
- **Ports** : lus au démarrage du jeu ; si le port est pris, nouvel essai toutes les 5 s.

Règles : code Autosu isolé dans `src/App/Neomod/Autosu*`, préfixe `autosu_` pour toute convar ou commande
ajoutée, style `.clang-format` de neomod.

## Checklist CP2 (sur le vrai jeu)

Jeu lancé, une map en Auto ; depuis le dépôt `Autosu` :

```bash
npm run game:listen                              # l'état arrive en JSON, 4 fois par seconde
npm run game:send -- autosu_miss                 # exactement un objet raté, combo à 0, son de combo break
npm run game:send -- "speed_override 1.5"        # vitesse x1,5 ; "speed_override -1" pour revenir
npm run game:send -- "cs_override 7"             # cercles plus petits ; "cs_override -1" pour revenir
npm run game:send -- autosu_retry                # la map repart de zéro
npm run game:send -- quit                        # refusé (log « command refused »), le jeu continue
```

À vérifier aussi : un miss pendant un slider (seule la tête est ratée), pendant un spinner (le miss attend
l'objet suivant), hors partie (aucun effet).

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
