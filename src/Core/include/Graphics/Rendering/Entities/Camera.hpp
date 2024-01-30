#ifndef RENDERING_CAMERA_HPP
#define RENDERING_CAMERA_HPP
#include <glm/glm.h>
#include <Scene/Transform.hpp>
#include "../Renderable.hpp"

class Camera: public Renderable
{
private:
	bool ispresp;
	glm::mat4 ProjectionMatrix;
	
public:
	Camera();
	Camera(int h, int w);
	Camera(int h, int w, float fov, float znear);
	~Camera();

public:
	glm::mat4 GetProjectionMatrix();
	void SetOrtho(int ScreenXSz, int ScreenYSz);
	void SetPrespective(int ScreenXSz, int ScreenYSz, float FOV, float Znear);
	bool isSetPrespective();

	// Renderable impl
public:
    void Pass(const std::shared_ptr<RenderingContext>& ctx) override;
};

#endif