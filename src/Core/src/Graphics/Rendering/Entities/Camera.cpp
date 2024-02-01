
#include <Graphics/Rendering/Entities/Camera.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace ettycc {

    Camera::Camera()
    {

    }

    Camera::Camera(int h, int w)
    {
        this->SetOrtho(h, w);  
    }

    Camera::Camera(int w, int h, float fov,float znear)
    {
        this->SetPrespective(h, w, fov, znear);
        this->offScreenFrameBuffer = std::make_shared<FrameBuffer>(glm::ivec2(0,0),glm::ivec2(w,h), false);
        //TransformMatrix = glm::lookAt(glm::vec3(0,0,-5),glm::vec3(0,0,1),glm::vec3(0,1,0));
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
        //this->ProjectionMatrix = glm::infinitePerspective(glm::radians(FOV), (float)ScreenXSz / (float)ScreenYSz, Znear);
        // this->ProjectionMatrix = glm::mat4(1.0f);
        this->ProjectionMatrix = glm::perspective(glm::radians(FOV), (float)ScreenXSz / (float)ScreenYSz, Znear, 500.0f);
    }

    bool Camera::isSetPrespective()
    {
        return ispresp;
    }

    // Renderable

    // void Camera::SetTransform(const Transform &trans)
    // {
    //     this->underylingTransform = trans;
    // } 
    
    // Transform Camera::GetTransform()
    // {
    //     return this->underylingTransform;
    // }

    void Camera::Pass(const std::shared_ptr<RenderingContext> &ctx, float time)
    {
        ctx->Projection = this->ProjectionMatrix;
        ctx->View = this->underylingTransform.GetMatrix();
    }
}
