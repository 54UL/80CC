#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <enet/enet.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ettycc
{
    class NetworkComponent; // forward

    // ── Packet types ──────────────────────────────────────────────────────────
    enum NetMsgType : uint8_t
    {
        NET_MSG_TRANSFORM = 1,
    };

#pragma pack(push, 1)
    struct TransformPacket
    {
        uint8_t  msgType   = NET_MSG_TRANSFORM;
        uint32_t networkId = 0;
        float    px = 0, py = 0, pz = 0;
        float    rw = 1, rx = 0, ry = 0, rz = 0; // quaternion (w first)
        float    sx = 1, sy = 1, sz = 1;
    };
#pragma pack(pop)

    // ── Connection state ──────────────────────────────────────────────────────
    enum class NetState
    {
        OFFLINE,        // no host created
        CONNECTING,     // InitClient called, awaiting CONNECT event
        CONNECTED,      // ready to send/receive
        DISCONNECTED,   // remote peer dropped the connection
    };

    // ── NetworkManager ────────────────────────────────────────────────────────
    class NetworkManager
    {
    public:
        NetworkManager();
        ~NetworkManager();

        bool InitHost  (uint16_t port);
        bool InitClient(const std::string& address, uint16_t port);

        // Service ENet events; call once per frame from Engine::Update.
        void Poll();

        // Graceful shutdown — releases physics on all registered components,
        // clears the registry and destroys the ENet host.
        void Shutdown();

        NetState    GetState   () const { return state_;   }
        const char* GetStateStr() const;

        bool IsHost  () const { return isHost_;          }
        bool IsActive() const { return host_ != nullptr; }

        // ── Bandwidth / stats ─────────────────────────────────────────────────
        struct BandwidthStats
        {
            float    sendBps      = 0.f;  // current send rate  (bytes/sec)
            float    recvBps      = 0.f;  // current recv rate  (bytes/sec)
            uint64_t totalBytesSent     = 0;
            uint64_t totalBytesReceived = 0;
            double   connectedForSecs   = 0.0; // seconds since CONNECTED
        };

        static constexpr int kBwHistorySize = 128;  // one sample per second

        const BandwidthStats& GetBandwidthStats() const { return bwStats_; }

        // Circular-buffer history — index 0..kBwHistorySize-1.
        // Use GetBwHistoryOffset() as the `values_offset` for ImGui::PlotLines.
        const std::array<float, kBwHistorySize>& GetSendHistory() const { return sendHistory_; }
        const std::array<float, kBwHistorySize>& GetRecvHistory() const { return recvHistory_; }
        int GetBwHistoryOffset() const { return bwHistoryOffset_; }

        // ── Per-object debug ──────────────────────────────────────────────────
        struct DebugEntry
        {
            glm::vec3 pos   = {};
            uint64_t  count = 0; // total packets processed for this id
        };

        uint64_t GetTotalSent    () const { return totalSent_;     }
        uint64_t GetTotalReceived() const { return totalReceived_; }
        const std::unordered_map<uint32_t, DebugEntry>& GetDebugEntries() const
            { return debugEntries_; }

        // ── NetworkComponent lifecycle ────────────────────────────────────────
        void Register  (uint32_t networkId, NetworkComponent* comp);
        void Unregister(uint32_t networkId);

        void BroadcastTransform(uint32_t networkId,
                                const glm::vec3& pos,
                                const glm::quat& rot,
                                const glm::vec3& scale);

    private:
        bool       isHost_     = false;
        ENetHost*  host_       = nullptr;
        ENetPeer*  serverPeer_ = nullptr;

        NetState   state_      = NetState::OFFLINE;

        std::chrono::steady_clock::time_point connectStart_;
        std::chrono::steady_clock::time_point connectedSince_;
        std::chrono::steady_clock::time_point lastPollTime_;
        static constexpr double kConnectTimeoutSec = 5.0;

        std::unordered_map<uint32_t, NetworkComponent*> registry_;
        std::unordered_map<uint32_t, DebugEntry>        debugEntries_;

        // Packet counters
        uint64_t totalSent_     = 0;
        uint64_t totalReceived_ = 0;

        // Bandwidth accumulators (reset each sample window)
        uint64_t bytesSentWindow_ = 0;
        uint64_t bytesRecvWindow_ = 0;
        double   bwTimeAccum_     = 0.0;

        BandwidthStats bwStats_;
        std::array<float, kBwHistorySize> sendHistory_ = {};
        std::array<float, kBwHistorySize> recvHistory_ = {};
        int bwHistoryOffset_ = 0;

        void HandlePacket(const uint8_t* data, size_t length);
        void ReleaseAllPhysics();
        void ResetStats();
    };

} // namespace ettycc

#endif
