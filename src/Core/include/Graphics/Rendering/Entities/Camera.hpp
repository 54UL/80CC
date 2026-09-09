#ifndef RENDERING_CAMERA_HPP
#define RENDERING_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/FrameBuffer.hpp>
#include <Graphics/Rendering/Frustum.hpp>
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
		bool frustumCullingEnabled_ = true;
		// When set, Camera::Pass uses this frustum instead of computing from
		// its own PV matrix.  Used by the editor to preview scene-camera culling.
		bool  useFrustumOverride_ = false;
		Frustum frustumOverride_;
		glm::mat4 ProjectionMatrix{};
		std::shared_ptr<FrameBuffer> offScreenFrameBuffer;

		// Multi-camera support: lower depth renders first (background).
		// Cameras with depth >= 0 are game cameras; the editor camera uses a
		// special negative depth so it never interferes.
		int depth = 0;

		// Normalized viewport rectangle (x, y, w, h) in [0,1].
		// Full-screen camera = (0,0,1,1).  Smaller rects enable split-screen
		// or picture-in-picture overlays.
		glm::vec4 viewportRect = glm::vec4(0.f, 0.f, 1.f, 1.f);

		// How the camera clears before rendering.
		enum class ClearFlags : int { SolidColor = 0, DepthOnly = 1, Nothing = 2 };
		ClearFlags clearFlags = ClearFlags::SolidColor;

		// Background clear color (used when clearFlags == SolidColor).
		glm::vec4 clearColor = glm::vec4(0.12f, 0.12f, 0.12f, 1.f);

		// Layer culling mask -- a camera only renders objects whose layer
		// bit is set in this mask.  Default = all layers.
		uint32_t cullingMask = 0xFFFFFFFF;

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
		void Inspect(EditorPropertyVisitor& v) override;

		// Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(offScreenFrameBuffer),
               CEREAL_NVP(ispresp),
               CEREAL_NVP(frustumCullingEnabled_),
               CEREAL_NVP(depth),
               CEREAL_NVP(viewportRect),
               CEREAL_NVP(clearFlags),
               CEREAL_NVP(clearColor),
               CEREAL_NVP(cullingMask));
        }
	};
} // namespace ettycc


#endif
