
#include <Graphics/Rendering.hpp>
#include <spdlog/spdlog.h>

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

        for (auto renderable : renderables_)
            renderable->Pass(this->renderingCtx_, deltaTime);

        if (sceneFrameBuffer_)
            sceneFrameBuffer_->EndFrame();
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

    void Rendering::ClearRenderables()
    {
        renderables_.clear();
    }
}
