#pragma once
// Autosu : pilotage du jeu en UDP local pour le live TikTok interactif (voir AUTOSU.md).

class GameplayJudge;

namespace autosu {

// une fois par frame (Osu::update) : exécute les commandes reçues, publie l'état du jeu
void update();

// vrai une seule fois après autosu_miss, et seulement pour la partie en cours (pas une simulation) :
// l'Auto envoie alors HIT_MISS au lieu de HIT_300 pour cet objet
bool consumeMiss(const GameplayJudge *judge);

// fin de map en mode endless (Osu::onPlayEnd) : si autosu_endless_delay > 0, renvoie vrai et update() lance la map
// suivante après ce délai ; le jeu affiche l'écran de résultats en attendant
bool delayEndless();

// commandes console autosu_miss, autosu_retry et autosu_next (quitte la map, lance la suivante dans l'ordre)
void miss();
void retry();
void next();
// commande autosu_play : depuis le menu, ouvre la liste des maps et lance la suivante (rien en pleine partie)
void play();

}  // namespace autosu
