#include <Networking/NetworkManager.hpp>
#include <Networking/NetworkComponent.hpp>
#include <spdlog/spdlog.h>

#include <cstring>

namespace ettycc
{
    NetworkManager::NetworkManager()
    {
        lastPollTime_ = std::chrono::steady_clock::now();

        if (enet_initialize() != 0)
            spdlog::error("[NetworkManager] enet_initialize() failed");
        else
            spdlog::info("[NetworkManager] ENet initialized");
    }

    NetworkManager::~NetworkManager()
    {
        Shutdown();
        enet_deinitialize();
    }

    const char* NetworkManager::GetStateStr() const
    {
        switch (GetState())
        {
        case NetState::OFFLINE:      return "Offline";
        case NetState::CONNECTING:   return "Connecting...";
        case NetState::CONNECTED:    return "Connected";
        case NetState::DISCONNECTED: return "Disconnected";
        }
        return "Unknown";
    }

    bool NetworkManager::InitHost(uint16_t port)
    {
        if (host_)
        {
            spdlog::warn("[NetworkManager] Already active — call Shutdown() first");
            return false;
        }

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        host_ = enet_host_create(&address, 32, 2, 0, 0);
        if (!host_)
        {
            spdlog::error("[NetworkManager] Failed to create host on port {}", port);
            state_.store(NetState::DISCONNECTED, std::memory_order_release);
            return false;
        }

        isHost_ = true;
        state_.store(NetState::CONNECTED, std::memory_order_release);
        connectedSince_ = std::chrono::steady_clock::now();
        spdlog::info("[NetworkManager] Host listening on port {}", port);
        return true;
    }

    bool NetworkManager::InitClient(const std::string& address, uint16_t port)
    {
        if (host_)
        {
            spdlog::warn("[NetworkManager] Already active — call Shutdown() first");
            return false;
        }

        host_ = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host_)
        {
            spdlog::error("[NetworkManager] Failed to create client ENet host");
            state_.store(NetState::DISCONNECTED, std::memory_order_release);
            return false;
        }

        ENetAddress addr;
        enet_address_set_host(&addr, address.c_str());
        addr.port = port;

        serverPeer_ = enet_host_connect(host_, &addr, 2, 0);
        if (!serverPeer_)
        {
            spdlog::error("[NetworkManager] No available peer slot for connection");
            enet_host_destroy(host_);
            host_  = nullptr;
            state_.store(NetState::DISCONNECTED, std::memory_order_release);
            return false;
        }

