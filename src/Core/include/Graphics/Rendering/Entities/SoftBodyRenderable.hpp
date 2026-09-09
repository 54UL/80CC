#ifndef SOFT_BODY_RENDERABLE_HPP
#define SOFT_BODY_RENDERABLE_HPP

#include <Graphics/Rendering/Renderable.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Scene/Assets/AssetHandle.hpp>
#include <Scene/Assets/ShaderAsset.hpp>
#include <Scene/Assets/TextureAsset.hpp>
#include <Physics/IPhysicsSoftBody.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

#include <GL/glew.h>
#include <GL/gl.h>
#include <spdlog/spdlog.h>

namespace ettycc
{
    class SoftBodyRenderable : public Renderable
    {
        static constexpr const char* kShaderName = "softbody";

    public:
        SoftBodyRenderable(std::string texPath,
                           physics::IPhysicsSoftBody* body,
                           std::vector<float>  uvs,
                           std::vector<int>    indices);

        ~SoftBodyRenderable() override;

        // Renderable interface
        void Init(const std::shared_ptr<Engine>& engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float deltaTime) override;
        void Inspect(EditorPropertyVisitor& v) override;

        void DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                           GLuint program, uint32_t id) override;

        // Call before destroying the physics body to avoid dangling pointer.
        void ClearBody() { body_ = nullptr; }

        // Double-buffer: physics thread writes to back buffer, then swaps.
        // Call from physics/main thread after Step() completes.
        void SyncFromPhysics();

    public:
        glm::vec2 tiling { 1.0f, 1.0f };

    private:
        GLuint VAO_     = 0;
        GLuint VBO_     = 0;
        GLuint EBO_     = 0;
        GLuint TEXTURE_ = 0;

        AssetHandle<ShaderAsset>  shaderHandle_;
        AssetHandle<TextureAsset> textureHandle_;

        std::string                texturePath_;
        physics::IPhysicsSoftBody* body_       = nullptr;  // non-owning
        std::vector<int>           indices_;
        int                        numVerts_   = 0;

        // Double-buffered vertex data: physics writes to back, render reads from front.
        // Swap is done atomically via index flip after physics completes.
        std::vector<float>         vertexBuffers_[2];
        std::atomic<int>           frontBuffer_{0};     // index the render thread reads
        int                        backBuffer_ = 1;     // index the physics thread writes
    };

} // namespace ettycc

#endif
