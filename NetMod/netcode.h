#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "memory_scanner.h"

// ─── Wire packet ──────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct NetPacket {
    uint32_t magic;        // 0x4C474F4C ("LGOL")
    uint8_t  version;      // Protocol version = 1
    uint8_t  playerSlot;   // 0=host, 1=client
    uint16_t seq;          // Wrapping sequence number
    uint32_t timestamp;    // timeGetTime()
    // Player state
    float    posX, posY, posZ;
    float    spdX, spdY, spdZ;
    float    fwdX, fwdY, fwdZ;
    float    upX,  upY,  upZ;
    int32_t  brick;
    int32_t  whiteBricks;
    uint8_t  flags;        // bit0=inRace, bit1=finished
    uint8_t  _pad[3];
};
#pragma pack(pop)

static_assert(sizeof(NetPacket) == 4+1+1+2+4 + 12+12+12+12+4+4+1+3, "NetPacket size mismatch");

#define NET_MAGIC    0x4C474F4CU
#define NET_VERSION  1
#define NET_PORT     27777
#define NET_TIMEOUT_MS 5000

// ─── Session ──────────────────────────────────────────────────────────────────
class NetSession {
public:
    NetSession();
    ~NetSession();

    bool StartHost(uint16_t port = NET_PORT);
    bool Connect(const char* ip, uint16_t port = NET_PORT);
    void Stop();

    // Send our player state; playerSlot = 0 if host, 1 if client
    void SendState(const PlayerState& state, uint8_t playerSlot);

    // Poll for new remote packet. Returns true if new data is available.
    bool RecvState(PlayerState* out);

    bool IsConnected() const { return connected; }
    uint32_t GetPing() const { return latencyMs; }

private:
    SOCKET     sock;
    sockaddr_in remoteAddr;
    bool       connected;
    bool       isHost;
    uint16_t   seqSend;
    uint16_t   seqRecvLast;
    uint32_t   latencyMs;
    uint32_t   lastRecvTime;

    // Latest received state
    PlayerState latestRemote;
    bool        hasNewRemote;

    bool InitWinsock();
};

extern NetSession* g_netSession;
