#include "Camera.h"
#include <gtc\matrix_transform.hpp>
#include <glm.hpp>

namespace ettycc {

    Camera::Camera(int h, int w)
    {
        this->SetOrtho(h, w);
        
    }

    Camera::Camera(int h, int w, float fov,float znear)
    {
        this->SetPrespective(h, w, fov, znear);

        //TransformMatrix = glm::lookAt(glm::vec3(0,0,-5),glm::vec3(0,0,1),glm::vec3(0,1,0));
    }

    Camera::Camera()
    {
    }


    Camera::~Camera()
    {

    }

    glm::mat4 Camera::GetProyectionMatrix()
    {
        return this->ProjectionMatrix;
    }
    //solucionar problema de camara ortografica
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
}
