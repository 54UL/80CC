
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Frustum.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace ettycc
{
    Rendering::Rendering(): renderingTime(0)
    {
        this->renderingCtx_ = std::make_shared<RenderingContext>();
        renderables_ = std::vector<std::shared_ptr<Renderable>>();
    }

    Rendering::~Rendering()
    {
    }

    void Rendering::SetScreenSize(int width, int height)
    {
        this->renderingCtx_->ScreenSize = glm::vec2(width, height);
    }

    void Rendering::Init()
    {
        spdlog::info("scene init");
    }

    void Rendering::SetViewPortFrameBuffer(std::shared_ptr<FrameBuffer> frameBuffer)
    {
        sceneFrameBuffer_ = frameBuffer;
    }

    std::shared_ptr<FrameBuffer> Rendering::GetViewPortFrameBuffer()
    {
        return sceneFrameBuffer_;
    }

    void Rendering::Pass(float deltaTime)
    {
        static float accumulatedTime = 0;
        accumulatedTime += deltaTime;

        // FBO is optional: editor sets one (viewport texture); standalone renders
        // directly to GL 0 (the window), which SDL2App already bound in PrepareFrame.
        if (sceneFrameBuffer_)
            sceneFrameBuffer_->BeginFrame();

        // Lazy-init SpriteBatch on first frame (GL context must be ready)
        if (!spriteBatchReady_)
        {
            spriteBatch_.Init();
            spriteBatchReady_ = true;
        }

        spriteBatch_.Begin(renderingCtx_, deltaTime);

        for (auto& renderable : renderables_)
        {
            if (!renderable->enabled) continue;

            // Route Sprite renderables through the batch
            auto* sprite = dynamic_cast<Sprite*>(renderable.get());
            if (sprite)
            {
                // Frustum culling — skip sprites entirely outside the view
                if (renderingCtx_->frustum.enabled)
                {
                    const glm::vec3 pos   = sprite->underylingTransform.getGlobalPosition();
                    const glm::vec3 scale = sprite->underylingTransform.getGlobalScale();
                    if (!renderingCtx_->frustum.IsVisible(pos, scale.x, scale.y))
                        continue;
                }

                spriteBatch_.Submit(sprite);
                continue;
            }

            // Non-sprite renderables (Camera, Grid, etc.) use the old path
            renderable->Pass(this->renderingCtx_, deltaTime);
        }

        spriteBatch_.End();

        if (sceneFrameBuffer_)
            sceneFrameBuffer_->EndFrame();
    }

    void Rendering::RenderToTarget(const std::shared_ptr<FrameBuffer>& fbo,
                                    const glm::mat4& proj,
                                    const glm::mat4& view,
                                    float deltaTime)
    {
        if (!fbo || !spriteBatchReady_) return;

        // Save the editor state so we can restore it after.
        const glm::mat4 prevProj    = renderingCtx_->Projection;
        const glm::mat4 prevView    = renderingCtx_->View;
        const Frustum   prevFrustum = renderingCtx_->frustum;

        // Set game camera matrices and recompute frustum for this target view
        renderingCtx_->Projection = proj;
        renderingCtx_->View       = view;
        renderingCtx_->frustum    = Frustum::FromPV(proj * view);

        fbo->BeginFrame();

        spriteBatch_.Begin(renderingCtx_, deltaTime);

        for (auto& renderable : renderables_)
        {
            if (!renderable->enabled) continue;

            // Skip cameras — we already set matrices manually.
            if (dynamic_cast<Camera*>(renderable.get())) continue;

            auto* sprite = dynamic_cast<Sprite*>(renderable.get());
            if (sprite)
            {
                if (renderingCtx_->frustum.enabled)
                {
                    const glm::vec3 pos   = sprite->underylingTransform.getGlobalPosition();
                    const glm::vec3 scale = sprite->underylingTransform.getGlobalScale();
                    if (!renderingCtx_->frustum.IsVisible(pos, scale.x, scale.y))
                        continue;
                }
                spriteBatch_.Submit(sprite);
                continue;
            }

            renderable->Pass(renderingCtx_, deltaTime);
        }

        spriteBatch_.End();
        fbo->EndFrame();

        // Restore editor state
        renderingCtx_->Projection = prevProj;
        renderingCtx_->View       = prevView;
        renderingCtx_->frustum    = prevFrustum;
    }

    void Rendering::EnsureFirst(const std::shared_ptr<Renderable>& renderable)
    {
        auto it = std::find(renderables_.begin(), renderables_.end(), renderable);
        if (it != renderables_.end())
        {
            if (it != renderables_.begin())
            {
                renderables_.erase(it);
                renderables_.insert(renderables_.begin(), renderable);
            }
        }
        else
        {
            renderables_.insert(renderables_.begin(), renderable);
        }
    }

    const std::vector<std::shared_ptr<Renderable>>& Rendering::GetRenderables() const
    {
        return renderables_;
    }

    std::shared_ptr<RenderingContext> Rendering::GetRenderingContext() const
    {
        return renderingCtx_;
    }

    auto Rendering::AddRenderable(std::shared_ptr<Renderable> renderable) -> void
    {
        renderables_.emplace_back(renderable);
    }

    void Rendering::AddRenderables(const std::vector<std::shared_ptr<Renderable>> &renderables)
    {
        renderables_.insert(renderables_.end(), renderables.begin(), renderables.end());
    }

    void Rendering::RemoveRenderable(const std::shared_ptr<Renderable>& renderable)
    {
        renderables_.erase(
            std::remove(renderables_.begin(), renderables_.end(), renderable),
            renderables_.end());
    }

    void Rendering::ClearRenderables()
    {
        renderables_.clear();
    }
}
