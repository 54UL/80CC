#ifndef RENDERING_CAMERA_HPP
#define RENDERING_CAMERA_HPP
#include <glm/glm.hpp>
#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/FrameBuffer.hpp>

namespace ettycc
{
	class Camera: public Renderable
	{
	private:
		bool ispresp;
		glm::mat4 ProjectionMatrix;
	public:
		std::shared_ptr<FrameBuffer> offScreenFrameBuffer;

	public:
		Camera();
		Camera(int w, int h);
		Camera(int w, int h, float fov, float znear);
		~Camera();

	public:
		glm::mat4 GetProjectionMatrix();
		void SetOrtho(int ScreenXSz, int ScreenYSz);
		void SetPrespective(int ScreenXSz, int ScreenYSz, float FOV, float Znear);
		bool isSetPrespective();

		// Renderable impl
	public:
		void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;

		// Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(ispresp));
        }
	};
} // namespace ettycc


#endif