
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <Engine.hpp>

namespace ettycc {

    Camera::Camera()
    {
        this->mainCamera_ = false;
    }

    Camera::Camera(int h, int w)
    {
        this->SetOrtho(h, w);  
        this->mainCamera_ = false;
    }

    Camera::Camera(int w, int h, float fov,float znear)
    {
        this->SetPrespective(h, w, fov, znear);
        this->offScreenFrameBuffer = std::make_shared<FrameBuffer>(glm::ivec2(0,0), glm::ivec2(w,h), false);
        this->mainCamera_ = false;
    }

    Camera::~Camera()
    {

    }

    glm::mat4 Camera::GetProjectionMatrix()
    {
        return this->ProjectionMatrix;
    }

    void Camera::SetOrtho(int ScreenXSz, int ScreenYSz)
    {
        //	this->ProjectionMatrix = glm::mat4(1.0f);
        ispresp = false;
        // this->ProjectionMatrix = glm::ortho(1, ScreenXSz, 1, ScreenYSz,1, 20);
    }

    void Camera::SetPrespective(int ScreenXSz, int ScreenYSz, float FOV, float Znear)
    {
        ispresp = true;
        this->ProjectionMatrix = glm::perspective(glm::radians(FOV), (float)ScreenXSz / (float)ScreenYSz, Znear, 500.0f);
    }

    bool Camera::isSetPrespective()
    {
        return ispresp;
    }

    // Renderable
    void Camera::Init(const std::shared_ptr<Engine>& engineCtx) 
    {
        // Init frame buffer backend (if deserialized then there's already populated data so might run???)
        if (offScreenFrameBuffer && initializable_)
        {
            spdlog::info("Initializing seeded camera");
            offScreenFrameBuffer->Init();

            // convention just to make this camera the current editor viewport
            if (mainCamera_) 
            {
                //TODO: move SetViewPortFrameBuffer above....
                spdlog::info("Setting as view port camera...");
                engineCtx->renderEngine_.SetViewPortFrameBuffer(offScreenFrameBuffer);
            }

            initializable_ = false;
            initialized = true;
        }
        else
        {
            spdlog::info("Empty camera init");
        }
    }

    void Camera::Pass(const std::shared_ptr<RenderingContext> &ctx, float time)
    {
        ctx->Projection = this->ProjectionMatrix;
        ctx->View = this->underylingTransform.GetMatrix();
    }
}
