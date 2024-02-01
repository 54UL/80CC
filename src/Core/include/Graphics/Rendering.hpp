#ifndef RENDERING_HPP
#define RENDERING_HPP
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/FrameBuffer.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>

#include <memory>
#include <cstdint>
#include <vector>


namespace ettycc
{
    class Rendering
    {
    private:
        std::vector<std::shared_ptr<Renderable>> renderables_;
        std::shared_ptr<RenderingContext> renderingCtx_;
        float renderingTime;
        std::shared_ptr<FrameBuffer> sceneFrameBuffer_;

    public:
        Rendering();
        ~Rendering();

    public:
        void SetScreenSize(int width, int height);
        void InitGraphicsBackend();
        
        void SetViewPortFrameBuffer(std::shared_ptr<FrameBuffer> frameBuffer);
        std::shared_ptr<FrameBuffer> GetViewPortFrameBuffer();

        void Pass(float deltaTime);
        auto AddRenderable(std::shared_ptr<Renderable> renderable) -> void;
        void AddRenderables(const std::vector<std::shared_ptr<Renderable>>& renderables);
    };

}

#endif