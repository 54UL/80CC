
#include <Graphics/Rendering.hpp>
#include "Rendering.hpp"

namespace ettycc
{
    Rendering::Rendering()
    {
        this->renderingCtx_ = std::make_shared<RenderingContext>();
    }

    Rendering::~Rendering()
    {
    }

    void SetScreenSize(int width, int height)
    {
        this->renderingCtx_->ScreenSize = glm::vec2(widht,height);
    }

    void Rendering::InitGraphicsBackend()
    {
        
    }

    void Rendering::Pass()
    {
        for (auto renderable : renderables_)
        {
            renderable.Pass();
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
