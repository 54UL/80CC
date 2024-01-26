#ifndef RENDERING_HPP
#define RENDERING_HPP

// #include <std>
#include <memory>
#include <cstdint>
#include <vector>

namespace etycc
{
    class Rendering
    {
    public:
        enum class PrimitiveTye
        {
            CAMERA,
            QUAD,
            SPRITE,
            MESH,
            CUSTOM
        };

        class RenderingEntity
        {
        public:
            RenderingEntity() {}
            ~RenderingEntity() {}
        };

    public:
        Rendering();
        ~Rendering();

    public:
        void InitBackend();
        void AddShaders(/*array of shaders (define a type)*/);
        void CompileShaders();

        auto InstanceEntity(std::shared<RenderingEntity> entity) -> void;
        auto GetEntity(uint64_t id) -> std::shared<RenderingEntity>;
        auto GetEntities() -> std::vector<std::shared<RenderingEntity>>;

        void Pass();
    };

}

#endif