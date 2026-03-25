#ifndef SOFT_BODY_RENDERABLE_HPP
#define SOFT_BODY_RENDERABLE_HPP

#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Shading/ShaderPipeline.hpp>
#include <Dependency.hpp>
#include <Dependencies/Resources.hpp>

#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GL/gl.h>
#include <spdlog/spdlog.h>

#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBody.h>

namespace ettycc
{
    class SoftBodyRenderable : public Renderable
    {
        const std::string shaderBaseName_ = "softbody";

    public:
        SoftBodyRenderable(std::string texPath,
                           btSoftBody*         body,
                           std::vector<float>  uvs,
                           std::vector<int>    indices);

        ~SoftBodyRenderable() override;

        // Renderable interface
        void Init(const std::shared_ptr<Engine>& engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float deltaTime) override;
        void Inspect(EditorPropertyVisitor& v) override;

        // DrawForPicker: no-op for soft bodies
        void DrawForPicker(const std::shared_ptr<RenderingContext>& /*ctx*/,
                           GLuint /*program*/, uint32_t /*id*/) override {}

    public:
        ShaderPipeline shader_;

    private:
        GLuint VAO_     = 0;
        GLuint VBO_     = 0;
        GLuint EBO_     = 0;
        GLuint TEXTURE_ = 0;

        std::string         texturePath_;
        btSoftBody*         body_       = nullptr;  // non-owning
        std::vector<float>  vertexBuffer_;           // x,y,z,u,v per vertex
        std::vector<int>    indices_;
        int                 numVerts_   = 0;

        std::string LoadShaderFile(const std::string& path);
        void        LoadShaders();
        void        LoadTexture();
    };

} // namespace ettycc

#endif
