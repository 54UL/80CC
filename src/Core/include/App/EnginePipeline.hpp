#ifndef ENGINE_PIPELINE_HPP
#define ENGINE_PIPELINE_HPP
#include <Input/PlayerInput.hpp>

namespace ettycc
{
    class EnginePipeline
    {
    public:
        virtual void Init() = 0;
        virtual void Update() = 0;
        virtual void PrepareFrame() = 0;
        virtual void PresentFrame() = 0;
        virtual void ProcessInput(PlayerInputType type, uint64_t *data) = 0; // TODO: UNSAFE CODE, SET FIXED LENGTHS
    };
} // namespace ettycc

#endif