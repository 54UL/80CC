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

    typedef struct 
    {
        uint64_t id;
        std::string name;
        bool enabled;
        ProcessingChannel processingChannel;
    } NodeComponentInfo;

    namespace Utils
    {
        // temporal hack to define consistent incremental ids across all the components...
        static uint64_t currentComponentIndex = 1;
        static uint64_t GetNextIncrementalId()
        {
            return currentComponentIndex++;
        }
    }
} // namespace ettcc
#endif