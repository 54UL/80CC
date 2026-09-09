#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Frustum.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <Engine.hpp>

#include "Input/Controls/EditorCamera.hpp"

namespace ettycc
{
    Camera::Camera()
    {
        renderableType = Type::Camera;
    }

    Camera::Camera(int w, int h)
    {
        renderableType = Type::Camera;
        this->SetOrtho(w, h);
        Init(w, h);
    }

    Camera::Camera(int w, int h, float fov, float znear)
    {
        renderableType = Type::Camera;
        this->SetPerspective(w, h, fov, znear);
        Init(w, h);
    }

    Camera::~Camera() = default;

    void Camera::Init(int w, int h)
    {
        this->offScreenFrameBuffer = std::make_shared<FrameBuffer>(glm::ivec2(0, 0), glm::ivec2(w, h), false);
    }

    glm::mat4 Camera::GetProjectionMatrix() const
    {
        return this->ProjectionMatrix;
    }

    void Camera::SetOrtho(int ScreenXSz, int ScreenYSz)
    {
        ispresp = false;
        this->ProjectionMatrix = glm::mat4(1.0f);
        this->ProjectionMatrix = glm::ortho(-1, 1, -1, 1, 1, 20);
    }

    void Camera::SetPerspective(int ScreenXSz, int ScreenYSz, float FOV, float Znear)
    {
        ispresp = true;
        this->ProjectionMatrix = glm::perspective(glm::radians(FOV), (float) ScreenXSz / (float) ScreenYSz, Znear,
                                                  500.0f);
    }

    bool Camera::isSetPerspective() const {
        return ispresp;
    }

    void Camera::AttachEditorControl(PlayerInput *inputSystem)
    {
        editorCameraControl_ = std::make_shared<EditorCamera>(inputSystem, this->offScreenFrameBuffer.get());
        // Bind the renderable's own transform so the control operates on it
        editorCameraControl_->BindTransform(&transform());
    }

    // Renderable
    void Camera::Init(const std::shared_ptr<Engine> &engineCtx)
    {
        // Init frame buffer backend (if deserialized then there's already populated data so might run???)
        if (offScreenFrameBuffer && initializable_)
        {
            spdlog::info("Initializing scene camera");
            offScreenFrameBuffer->Init();
            initializable_ = false;
            initialized = true;
        }
    }

    void Camera::Pass(const std::shared_ptr<RenderingContext> &ctx, float deltaTime)
    {
        // editor camera control logic (should not be here...)
        if (editorCameraControl_ != nullptr)
        {
            editorCameraControl_->Update(deltaTime);
            ctx->Projection = editorCameraControl_->ComputeProjectionMatrix(deltaTime);
            ctx->View =  editorCameraControl_->ComputeViewMatrix(deltaTime);
        }
        else
        {
            ctx->Projection = this->ProjectionMatrix;
            ctx->View = this->transform().GetMatrix();
        }

        // Compute frustum planes from the combined PV matrix
        if (useFrustumOverride_)
            ctx->frustum = frustumOverride_;
        else if (frustumCullingEnabled_)
            ctx->frustum = Frustum::FromPV(ctx->Projection * ctx->View);
        else
            ctx->frustum.enabled = false;
    }

    void Camera::Inspect(EditorPropertyVisitor& v)
    {
        Renderable::Inspect(v);
        PROP_SECTION("Camera");
        PROP(ispresp, "Perspective");
        PROP(frustumCullingEnabled_, "Frustum Culling");
        PROP(depth, "Depth");
        PROP(viewportRect, "Viewport Rect");

        // Clear flags combo
        {
            static const char* clearFlagLabels[] = { "Solid Color", "Depth Only", "Nothing" };
            int idx = static_cast<int>(clearFlags);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Clear Flags");
            ImGui::SameLine(ImMax(80.f, ImGui::GetContentRegionAvail().x * 0.38f));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##ClearFlags", &idx, clearFlagLabels, 3))
                clearFlags = static_cast<ClearFlags>(idx);
        }

        PROP_COLOR(clearColor, "Clear Color");

        // Culling mask -- checkboxes for each active layer
        {
            auto& layerConfig = GetDependency(Engine)->renderLayerConfig_;
            int layerCount = layerConfig.GetActiveCount();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Culling Mask");
            ImGui::SameLine(ImMax(80.f, ImGui::GetContentRegionAvail().x * 0.38f));

            // Preview label: show names of enabled layers
            std::string preview;
            for (int i = 0; i < layerCount; ++i)
            {
                if (cullingMask & (1u << i))
                {
                    if (!preview.empty()) preview += ", ";
                    preview += layerConfig.GetName(i);
                }
            }
            if (preview.empty()) preview = "(none)";

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##CullingMask", preview.c_str()))
            {
                for (int i = 0; i < layerCount; ++i)
                {
                    bool bit = (cullingMask & (1u << i)) != 0;
                    if (ImGui::Checkbox(layerConfig.GetName(i).c_str(), &bit))
                    {
                        if (bit) cullingMask |= (1u << i);
                        else     cullingMask &= ~(1u << i);
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
}
