#include <cstring>
#include <iostream>
#include <thread>
#include <enet/enet.h>

// Define a custom RPC identifier
#define RPC_MY_CUSTOM_PROCEDURE 1

void serverThread()
{
    if (enet_initialize() != 0)
    {
        std::cerr << "Failed to initialize ENet." << std::endl;
        return;
    }

    atexit(enet_deinitialize);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1234;

    ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr)
    {
        std::cerr << "Failed to create ENet server host." << std::endl;
        return;
    }

    std::cout << "Server started. Waiting for connections..." << std::endl;

    ENetEvent event;
    while (true)
    {
        while (enet_host_service(server, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "Client connected." << std::endl;
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "Client disconnected." << std::endl;
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                std::cout << "RPC called from client." << std::endl;

                // Process the RPC
                if (event.packet->dataLength >= sizeof(uint8_t))
                {
                    uint8_t rpcID;
                    memcpy(&rpcID, event.packet->data, sizeof(uint8_t));

                    switch (rpcID)
                    {
                    case RPC_MY_CUSTOM_PROCEDURE:
                        std::cout << "Custom RPC received." << std::endl;
                        // Process the RPC payload here
                        break;

                    default:
                        std::cout << "Unknown RPC received." << std::endl;
                        break;
                    }
                }

                enet_packet_destroy(event.packet);
                break;

            default:
                break;
            }
        }
    }
}

void clientThread()
{
    if (enet_initialize() != 0)
    {
        std::cerr << "Failed to initialize ENet." << std::endl;
        return;
    }

    atexit(enet_deinitialize);

    ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr)
    {
        std::cerr << "Failed to create ENet client host." << std::endl;
        return;
    }

    ENetAddress address;
    ENetPeer *peer;
    enet_address_set_host(&address, "localhost");
    address.port = 1234;
    peer = enet_host_connect(client, &address, 2, 0);

    if (peer == nullptr)
    {
        std::cerr << "No available peers for initiating an ENet connection." << std::endl;
        return;
    }

    ENetEvent event;
    while (enet_host_service(client, &event, 1000) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            std::cout << "Connected to server." << std::endl;

            // Send an RPC to the server
            {
                uint8_t rpcID = RPC_MY_CUSTOM_PROCEDURE;
                ENetPacket *packet = enet_packet_create(&rpcID, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, 0, packet);
            }

            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            std::cout << "Disconnected from server." << std::endl;
            return;

        default:
            break;
        }
    }
}

int main()
{
    std::thread server(serverThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait for the server to start
    std::thread client(clientThread);

    server.join();
    client.join();

    return 0;
}
