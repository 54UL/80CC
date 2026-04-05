#ifndef RESOURCE_CACHE_HPP
#define RESOURCE_CACHE_HPP

#include <Graphics/Shading/ShaderPipeline.hpp>

#include <GL/glew.h>
#include <GL/gl.h>
#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <future>

namespace ettycc
{
    // ── Cached shader program ────────────────────────────────────────────────
    // One compiled GL program shared by all renderables that use the same
    // vertex/fragment pair (e.g. every Sprite shares a single "sprite" program).
    struct CachedShader
    {
        ShaderPipeline pipeline;
        GLuint         programId = 0;
    };

    // ── Cached GPU texture ───────────────────────────────────────────────────
    // One GL texture object shared by all renderables that reference the same
    // image file on disk.
    struct CachedTexture
    {
        GLuint handle   = 0;
        int    width    = 0;
        int    height   = 0;
        int    channels = 0;
    };

    // ── Pre-loaded image data (CPU side, no GL calls) ────────────────────────
    // Used by the async loader: pixel data is read on a worker thread, then
    // uploaded to GL on the main thread.
    struct PreloadedImage
    {
        std::string    path;
        unsigned char* pixels   = nullptr;
        int            width    = 0;
        int            height   = 0;
        int            channels = 0;

        // Destructor and move ops defined in ResourceCache.cpp (needs stb_image)
        PreloadedImage() = default;
        ~PreloadedImage();

        PreloadedImage(const PreloadedImage&) = delete;
        PreloadedImage& operator=(const PreloadedImage&) = delete;

        PreloadedImage(PreloadedImage&& o) noexcept;
        PreloadedImage& operator=(PreloadedImage&& o) noexcept;
    };

    // ── ResourceCache ────────────────────────────────────────────────────────
    // Central cache for GPU resources.  Owned by Engine, accessible via the
    // Dependency system.  All GL calls happen on the main thread; disk I/O
    // can be offloaded with PreloadImagesAsync().
    class ResourceCache
    {
    public:
        ResourceCache() = default;
        ~ResourceCache() = default;

        // Must be called before use — stores the base shaders path.
        void Init(const std::string& shadersPath);

        // ── Shader API ───────────────────────────────────────────────────────
        // Returns a shared, compiled shader program for the given base name
        // (e.g. "sprite" -> loads sprite.vert + sprite.frag once).
        // Thread-safe: only the first call compiles; subsequent calls return
        // the cached program.
        std::shared_ptr<CachedShader> GetShader(const std::string& baseName);

        // ── Texture API ──────────────────────────────────────────────────────
        // Returns a shared GL texture for the given absolute file path.
        // Loads from disk + uploads to GPU on first call; returns cached
        // handle afterwards.
        GLuint GetTexture(const std::string& absolutePath);

        // Upload a PreloadedImage (pixels already in RAM) to the GPU.
        // Returns the GL texture handle and caches it.
        GLuint UploadTexture(PreloadedImage& img);

        // ── Async preloading ─────────────────────────────────────────────────
        // Reads image files from disk on a background thread.
        // Returns a future that resolves to a vector of PreloadedImages.
        // Caller must then call UploadTexture() on the main thread for each.
        static std::future<std::vector<PreloadedImage>> PreloadImagesAsync(
            const std::vector<std::string>& absolutePaths);

        // Preload shader source files from disk (CPU only, no GL).
        // Returns map of baseName -> {vertSource, fragSource}.
        struct ShaderSource { std::string vert; std::string frag; };
        static std::future<std::unordered_map<std::string, ShaderSource>> PreloadShadersAsync(
            const std::string& shadersPath,
            const std::vector<std::string>& baseNames);

        // Finalize preloaded shaders on the main thread (compiles GL programs).
        void UploadShaders(const std::unordered_map<std::string, ShaderSource>& sources);

        // ── Stats ────────────────────────────────────────────────────────────
        size_t GetCachedShaderCount()  const { return shaders_.size(); }
        size_t GetCachedTextureCount() const { return textures_.size(); }

    private:
        std::string shadersPath_;

        std::unordered_map<std::string, std::shared_ptr<CachedShader>> shaders_;
        std::unordered_map<std::string, CachedTexture>                 textures_;

        // Helpers
        static std::string ReadFile(const std::string& path);
        static GLuint      CreateGLTexture(unsigned char* pixels, int w, int h, int channels);
    };

} // namespace ettycc

#endif // RESOURCE_CACHE_HPP
