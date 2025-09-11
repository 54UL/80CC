#ifndef RENDERING_CAMERA_HPP
#define RENDERING_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/FrameBuffer.hpp>
#include <Input/PlayerInput.hpp>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
	class EditorCamera;

	class Camera: public Renderable
	{
	public:
		std::shared_ptr<EditorCamera> editorCameraControl_;

		bool ispresp{};
		glm::mat4 ProjectionMatrix{};
		std::shared_ptr<FrameBuffer> offScreenFrameBuffer;
		bool mainCamera_;// no need to have this on all cameras...

	public:
		Camera();
		Camera(int w, int h);

		void Init(int w, int h);

		Camera(int w, int h, float fov, float znear);
		~Camera() override;

	public:
		glm::mat4 GetProjectionMatrix() const;
		void SetOrtho(int ScreenXSz, int ScreenYSz);
		void SetPerspective(int ScreenXSz, int ScreenYSz, float FOV, float Znear);
		bool isSetPerspective() const;
		void AttachEditorControl(PlayerInput *inputSystem);
		// Renderable impl
	public:
		void Init(const std::shared_ptr<Engine>& engineCtx) override;
		void Pass(const std::shared_ptr<RenderingContext>& ctx, float deltaTime) override;

		// Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this), CEREAL_NVP(offScreenFrameBuffer), CEREAL_NVP(ispresp));
        }
	};
} // namespace ettycc


#endif