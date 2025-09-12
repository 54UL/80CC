#ifndef ENGINE_PIPELINE_HPP
#define ENGINE_PIPELINE_HPP
#include <Input/PlayerInput.hpp>

namespace ettycc
{
    class EnginePipeline
    {
    public:
        EnginePipeline() = default;
        virtual ~EnginePipeline() = default;

        virtual void          Init() = 0;
        virtual void          Update() = 0;
        virtual void          PrepareFrame() = 0;
        virtual void          PresentFrame() = 0;
        virtual PlayerInput * GetInputSystem() = 0;
    };
} // namespace ettycc

#endif