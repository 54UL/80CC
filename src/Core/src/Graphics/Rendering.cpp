
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

    void Rendering::Pass(float deltaTime)
    {
        // renderingTime += deltaTime;

        for (auto renderable : renderables_)
        {
            renderable->Pass(this->renderingCtx_, deltaTime);
        }
    }

    auto Rendering::AddRenderable(std::shared_ptr<Renderable> renderable) -> void
    {
        renderables_.emplace_back(renderable);
    }

    void Rendering::AddRenderables(const std::vector<std::shared_ptr<Renderable>> &renderables)
    {
        renderables_.insert(renderables_.begin(), renderables.begin(), renderables.end());
    }
}
