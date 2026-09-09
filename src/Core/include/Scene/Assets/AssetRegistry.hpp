#pragma once

#include "AssetPool.hpp"
#include "AssetHandle.hpp"
#include "TextureAsset.hpp"
#include "ShaderAsset.hpp"
#include "MaterialAsset.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <future>
#include <memory>
#include <functional>

namespace ettycc
{
    // Pre-loaded image data (CPU side, no GL calls).
    // Used by the async loader: pixel data is read on a worker thread, then
    // uploaded to GL on the main thread.
    struct PreloadedImage
    {
        std::string    path;
        unsigned char* pixels   = nullptr;
        int            width    = 0;
        int            height   = 0;
        int            channels = 0;

        PreloadedImage() = default;
        ~PreloadedImage();

        PreloadedImage(const PreloadedImage&) = delete;
        PreloadedImage& operator=(const PreloadedImage&) = delete;

        PreloadedImage(PreloadedImage&& o) noexcept;
        PreloadedImage& operator=(PreloadedImage&& o) noexcept;
    };

    // Shader source pair (CPU side, for async preloading).
    struct ShaderSource
    {
        std::string vert;
        std::string frag;
    };

    // Central asset registry.
    //
    // Owns all asset pools (textures, shaders, materials) in contiguous,
    // cache-friendly storage.  All assets are eager-initialized at startup
    // and accessed by lightweight handles.
    //
    // Registered as a dependency so any system can resolve it via GetDependency(AssetRegistry).
    class AssetRegistry
    {
    public:
        AssetRegistry() = default;
        ~AssetRegistry() = default;

        // ---- Initialization ------------------------------------------------

        // Must be called before use.
        void Init(const std::string& workingFolder, const std::string& shadersPath);

        // Scan directories and eager-load all known assets.
        // Call after Init(). Loads textures, shaders, and materials.
        void ScanAndLoad(const std::string& imagesRelPath, const std::string& materialsRelPath);

        // ---- Texture API ---------------------------------------------------

        AssetHandle<TextureAsset> GetTexture(const std::string& relativePath);
        AssetHandle<TextureAsset> LoadTextureFromDisk(const std::string& absolutePath, const std::string& relativePath);

        // Upload a pre-loaded image to the GPU and register as a texture asset.
        AssetHandle<TextureAsset> UploadTexture(PreloadedImage& img, const std::string& relativePath);

        // Ensure a texture entry has a valid GL handle; loads pixels + uploads if needed.
        void EnsureGPUTexture(AssetHandle<TextureAsset> h, const std::string& relativePath);

        // Upload all scanned textures that don't have a GL handle yet.
        void UploadAllTextures();

        // Re-apply GL params after editing a texture's descriptor.
        void ReapplyTextureParams(AssetHandle<TextureAsset> h);

        // Save a texture's metadata to its .meta file on disk.
        void SaveTextureMeta(AssetHandle<TextureAsset> h);

        // ---- Shader API ----------------------------------------------------

        AssetHandle<ShaderAsset> GetShader(const std::string& baseName);

        // Async preloading
        static std::future<std::vector<PreloadedImage>> PreloadImagesAsync(
            const std::vector<std::string>& absolutePaths);
        static std::future<std::unordered_map<std::string, ShaderSource>> PreloadShadersAsync(
            const std::string& shadersPath, const std::vector<std::string>& baseNames);
        void UploadShaders(const std::unordered_map<std::string, ShaderSource>& sources);

        // ---- Material API --------------------------------------------------

        AssetHandle<MaterialAsset> GetMaterial(const std::string& relativePath);
        AssetHandle<MaterialAsset> LoadMaterialFromDisk(const std::string& absolutePath, const std::string& relativePath);

        // Get or create a default material for a texture (auto-generated, not persisted).
        AssetHandle<MaterialAsset> GetOrCreateDefaultMaterial(const std::string& textureRelPath);

        // Get or create a procedural material (no texture, shader-only).
        // Uniforms are set on the returned material.
        AssetHandle<MaterialAsset> GetOrCreateProceduralMaterial(
            const std::string& shaderName,
            const std::unordered_map<std::string, UniformValue>& uniforms = {});

        // Save a material to disk.
        void SaveMaterial(AssetHandle<MaterialAsset> h);

        // Resolve a material's texture/shader handles from its path strings.
        // Called after all textures and shaders are loaded.
        void ResolveMaterialHandles(AssetHandle<MaterialAsset> h);

        // ---- Pool access (for iteration, editor, etc.) ---------------------

        AssetPool<TextureAsset>&  Textures()  { return textures_; }
        AssetPool<ShaderAsset>&   Shaders()   { return shaders_; }
        AssetPool<MaterialAsset>& Materials() { return materials_; }

        const AssetPool<TextureAsset>&  Textures()  const { return textures_; }
        const AssetPool<ShaderAsset>&   Shaders()   const { return shaders_; }
        const AssetPool<MaterialAsset>& Materials() const { return materials_; }

        // ---- Legacy compat helpers -----------------------------------------

        // Get GL texture handle directly (for SpriteBatch and old code paths).
        GLuint GetGLTextureHandle(AssetHandle<TextureAsset> h) const;

        // Get GL shader program ID directly.
        GLuint GetGLShaderProgram(AssetHandle<ShaderAsset> h) const;

        // Lookup texture meta by relative path (for old code that uses string paths).
        TextureAsset* GetTextureAssetByPath(const std::string& relativePath);
        const TextureAsset* GetTextureAssetByPath(const std::string& relativePath) const;

        // Image stem -> relative path index (for sprite lookup by name).
        const std::string& GetSpritePath(const std::string& stem) const;
        const std::unordered_map<std::string, std::string>& GetImageIndex() const { return imageIndex_; }

        // Stats
        size_t GetTextureCount()  const { return textures_.Size(); }
        size_t GetShaderCount()   const { return shaders_.Size(); }
        size_t GetMaterialCount() const { return materials_.Size(); }

        const std::string& GetWorkingFolder() const { return workingFolder_; }

        // ---- File utilities ------------------------------------------------

        // Read a text file from disk (no caching -- caller should cache if needed).
        static std::string ReadFile(const std::string& path);

    private:
        std::string workingFolder_;
        std::string shadersPath_;

        AssetPool<TextureAsset>  textures_;
        AssetPool<ShaderAsset>   shaders_;
        AssetPool<MaterialAsset> materials_;

        // stem -> relative path (e.g. "rock" -> "images/rock.png")
        std::unordered_map<std::string, std::string> imageIndex_;
        static GLuint CreateGLTexture(unsigned char* pixels, int w, int h, int channels,
                                       const TextureAsset& meta);

        void ScanImages(const std::string& imagesRelPath);
        void ScanMaterials(const std::string& materialsRelPath);
        void ResolveAllMaterialHandles();
    };

} // namespace ettycc
