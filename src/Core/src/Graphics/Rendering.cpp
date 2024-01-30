
#include <Graphics/Rendering.hpp>

namespace ettycc
{
    Rendering::Rendering()
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
    static float timedemo=0;
    void Rendering::Pass()
    {
        timedemo+=0.01f;//todo: calculate via delta time...
        for (auto renderable : renderables_)
        {
            renderable->Pass(this->renderingCtx_,timedemo);
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
