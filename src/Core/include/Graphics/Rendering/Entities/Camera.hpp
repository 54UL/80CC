#ifndef RENDERING_CAMERA_HPP
#define RENDERING_CAMERA_HPP
#include <glm/glm.hpp>
#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Renderable.hpp>

namespace ettycc
{
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

   		void Camera::SetTransform(const Transform &trans);
		Transform Camera::GetTransform();

		// Renderable impl
	public:
		void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;
	};
} // namespace ettycc


#endif