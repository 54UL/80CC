#ifndef PICK_BUFFER_HPP
#define PICK_BUFFER_HPP

#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace ettycc
{
    class Renderable;
    class Sprite;

    // Completely separate from the main render pipeline.
    // Renders each renderable with a unique flat color (encoded object ID)
    // into its own RGBA8 FBO. Use ReadPixel() to identify what is under the cursor.
    //
    // Sprites are batched using instanced rendering (one draw call per shape preset).
    class PickerBuffer
    {
    public:
        glm::ivec2 size_;
        bool       initialized_ = false;

        PickerBuffer(glm::ivec2 size);
        ~PickerBuffer();

        // Call once after GL is ready and shader path is known
        void Init();
        void InitShader(const std::string& shadersPath);

        // Render all renderables into the picker FBO with ID colours
        void RenderPass(const std::shared_ptr<RenderingContext>& ctx,
                        const std::vector<std::shared_ptr<Renderable>>& renderables);

        // Read one pixel and decode to object index (0 = background)
        uint32_t ReadPixel(int x, int y) const;

        // RGBA8 texture — can be displayed directly as an ImGui::Image
        GLuint GetTextureId() const;

        void SetSize(glm::ivec2 size);
        void CleanUp();

    private:
        GLuint fbo_       = 0;
        GLuint texture_   = 0;  // GL_RGBA8 color attachment
        GLuint depthRbo_  = 0;  // depth renderbuffer

        // ── Legacy per-object shader (for non-sprite renderables) ───────────
        GLuint program_   = 0;  // picker shader program
        GLint  pvmLoc_     = -1;
        GLint  idColorLoc_ = -1;

        // ── Instanced picker shader (for batched sprites) ───────────────────
        GLuint instancedProgram_ = 0;
        GLint  instancedPVLoc_   = -1;

        // ── Per-instance data for picker batching ───────────────────────────
        struct alignas(16) PickerInstanceData
        {
            glm::mat4 model;      // 64 bytes
            glm::vec3 idColor;    // 12 bytes
            float     _pad;       // 4 bytes → 80 bytes total
        };

        // ── Shared preset geometry (one VAO per shape preset) ───────────────
        struct PresetGeo
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            int    indexCount = 0;
        };

        std::unordered_map<int, PresetGeo> presetGeos_;
        void BuildPresetGeo(int preset);
        PresetGeo& GetPresetGeo(int preset);

        // ── Instance VBO (reused each frame) ────────────────────────────────
        GLuint instanceVBO_ = 0;
        size_t instanceVBOCapacity_ = 0;
        void EnsureInstanceVBO(size_t count);
        void SetupInstanceAttributes(GLuint vao);

        // ── Per-frame batch collection ──────────────────────────────────────
        struct PickerBatch
        {
            int shapePreset;
            std::vector<PickerInstanceData> instances;
        };

        // Custom geometry sprites rendered individually
        struct CustomPickerEntry
        {
            Sprite*            sprite;
            PickerInstanceData instance;
        };

        // ── Shader compilation helpers ──────────────────────────────────────
        GLuint CompileShader(const std::string& src, GLenum type) const;
        std::string LoadFile(const std::string& path) const;

        // Golden-ratio hue spread — gives bright, visually distinct colors for small IDs
        static glm::vec3 IdToColor(uint32_t id);
        // Encode packed RGB to lookup key
        static uint32_t  PackColor(uint8_t r, uint8_t g, uint8_t b);

        // Rebuilt every RenderPass: packed RGB -> object ID
        mutable std::unordered_map<uint32_t, uint32_t> colorToId_;
    };
}

#endif
