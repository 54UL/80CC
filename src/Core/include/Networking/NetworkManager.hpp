#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <enet/enet.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Threading/AsyncQueue.hpp>

#include <array>
#include <atomic>
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
    // Thread-safe design:
    //   • Poll() and SendOutbound() run on the NETWORK WORKER thread.
    //     All ENet calls are confined to that thread.
    //   • QueueBroadcast() is called from the MAIN thread to enqueue packets.
    //   • ApplyInboundTransforms() is called from the MAIN thread to drain
    //     received transform packets and apply them to components.
    //   • The two AsyncQueues provide lock-free SPSC communication.
    class NetworkManager
    {
    public:
        NetworkManager();
        ~NetworkManager();

        bool InitHost  (uint16_t port);
        bool InitClient(const std::string& address, uint16_t port);

        // ── Network-thread API ───────────────────────────────────────────────

        // Service ENet events.  Received transforms are pushed to the inbound
        // queue instead of being applied directly.  Called by network worker.
        void Poll();

        // Drain the outbound queue and send each packet via ENet.
        // Called by the network worker after Poll().
        void SendOutbound();

        // Graceful shutdown — releases physics on all registered components,
        // clears the registry and destroys the ENet host.
        void Shutdown();

        // ── Main-thread API ──────────────────────────────────────────────────

        // Drain the inbound queue and call ApplyRemoteTransform on each
        // matching NetworkComponent.  Call once per frame from the main thread.
        // Returns number of transforms applied.
        size_t ApplyInboundTransforms();

        // Queue a transform packet for sending by the network worker.
        // Called from main thread (NetworkSystem::OnUpdate).
        void QueueBroadcast(uint32_t networkId,
                            const glm::vec3& pos,
                            const glm::quat& rot,
                            const glm::vec3& scale);

        // ── State / accessors ────────────────────────────────────────────────

        NetState    GetState   () const { return state_.load(std::memory_order_acquire); }
        const char* GetStateStr() const;

        bool IsHost  () const { return isHost_;          }
        bool IsActive() const { return host_ != nullptr; }

        // True if the network worker detected a disconnect that the main thread
        // hasn't processed yet.  Main thread should call HandlePendingDisconnect().
        bool HasPendingDisconnect() const { return disconnectPending_.load(std::memory_order_acquire); }
        void HandlePendingDisconnect();

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

        // Legacy synchronous broadcast — still available but prefer QueueBroadcast
        // for thread-safe operation.  Only call from the network thread.
        void BroadcastTransform(uint32_t networkId,
                                const glm::vec3& pos,
                                const glm::quat& rot,
                                const glm::vec3& scale);

    private:
        bool       isHost_     = false;
        ENetHost*  host_       = nullptr;
        ENetPeer*  serverPeer_ = nullptr;

        std::atomic<NetState> state_{NetState::OFFLINE};
        std::atomic<bool>     disconnectPending_{false};

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

        // ── Lock-free queues ─────────────────────────────────────────────────
        // Inbound: network worker → main thread (received transforms)
        AsyncQueue<TransformPacket, 1024> inboundTransforms_;
        // Outbound: main thread → network worker (broadcasts)
        AsyncQueue<TransformPacket, 1024> outboundTransforms_;

        void HandlePacket(const uint8_t* data, size_t length);
        void ReleaseAllPhysics();
        void ResetStats();
    };

} // namespace ettycc

#endif
