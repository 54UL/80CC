#ifndef RENDERING_HPP
#define RENDERING_HPP

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
        void Pass();


        // auto InstanceEntity(std::shared<RenderingEntity> entity) -> void;
        // auto GetEntity(uint64_t id) -> std::shared<RenderingEntity>;
        // auto GetEntities() -> std::vector<std::shared<RenderingEntity>>;

  
    };

}

#endif