#include <Scene/Assets/ResourceCache.hpp>
#include <Graphics/Shading/Shader.hpp>
#include <stb_image.h>
#include <fstream>
#include <sstream>

namespace ettycc
{
    // ── PreloadedImage ───────────────────────────────────────────────────────
    PreloadedImage::~PreloadedImage()
    {
        if (pixels) stbi_image_free(pixels);
    }

    PreloadedImage::PreloadedImage(PreloadedImage&& o) noexcept
        : path(std::move(o.path)), pixels(o.pixels)
        , width(o.width), height(o.height), channels(o.channels)
    {
        o.pixels = nullptr;
    }

    PreloadedImage& PreloadedImage::operator=(PreloadedImage&& o) noexcept
    {
        if (this != &o) {
            if (pixels) stbi_image_free(pixels);
            path = std::move(o.path); pixels = o.pixels;
            width = o.width; height = o.height; channels = o.channels;
            o.pixels = nullptr;
        }
        return *this;
    }

    // ── Init ─────────────────────────────────────────────────────────────────
    void ResourceCache::Init(const std::string& shadersPath)
    {
        shadersPath_ = shadersPath;
        spdlog::info("[ResourceCache] initialized — shaders path: {}", shadersPath_);
    }

    // ── Shader cache ─────────────────────────────────────────────────────────
    std::shared_ptr<CachedShader> ResourceCache::GetShader(const std::string& baseName)
    {
        auto it = shaders_.find(baseName);
        if (it != shaders_.end())
            return it->second;

        // First request — compile from disk
        auto vertSrc = ReadFile(shadersPath_ + baseName + ".vert");
        auto fragSrc = ReadFile(shadersPath_ + baseName + ".frag");

        if (vertSrc.empty() || fragSrc.empty())
        {
            spdlog::error("[ResourceCache] Failed to load shader '{}'", baseName);
            return nullptr;
        }

        auto cached = std::make_shared<CachedShader>();
        cached->pipeline.AddShaders({
            std::make_shared<Shader>(vertSrc.c_str(), GL_VERTEX_SHADER),
            std::make_shared<Shader>(fragSrc.c_str(), GL_FRAGMENT_SHADER)
        });
        cached->pipeline.Create();
        cached->programId = cached->pipeline.GetProgramId();

        shaders_[baseName] = cached;
        spdlog::info("[ResourceCache] Compiled shader '{}' (program {})", baseName, cached->programId);
        return cached;
    }

    // ── Texture cache ────────────────────────────────────────────────────────
    GLuint ResourceCache::GetTexture(const std::string& absolutePath)
    {
        auto it = textures_.find(absolutePath);
        if (it != textures_.end())
            return it->second.handle;

        // First request — load from disk and upload
        int w, h, ch;
        unsigned char* pixels = stbi_load(absolutePath.c_str(), &w, &h, &ch, 0);
        if (!pixels)
        {
            spdlog::error("[ResourceCache] Failed to load image '{}': {}",
                          absolutePath, stbi_failure_reason());
            return 0;
        }

        GLuint handle = CreateGLTexture(pixels, w, h, ch);
        stbi_image_free(pixels);

        textures_[absolutePath] = CachedTexture{handle, w, h, ch};
        spdlog::info("[ResourceCache] Loaded texture '{}' ({}x{}, {} ch, GL {})",
                     absolutePath, w, h, ch, handle);
        return handle;
    }

    GLuint ResourceCache::UploadTexture(PreloadedImage& img)
    {
        // Check cache first — another path may have already uploaded this
        auto it = textures_.find(img.path);
        if (it != textures_.end())
            return it->second.handle;

        if (!img.pixels)
        {
            spdlog::error("[ResourceCache] UploadTexture called with null pixels for '{}'", img.path);
            return 0;
        }

        GLuint handle = CreateGLTexture(img.pixels, img.width, img.height, img.channels);
        textures_[img.path] = CachedTexture{handle, img.width, img.height, img.channels};

        // Free CPU-side pixels now that they're on the GPU
        stbi_image_free(img.pixels);
        img.pixels = nullptr;

        spdlog::info("[ResourceCache] Uploaded texture '{}' ({}x{}, GL {})",
                     img.path, img.width, img.height, handle);
        return handle;
    }

    // ── Async preloading (CPU only — no GL calls) ────────────────────────────
    std::future<std::vector<PreloadedImage>> ResourceCache::PreloadImagesAsync(
        const std::vector<std::string>& absolutePaths)
    {
        return std::async(std::launch::async, [absolutePaths]() -> std::vector<PreloadedImage>
        {
            std::vector<PreloadedImage> results;
            results.reserve(absolutePaths.size());

            for (const auto& path : absolutePaths)
            {
                PreloadedImage img;
                img.path   = path;
                img.pixels = stbi_load(path.c_str(), &img.width, &img.height, &img.channels, 0);

                if (!img.pixels)
                    spdlog::warn("[ResourceCache::Async] Failed to preload '{}': {}", path, stbi_failure_reason());

                results.push_back(std::move(img));
            }

            spdlog::info("[ResourceCache::Async] Preloaded {} images on background thread", results.size());
            return results;
        });
    }

    std::future<std::unordered_map<std::string, ResourceCache::ShaderSource>>
    ResourceCache::PreloadShadersAsync(
        const std::string& shadersPath,
        const std::vector<std::string>& baseNames)
    {
        return std::async(std::launch::async, [shadersPath, baseNames]()
            -> std::unordered_map<std::string, ShaderSource>
        {
            std::unordered_map<std::string, ShaderSource> results;
            for (const auto& name : baseNames)
            {
                ShaderSource src;
                src.vert = ReadFile(shadersPath + name + ".vert");
                src.frag = ReadFile(shadersPath + name + ".frag");
                if (!src.vert.empty() && !src.frag.empty())
                    results[name] = std::move(src);
                else
                    spdlog::warn("[ResourceCache::Async] Failed to preload shader '{}'", name);
            }
            spdlog::info("[ResourceCache::Async] Preloaded {} shader sources", results.size());
            return results;
        });
    }

    void ResourceCache::UploadShaders(const std::unordered_map<std::string, ShaderSource>& sources)
    {
        for (const auto& [name, src] : sources)
        {
            if (shaders_.count(name))
                continue; // already compiled

            auto cached = std::make_shared<CachedShader>();
            cached->pipeline.AddShaders({
                std::make_shared<Shader>(src.vert.c_str(), GL_VERTEX_SHADER),
                std::make_shared<Shader>(src.frag.c_str(), GL_FRAGMENT_SHADER)
            });
            cached->pipeline.Create();
            cached->programId = cached->pipeline.GetProgramId();
            shaders_[name] = cached;

            spdlog::info("[ResourceCache] Uploaded shader '{}' (program {})", name, cached->programId);
        }
    }

    // ── Helpers ──────────────────────────────────────────────────────────────
    std::string ResourceCache::ReadFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            spdlog::error("[ResourceCache] Failed to open file: {}", path);
            return {};
        }
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    GLuint ResourceCache::CreateGLTexture(unsigned char* pixels, int w, int h, int channels)
    {
        GLuint handle = 0;
        glGenTextures(1, &handle);
        glBindTexture(GL_TEXTURE_2D, handle);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format),
                     w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
        return handle;
    }

} // namespace ettycc
