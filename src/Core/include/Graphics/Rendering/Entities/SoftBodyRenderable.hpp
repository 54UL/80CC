#ifndef SOFT_BODY_RENDERABLE_HPP
#define SOFT_BODY_RENDERABLE_HPP

#include <Graphics/Rendering/Renderable.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Scene/Assets/ResourceCache.hpp>
#include <Physics/IPhysicsSoftBody.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

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

    public:
        glm::vec2 tiling { 1.0f, 1.0f };

    private:
        GLuint VAO_     = 0;
        GLuint VBO_     = 0;
        GLuint EBO_     = 0;
        GLuint TEXTURE_ = 0;

        std::shared_ptr<CachedShader> cachedShader_;

        std::string                texturePath_;
        physics::IPhysicsSoftBody* body_       = nullptr;  // non-owning
        std::vector<float>         vertexBuffer_;
        std::vector<int>           indices_;
        int                        numVerts_   = 0;
    };

} // namespace ettycc

#endif
