#ifndef RENDERING_HPP
#define RENDERING_HPP
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/FrameBuffer.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <Graphics/Rendering/SpriteBatch.hpp>

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
        SpriteBatch spriteBatch_;
        bool spriteBatchReady_ = false;

    public:
        Rendering();
        ~Rendering();

    public:
        void SetScreenSize(int width, int height);
        void Init();
        
        void SetViewPortFrameBuffer(std::shared_ptr<FrameBuffer> frameBuffer);
        std::shared_ptr<FrameBuffer> GetViewPortFrameBuffer();

        const std::vector<std::shared_ptr<Renderable>>& GetRenderables() const;
        std::shared_ptr<RenderingContext> GetRenderingContext() const;

        void Pass(float deltaTime);

        // Render all non-camera renderables into a separate FBO with the given
        // projection/view matrices.  Used by the game-view preview so it can
        // show the scene from a scene camera independent of the editor camera.
        void RenderToTarget(const std::shared_ptr<FrameBuffer>& fbo,
                            const glm::mat4& proj,
                            const glm::mat4& view,
                            float deltaTime);

        auto AddRenderable(std::shared_ptr<Renderable> renderable) -> void;
        void AddRenderables(const std::vector<std::shared_ptr<Renderable>>& renderables);
        void RemoveRenderable(const std::shared_ptr<Renderable>& renderable);
        void ClearRenderables();
        // Move renderable to position 0 so it runs before all others (e.g. camera).
        // If not already in the list it is inserted at the front.
        void EnsureFirst(const std::shared_ptr<Renderable>& renderable);
    };
}

#endif