#ifndef SCENE_API_HPP
#define SCENE_API_HPP

#include <cstdint>
#include <string>

namespace ettycc
{
    enum class ProcessingChannel: uint64_t
    {
        MAIN,
        RENDERING,
        AUDIO
    };

    struct NodeComponentInfo
    {
        uint64_t id;
        std::string name;
        bool enabled;
        ProcessingChannel processingChannel;
    };

    namespace Utils
    {
        // Returns a reference to the shared entity ID counter so that both
        // GetNextEntityId() and FastForwardEntityCounter() use the same value.
        inline uint64_t& EntityCounter()
        {
            static uint64_t counter = 1;
            return counter;
        }

        // Allocates the next unique entity ID.
        inline uint64_t GetNextEntityId()
        {
            return EntityCounter()++;
        }

        // Call after deserializing a scene: ensures the counter is strictly
        // above every ID that was loaded, preventing new nodes from getting
        // IDs that collide with existing ones.
        inline void FastForwardEntityCounter(uint64_t pastId)
        {
            if (EntityCounter() <= pastId)
                EntityCounter() = pastId + 1;
        }

        // Legacy alias — kept so any remaining code compiles.
        inline uint64_t GetNextIncrementalId()
        {
            return GetNextEntityId();
        }
    }
} // namespace ettcc
#endif