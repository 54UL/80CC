#ifndef RENDERING_HPP
#define RENDERING_HPP

#include <memory>
#include <cstdint>
#include <vector>
#include "Renderable.hpp"
#include "RenderingContext.hpp"

namespace etycc
{
    class Rendering
    {
    private:
        std::vector<std::shared_ptr<Renderable>> renderables_;
        std::shared_ptr<RenderingContext> renderingCtx_;
        
    public:
        Rendering();
        ~Rendering();

    public:
        void SetScreenSize(int width, int height);
        void InitGraphicsBackend();

        void Pass();
        auto AddRenderable(std::shared_ptr<Renderable> renderable) -> void;
        void AddRenderables(const std::vector<std::shared_ptr<Renderable>>& renderables);
    };

}

#endif