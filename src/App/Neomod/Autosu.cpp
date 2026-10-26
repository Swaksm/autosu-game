// Autosu : canal UDP local du live TikTok interactif (voir AUTOSU.md).
// Reçoit des commandes console sur 127.0.0.1:autosu_port (liste autorisée uniquement) et publie l'état du jeu en JSON
// sur 127.0.0.1:autosu_state_port, 4 fois par seconde et juste après un miss, un retry ou un changement de map.
#include "config.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "Autosu.h"

#include "BeatmapInterface.h"
#include "Console.h"
#include "DatabaseBeatmap.h"
#include "Logging.h"
#include "Osu.h"
#include "OsuConVars.h"
#include "score.h"

#include "fmt/format.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace autosu {
namespace {

constexpr int PROTOCOL = 1;  // à incrémenter ici et dans le serveur à chaque changement incompatible
constexpr auto STATE_INTERVAL = std::chrono::milliseconds(250);
constexpr auto BIND_RETRY_INTERVAL = std::chrono::seconds(5);
constexpr int MAX_COMMANDS_PER_FRAME = 16;  // une rafale de commandes ne bloque jamais une frame

// seules ces commandes sont exécutées, tout le reste est ignoré
constexpr std::array<std::string_view, 7> ALLOWED{"autosu_miss", "autosu_retry",         "speed_override",
                                                  "cs_override", "mods",                 "mod_endless",
                                                  "pause_on_focus_loss"};

#ifdef MCENGINE_PLATFORM_WINDOWS
using socket_t = SOCKET;
constexpr socket_t NO_SOCKET = INVALID_SOCKET;
void closeSocket(socket_t s) { closesocket(s); }
#else
using socket_t = int;
constexpr socket_t NO_SOCKET = -1;
void closeSocket(socket_t s) { close(s); }
#endif

using steady = std::chrono::steady_clock;

std::atomic<bool> pendingMiss{false};
std::atomic<bool> sendNow{false};
socket_t recvSocket = NO_SOCKET;
socket_t sendSocket = NO_SOCKET;
steady::time_point nextBindAttempt{};
steady::time_point nextStateAt{};
const void *lastMap = nullptr;

sockaddr_in loopback(int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return addr;
}

bool setNonBlocking(socket_t s) {
#ifdef MCENGINE_PLATFORM_WINDOWS
    u_long on = 1;
    return ioctlsocket(s, FIONBIO, &on) == 0;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// ouvre les sockets au premier appel, puis réessaie toutes les 5 s si le port est pris
bool ensureSockets() {
    if(recvSocket != NO_SOCKET) return true;
    const auto now = steady::now();
    if(now < nextBindAttempt) return false;
    nextBindAttempt = now + BIND_RETRY_INTERVAL;

#ifdef MCENGINE_PLATFORM_WINDOWS
    static const bool wsaReady = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    if(!wsaReady) return false;
#endif

    const socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s == NO_SOCKET) return false;
    const sockaddr_in addr = loopback(cv::autosu_port.getInt());
    if(bind(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0 || !setNonBlocking(s)) {
        debugLog("Autosu: UDP port 127.0.0.1:{} unavailable, retrying in 5 s", cv::autosu_port.getInt());
        closeSocket(s);
        return false;
    }
    recvSocket = s;
    // envoi sur un socket séparé : sous Windows, un envoi vers un port fermé ferait échouer la réception suivante
    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    debugLog("Autosu: commands on 127.0.0.1:{}, state published to 127.0.0.1:{}", cv::autosu_port.getInt(),
             cv::autosu_state_port.getInt());
    return true;
}

bool allowed(std::string_view command) {
    const std::string_view name = command.substr(0, command.find(' '));
    for(const auto a : ALLOWED)
        if(a == name) return true;
    return false;
}

void receiveCommands() {
    std::array<char, 512> buffer{};
    for(int i = 0; i < MAX_COMMANDS_PER_FRAME; i++) {
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        const auto n = recvfrom(recvSocket, buffer.data(), static_cast<int>(buffer.size()), 0,
                                reinterpret_cast<sockaddr *>(&from), &fromLen);
        if(n <= 0) return;                                            // plus rien à lire
        if(from.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;  // la machine locale uniquement
        std::string_view command(buffer.data(), static_cast<size_t>(n));
        while(!command.empty() && (command.back() == '\n' || command.back() == '\r' || command.back() == ' '))
            command.remove_suffix(1);
        if(!allowed(command)) {
            debugLog("Autosu: command refused: {}", command);
            continue;
        }
        Console::processCommand(command);
    }
}

// JSON : guillemets, antislash et caractères de contrôle échappés (le titre vient des fichiers .osu)
std::string jsonString(std::string_view s) {
    std::string out = "\"";
    for(const char c : s) {
        if(c == '"')
            out += "\\\"";
        else if(c == '\\')
            out += "\\\\";
        else if(static_cast<unsigned char>(c) < 0x20)
            out += fmt::format("\\u{:04x}", static_cast<unsigned>(c));
        else
            out += c;
    }
    return out + '"';
}

void publishState() {
    const LiveScore *score = osu->getScore();
    BeatmapInterface *map = osu->getMapInterface();
    const bool playing = osu->isInPlayMode() && map != nullptr;
    const DatabaseBeatmap *diff = map != nullptr ? map->getBeatmap() : nullptr;

    std::string json = fmt::format(R"({{"protocol":{},"playing":{})", PROTOCOL, playing);
    if(score != nullptr)
        json += fmt::format(R"(,"combo":{},"maxCombo":{},"misses":{},"score":{},"acc":{:.2f})", score->getCombo(),
                            score->getComboMax(), score->getNumMisses(), score->getScore(),
                            score->getAccuracy() * 100.f);
    if(playing) {
        json += fmt::format(R"(,"pp":{:.1f},"progress":{:.4f},"speed":{:.2f},"cs":{:.2f})", map->live_pp(),
                            map->getPercentFinished(), map->getSpeedMultiplier(), map->getCS());
        const auto &whole = map->getWholeMapPPInfo();
        if(whole.pp >= 0.0) json += fmt::format(R"(,"ppFc":{:.1f},"stars":{:.2f})", whole.pp, whole.total_stars);
    }
    if(diff != nullptr)
        json += fmt::format(R"(,"map":{})", jsonString(fmt::format("{} - {} [{}]", diff->getArtist(),
                                                                     diff->getTitle(), diff->getDifficultyName())));
    json += '}';

    const sockaddr_in to = loopback(cv::autosu_state_port.getInt());
    sendto(sendSocket, json.data(), static_cast<int>(json.size()), 0, reinterpret_cast<const sockaddr *>(&to),
           sizeof(to));
}

}  // namespace

void update() {
    if(osu == nullptr || !ensureSockets()) return;
    receiveCommands();

    // un miss en attente ne survit pas à un changement de map ni à la sortie de la partie
    BeatmapInterface *map = osu->getMapInterface();
    const void *current = osu->isInPlayMode() && map != nullptr ? map->getBeatmap() : nullptr;
    if(current != lastMap) {
        lastMap = current;
        pendingMiss = false;
        sendNow = true;
    }

    const auto now = steady::now();
    if(sendNow.exchange(false) || now >= nextStateAt) {
        nextStateAt = now + STATE_INTERVAL;
        publishState();
    }
}

bool consumeMiss(const GameplayJudge *judge) {
    BeatmapInterface *map = osu != nullptr ? osu->getMapInterface() : nullptr;
    if(map == nullptr || map->getJudge() != judge || !pendingMiss.exchange(false)) return false;
    sendNow = true;
    return true;
}

void miss() {
    if(osu == nullptr || !osu->isInPlayMode()) return;
    pendingMiss = true;
}

void retry() {
    if(osu == nullptr || !osu->isInPlayMode() || osu->getMapInterface() == nullptr) return;
    pendingMiss = false;
    osu->getMapInterface()->restart(true);
    sendNow = true;
}

}  // namespace autosu
