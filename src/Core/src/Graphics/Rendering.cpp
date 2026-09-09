
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <Input/Controls/EditorCamera.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

        // --- Run the camera that drives this pass ---
        // Editor mode: dedicated editor camera (lives outside renderables).
        // Standalone: first enabled Camera in renderables sets the matrices.
        if (editorCamera_ && editorCamera_->enabled)
        {
            editorCamera_->Pass(renderingCtx_, deltaTime);
        }
        else
        {
            // Standalone fallback: first camera in renderables
            for (auto& r : renderables_)
            {
                if (!r->enabled) continue;
                if (r->renderableType == Renderable::Type::Camera)
                {
                    static_cast<Camera*>(r.get())->Pass(renderingCtx_, deltaTime);
                    break;
                }
            }
        }

        // Editor grid (if any)
        if (editorGrid_ && editorGrid_->enabled)
            editorGrid_->Pass(renderingCtx_, deltaTime);

        spriteBatch_.Begin(renderingCtx_, deltaTime);
        SubmitRenderables(deltaTime);
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
        SubmitRenderables(deltaTime);
        spriteBatch_.End();
        fbo->EndFrame();

        // Restore editor state
        renderingCtx_->Projection = prevProj;
        renderingCtx_->View       = prevView;
        renderingCtx_->frustum    = prevFrustum;
    }

    void Rendering::RenderGameView(const std::shared_ptr<FrameBuffer>& fbo,
                                    float deltaTime)
    {
        if (!fbo || !spriteBatchReady_) return;

        // Collect enabled scene cameras from renderables (editor camera is
        // separate and never in this list).
        std::vector<Camera*> cameras;
        for (auto& r : renderables_)
        {
            if (!r->enabled) continue;
            if (r->renderableType == Renderable::Type::Camera)
                cameras.push_back(static_cast<Camera*>(r.get()));
        }
        if (cameras.empty()) return;

        // Sort by depth (lower depth = rendered first = background)
        std::sort(cameras.begin(), cameras.end(),
                  [](const Camera* a, const Camera* b) { return a->depth < b->depth; });

        // Save editor state
        const glm::mat4 prevProj    = renderingCtx_->Projection;
        const glm::mat4 prevView    = renderingCtx_->View;
        const Frustum   prevFrustum = renderingCtx_->frustum;

        const glm::ivec2 fboSize = fbo->GetSize();

        // Bind FBO manually (don't use BeginFrame -- it clears with hardcoded color)
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->GetId());
        glViewport(0, 0, fboSize.x, fboSize.y);
        glEnable(GL_DEPTH_TEST);

        // Clear entire FBO with the first camera's clear color
        {
            const auto& cc = cameras.front()->clearColor;
            glClearColor(cc.r, cc.g, cc.b, cc.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        for (Camera* cam : cameras)
        {
            // Compute viewport in pixels from normalized rect
            const glm::vec4& vr = cam->viewportRect;
            GLint   vx = static_cast<GLint>(vr.x * fboSize.x);
            GLint   vy = static_cast<GLint>(vr.y * fboSize.y);
            GLsizei vw = static_cast<GLsizei>(vr.z * fboSize.x);
            GLsizei vh = static_cast<GLsizei>(vr.w * fboSize.y);
            if (vw <= 0 || vh <= 0) continue;
            glViewport(vx, vy, vw, vh);
            glScissor(vx, vy, vw, vh);
            glEnable(GL_SCISSOR_TEST);

            // Per-camera clear
            switch (cam->clearFlags)
            {
            case Camera::ClearFlags::SolidColor:
            {
                const auto& cc = cam->clearColor;
                glClearColor(cc.r, cc.g, cc.b, cc.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                break;
            }
            case Camera::ClearFlags::DepthOnly:
                glClear(GL_DEPTH_BUFFER_BIT);
                break;
            case Camera::ClearFlags::Nothing:
                break;
            }

            // Compute camera matrices using the viewport aspect ratio
            const float vpAspect = static_cast<float>(vw) / static_cast<float>(vh);
            glm::mat4 proj, view;
            if (cam->editorCameraControl_)
            {
                // Recompute ortho projection with viewport-correct aspect
                const float halfH = EditorCamera::baseSize_ / cam->editorCameraControl_->zoom;
                const float halfW = halfH * vpAspect;
                proj = glm::ortho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);
                view = cam->editorCameraControl_->ComputeViewMatrix(deltaTime);
            }
            else
            {
                // Rebuild projection with correct aspect for this viewport
                if (cam->isSetPerspective())
                    proj = glm::perspective(glm::radians(45.0f), vpAspect, 0.1f, 500.0f);
                else
                    proj = glm::ortho(-vpAspect, vpAspect, -1.0f, 1.0f, -100.0f, 100.0f);
                view = cam->transform().GetMatrix();
            }

            renderingCtx_->Projection = proj;
            renderingCtx_->View       = view;
            if (cam->frustumCullingEnabled_)
                renderingCtx_->frustum = Frustum::FromPV(proj * view);
            else
                renderingCtx_->frustum.enabled = false;

            const uint32_t mask = cam->cullingMask;

            spriteBatch_.Begin(renderingCtx_, deltaTime);
            SubmitRenderables(deltaTime, mask);
            spriteBatch_.End();
        }

        glDisable(GL_SCISSOR_TEST);
        // Restore full viewport
        glViewport(0, 0, fboSize.x, fboSize.y);

        fbo->EndFrame();

        // Restore editor state
        renderingCtx_->Projection = prevProj;
        renderingCtx_->View       = prevView;
        renderingCtx_->frustum    = prevFrustum;
    }

    void Rendering::SetEditorOverlay(std::shared_ptr<Renderable> camera,
                                      std::shared_ptr<Renderable> grid)
    {
        editorCamera_ = std::move(camera);
        editorGrid_   = std::move(grid);
    }

    void Rendering::ClearEditorOverlay()
    {
        editorCamera_.reset();
        editorGrid_.reset();
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

    void Rendering::SubmitRenderables(float deltaTime, uint32_t layerMask)
    {
        for (auto& renderable : renderables_)
        {
            if (!renderable->enabled) continue;

            switch (renderable->renderableType)
            {
            case Renderable::Type::Camera:
            case Renderable::Type::Grid:
                continue;
            case Renderable::Type::Sprite:
            {
                if ((renderable->layer & layerMask) == 0) continue;
                auto* sprite = static_cast<Sprite*>(renderable.get());
                if (renderingCtx_->frustum.enabled)
                {
                    const glm::vec3 pos   = sprite->transform().getGlobalPosition();
                    const glm::vec3 scale = sprite->transform().getGlobalScale();
                    if (!renderingCtx_->frustum.IsVisible(pos, scale.x, scale.y))
                        continue;
                }
                spriteBatch_.Submit(sprite);
                continue;
            }
            default:
                if ((renderable->layer & layerMask) == 0) continue;
                renderable->Pass(renderingCtx_, deltaTime);
                continue;
            }
        }
    }
}
