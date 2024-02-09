
#include <Graphics/Rendering.hpp>

namespace ettycc
{
    Rendering::Rendering(): renderingTime(0)
    {
        this->renderingCtx_ = std::make_shared<RenderingContext>();
    }

    Rendering::~Rendering()
    {
    }

    void Rendering::SetScreenSize(int width, int height)
    {
        this->renderingCtx_->ScreenSize = glm::vec2(width, height);
    }

    void Rendering::InitGraphicsBackend()
    {
        
    }

    void Rendering::SetViewPortFrameBuffer(std::shared_ptr<FrameBuffer> frameBuffer)
    {
        sceneFrameBuffer_ = frameBuffer;
        sceneFrameBuffer_->Init();
    }

    std::shared_ptr<FrameBuffer> Rendering::GetViewPortFrameBuffer()
    {
        return sceneFrameBuffer_;
    }

    void Rendering::Pass(float deltaTime)
    {
        // renderingTime += deltaTime;

        // Frame buffer Single camera  implementation (Get all the cameras instead and then do the framebuffer pass then pass the propper camera model and view matrixes to each viewport...)
        sceneFrameBuffer_->BeginFrame();
        for (auto renderable : renderables_)
        {
            renderable->Pass(this->renderingCtx_, deltaTime);
        }
        
        sceneFrameBuffer_->EndFrame();
    }

    auto Rendering::AddRenderable(std::shared_ptr<Renderable> renderable) -> void
    {
        renderables_.emplace_back(renderable);
    }

    void Rendering::AddRenderables(const std::vector<std::shared_ptr<Renderable>> &renderables)
    {
        renderables_.insert(renderables_.end(), renderables.begin(), renderables.end());
    }
}