        state_.store(NetState::CONNECTING, std::memory_order_release);
        connectStart_ = std::chrono::steady_clock::now();
        spdlog::info("[NetworkManager] Connecting to {}:{}", address, port);
        return true;
    }

    // ── Network-thread: service ENet events ──────────────────────────────────

    void NetworkManager::Poll()
    {
        if (!host_) return;

        // ── Delta time for bandwidth sampling ─────────────────────────────────
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastPollTime_).count();
        lastPollTime_ = now;

        // ── Connection timeout (client CONNECTING state) ───────────────────────
        if (GetState() == NetState::CONNECTING)
        {
            double elapsed = std::chrono::duration<double>(now - connectStart_).count();
            if (elapsed > kConnectTimeoutSec)
            {
                spdlog::warn("[NetworkManager] Connection timed out after {:.1f}s", elapsed);
                // Can't call full Shutdown from worker — just mark disconnected.
                // Main thread will handle cleanup via HandlePendingDisconnect.
                state_.store(NetState::DISCONNECTED, std::memory_order_release);
                disconnectPending_.store(true, std::memory_order_release);
                return;
            }
        }

        // ── ENet event loop (non-blocking: timeout = 0) ──────────────────────
        ENetEvent event;
        while (enet_host_service(host_, &event, 0) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                state_.store(NetState::CONNECTED, std::memory_order_release);
                connectedSince_ = std::chrono::steady_clock::now();
                spdlog::info("[NetworkManager] Peer connected (id={})",
                             event.peer->incomingPeerID);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                spdlog::warn("[NetworkManager] Peer disconnected (id={})",
                             event.peer->incomingPeerID);
                if (!isHost_)
                {
                    serverPeer_ = nullptr;
                    state_.store(NetState::DISCONNECTED, std::memory_order_release);
                    // Defer physics release to main thread
                    disconnectPending_.store(true, std::memory_order_release);
                }
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                HandlePacket(event.packet->data, event.packet->dataLength);
                enet_packet_destroy(event.packet);
                break;

            default:
                break;
            }
        }

        // ── Bandwidth sampling (one sample per second) ────────────────────────
        auto currentState = GetState();
        if (currentState == NetState::CONNECTED || currentState == NetState::CONNECTING)
        {
            bwTimeAccum_ += dt;

            if (bwTimeAccum_ >= 1.0)
            {
                float sendBps = static_cast<float>(bytesSentWindow_) / static_cast<float>(bwTimeAccum_);
                float recvBps = static_cast<float>(bytesRecvWindow_) / static_cast<float>(bwTimeAccum_);

                bwStats_.sendBps = sendBps;
                bwStats_.recvBps = recvBps;

                sendHistory_[bwHistoryOffset_] = sendBps;
                recvHistory_[bwHistoryOffset_] = recvBps;
                bwHistoryOffset_ = (bwHistoryOffset_ + 1) % kBwHistorySize;

                bytesSentWindow_ = 0;
                bytesRecvWindow_ = 0;
                bwTimeAccum_     = 0.0;
            }
        }

        // ── Live connection duration ───────────────────────────────────────────
        if (currentState == NetState::CONNECTED)
        {
            bwStats_.connectedForSecs =
                std::chrono::duration<double>(now - connectedSince_).count();
        }
    }

    // ── Network-thread: drain outbound queue and send via ENet ───────────────

    void NetworkManager::SendOutbound()
    {
        if (!host_ || GetState() != NetState::CONNECTED) return;

        outboundTransforms_.DrainInto([this](TransformPacket&& pkt) {
            ENetPacket* packet = enet_packet_create(
                &pkt, sizeof(TransformPacket), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);

            if (isHost_)
                enet_host_broadcast(host_, 0, packet);
            else if (serverPeer_)
                enet_peer_send(serverPeer_, 0, packet);

            ++totalSent_;
            bytesSentWindow_        += sizeof(TransformPacket);
            bwStats_.totalBytesSent += sizeof(TransformPacket);

            auto& dbg = debugEntries_[pkt.networkId];
            dbg.pos = { pkt.px, pkt.py, pkt.pz };
            dbg.count++;
        });
    }

    // ── Main-thread: drain inbound queue and apply transforms ────────────────

    size_t NetworkManager::ApplyInboundTransforms()
    {
        return inboundTransforms_.DrainInto([this](TransformPacket&& pkt) {
            auto it = registry_.find(pkt.networkId);
            if (it == registry_.end() || !it->second) return;

            glm::vec3 pos  { pkt.px, pkt.py, pkt.pz };
            glm::quat rot  { pkt.rw, pkt.rx, pkt.ry, pkt.rz };
            glm::vec3 scale{ pkt.sx, pkt.sy, pkt.sz };

            it->second->ApplyRemoteTransform(pos, rot, scale);
        });
    }

    // ── Main-thread: handle deferred disconnect ──────────────────────────────

    void NetworkManager::HandlePendingDisconnect()
    {
        if (!disconnectPending_.load(std::memory_order_acquire)) return;

        spdlog::info("[NetworkManager] Processing deferred disconnect on main thread");
        ReleaseAllPhysics();
        disconnectPending_.store(false, std::memory_order_release);
    }

    // ── Main-thread: queue a broadcast for the network worker ────────────────

    void NetworkManager::QueueBroadcast(uint32_t networkId,
                                        const glm::vec3& pos,
                                        const glm::quat& rot,
                                        const glm::vec3& scale)
    {
        TransformPacket pkt;
        pkt.networkId = networkId;
        pkt.px = pos.x;   pkt.py = pos.y;   pkt.pz = pos.z;
        pkt.rw = rot.w;   pkt.rx = rot.x;   pkt.ry = rot.y;   pkt.rz = rot.z;
        pkt.sx = scale.x; pkt.sy = scale.y; pkt.sz = scale.z;

        if (!outboundTransforms_.TryPush(std::move(pkt)))
            spdlog::warn("[NetworkManager] Outbound queue full — dropping broadcast for id={}", networkId);
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────

    void NetworkManager::Shutdown()
    {
        if (!host_) return;

        ReleaseAllPhysics();
        registry_.clear();
        debugEntries_.clear();
        ResetStats();

        if (serverPeer_)
        {
            enet_peer_disconnect_now(serverPeer_, 0);
            serverPeer_ = nullptr;
        }

        enet_host_destroy(host_);
        host_   = nullptr;
        isHost_ = false;
        state_.store(NetState::OFFLINE, std::memory_order_release);

        spdlog::info("[NetworkManager] Shutdown complete");
    }

    void NetworkManager::ResetStats()
    {
        totalSent_       = 0;
        totalReceived_   = 0;
        bytesSentWindow_ = 0;
        bytesRecvWindow_ = 0;
        bwTimeAccum_     = 0.0;
        bwStats_         = {};
        sendHistory_.fill(0.f);
        recvHistory_.fill(0.f);
        bwHistoryOffset_ = 0;
    }

    // ── Registry ──────────────────────────────────────────────────────────────

    void NetworkManager::Register(uint32_t networkId, NetworkComponent* comp)
    {
        registry_[networkId] = comp;
        spdlog::info("[NetworkManager] Registered networkId={}", networkId);
    }

    void NetworkManager::Unregister(uint32_t networkId)
    {
        registry_.erase(networkId);
    }

    void NetworkManager::ReleaseAllPhysics()
    {
        for (auto& [id, comp] : registry_)
        {
            if (comp) comp->ReleasePhysics();
        }
    }

    // ── Legacy synchronous broadcast (network-thread only) ───────────────────

    void NetworkManager::BroadcastTransform(uint32_t networkId,
                                            const glm::vec3& pos,
                                            const glm::quat& rot,
                                            const glm::vec3& scale)
    {
        if (!host_ || GetState() != NetState::CONNECTED) return;

        TransformPacket pkt;
        pkt.networkId = networkId;
        pkt.px = pos.x;   pkt.py = pos.y;   pkt.pz = pos.z;
        pkt.rw = rot.w;   pkt.rx = rot.x;   pkt.ry = rot.y;   pkt.rz = rot.z;
        pkt.sx = scale.x; pkt.sy = scale.y; pkt.sz = scale.z;

        ENetPacket* packet = enet_packet_create(&pkt, sizeof(TransformPacket),
                                                ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
        if (isHost_)
            enet_host_broadcast(host_, 0, packet);
        else if (serverPeer_)
            enet_peer_send(serverPeer_, 0, packet);

        ++totalSent_;
        bytesSentWindow_        += sizeof(TransformPacket);
        bwStats_.totalBytesSent += sizeof(TransformPacket);

        auto& dbg = debugEntries_[networkId];
        dbg.pos = pos;
        dbg.count++;
    }

    // ── Receive (called on network thread) ───────────────────────────────────

    void NetworkManager::HandlePacket(const uint8_t* data, size_t length)
    {
        if (length < 1) return;

        switch (data[0])
        {
        case NET_MSG_TRANSFORM:
        {
            if (length < sizeof(TransformPacket)) return;

            TransformPacket pkt;
            std::memcpy(&pkt, data, sizeof(TransformPacket));

            // Push to inbound queue — main thread will apply via ApplyInboundTransforms()
            if (!inboundTransforms_.TryPush(pkt))
                spdlog::warn("[NetworkManager] Inbound queue full — dropping transform for id={}", pkt.networkId);

            ++totalReceived_;
            bytesRecvWindow_            += length;
            bwStats_.totalBytesReceived += length;

            auto& dbg = debugEntries_[pkt.networkId];
            dbg.pos = { pkt.px, pkt.py, pkt.pz };
            dbg.count++;

            // Host relays to all other clients (ENet call, safe on network thread)
            if (isHost_)
            {
                ENetPacket* relay = enet_packet_create(
                    data, length, ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
                enet_host_broadcast(host_, 0, relay);
            }
            break;
        }
        default:
            spdlog::warn("[NetworkManager] Unknown packet msgType={}", data[0]);
            break;
        }
    }

} // namespace ettycc
