#include <Scene/Assets/AssetRegistry.hpp>
#include <Graphics/Shading/Shader.hpp>
#include <stb_image.h>

#include <fstream>
#include <sstream>
#include <set>
#include <filesystem>

namespace ettycc
{

// -- PreloadedImage -----------------------------------------------------------

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

// -- Init ---------------------------------------------------------------------

void AssetRegistry::Init(const std::string& workingFolder, const std::string& shadersPath)
{
    workingFolder_ = workingFolder;
    shadersPath_   = shadersPath;
    spdlog::info("[AssetRegistry] Initialized -- working: {}, shaders: {}", workingFolder_, shadersPath_);
}

void AssetRegistry::ScanAndLoad(const std::string& imagesRelPath, const std::string& materialsRelPath)
{
    ScanImages(imagesRelPath);       // 1. Scan metadata (.meta files) -- no GL calls
    ScanMaterials(materialsRelPath);  // 2. Load .material files
    ResolveAllMaterialHandles();      // 3. Link materials → texture/shader handles
    // NOTE: GPU upload (UploadAllTextures) is NOT called here.
    // The Engine calls it explicitly when a GL context is available.
    spdlog::info("[AssetRegistry] Scanned {} textures, {} shaders, {} materials",
                 textures_.Size(), shaders_.Size(), materials_.Size());
}

// -- Texture API --------------------------------------------------------------

AssetHandle<TextureAsset> AssetRegistry::GetTexture(const std::string& relativePath)
{
    auto h = textures_.Find(relativePath);
    if (h.IsValid())
    {
        EnsureGPUTexture(h, relativePath);
        return h;
    }

    // Not in pool yet -- load from disk
    std::string absPath = workingFolder_ + relativePath;
    return LoadTextureFromDisk(absPath, relativePath);
}

AssetHandle<TextureAsset> AssetRegistry::LoadTextureFromDisk(const std::string& absolutePath,
                                                              const std::string& relativePath)
{
    // If already in the pool with a valid GL handle, nothing to do
    auto existing = textures_.Find(relativePath);
    if (existing.IsValid())
    {
        auto* tex = textures_.Get(existing);
        if (tex && tex->glHandle != 0)
            return existing;

        // Entry exists from ScanImages (metadata only) -- upload to GPU now
        EnsureGPUTexture(existing, relativePath);
        return existing;
    }

    // Not in pool at all -- full load from disk
    int w, h, ch;
    unsigned char* pixels = stbi_load(absolutePath.c_str(), &w, &h, &ch, 0);
    if (!pixels)
    {
        spdlog::error("[AssetRegistry] Failed to load image '{}': {}", absolutePath, stbi_failure_reason());
        return AssetHandle<TextureAsset>::Null();
    }

    TextureAsset tex;
    tex.width    = w;
    tex.height   = h;
    tex.channels = ch;
    tex.sourceWidth  = w;
    tex.sourceHeight = h;

    std::string metaPath = absolutePath + ".meta";
    tex.LoadMeta(metaPath);

    tex.glHandle = CreateGLTexture(pixels, w, h, ch, tex);
    stbi_image_free(pixels);

    auto handle = textures_.Add(relativePath, std::move(tex));
    spdlog::info("[AssetRegistry] Loaded texture '{}' ({}x{}, GL {})",
                 relativePath, w, h, textures_.Get(handle)->glHandle);
    return handle;
}

AssetHandle<TextureAsset> AssetRegistry::UploadTexture(PreloadedImage& img, const std::string& relativePath)
{
    if (!img.pixels)
    {
        spdlog::error("[AssetRegistry] UploadTexture called with null pixels for '{}'", relativePath);
        return AssetHandle<TextureAsset>::Null();
    }

    // If entry already exists (from ScanImages), merge the GL data into it
    auto existing = textures_.Find(relativePath);
    if (existing.IsValid())
    {
        auto* tex = textures_.Get(existing);
        if (tex)
        {
            if (tex->glHandle != 0)
            {
                // Already uploaded -- free the preloaded pixels and return
                stbi_image_free(img.pixels);
                img.pixels = nullptr;
                return existing;
            }

            // Merge: keep existing metadata (filter, wrap, PPU from .meta),
            // fill in runtime fields and create the GL texture
            tex->width    = img.width;
            tex->height   = img.height;
            tex->channels = img.channels;
            tex->glHandle = CreateGLTexture(img.pixels, img.width, img.height, img.channels, *tex);

            stbi_image_free(img.pixels);
            img.pixels = nullptr;

            spdlog::info("[AssetRegistry] Uploaded texture '{}' ({}x{}, GL {})",
                         relativePath, img.width, img.height, tex->glHandle);
            return existing;
        }
    }

    // Not in pool -- create a fresh entry
    TextureAsset tex;
    tex.width    = img.width;
    tex.height   = img.height;
    tex.channels = img.channels;
    tex.sourceWidth  = img.width;
    tex.sourceHeight = img.height;

    std::string metaPath = img.path + ".meta";
    tex.LoadMeta(metaPath);

    tex.glHandle = CreateGLTexture(img.pixels, img.width, img.height, img.channels, tex);

    stbi_image_free(img.pixels);
    img.pixels = nullptr;

    auto handle = textures_.Add(relativePath, std::move(tex));
    spdlog::info("[AssetRegistry] Uploaded texture '{}' ({}x{}, GL {})",
                 relativePath, img.width, img.height, textures_.Get(handle)->glHandle);
    return handle;
}

void AssetRegistry::EnsureGPUTexture(AssetHandle<TextureAsset> h, const std::string& relativePath)
{
    auto* tex = textures_.Get(h);
    if (!tex || tex->glHandle != 0) return;

    std::string absPath = workingFolder_ + relativePath;
    int w, h2, ch;
    unsigned char* pixels = stbi_load(absPath.c_str(), &w, &h2, &ch, 0);
    if (!pixels)
    {
        spdlog::error("[AssetRegistry] Failed to load '{}' for GPU upload: {}",
                      absPath, stbi_failure_reason());
        return;
    }

    tex->width    = w;
    tex->height   = h2;
    tex->channels = ch;
    tex->glHandle = CreateGLTexture(pixels, w, h2, ch, *tex);
    stbi_image_free(pixels);

    spdlog::info("[AssetRegistry] GPU upload '{}' ({}x{}, GL {})",
                 relativePath, w, h2, tex->glHandle);
}

void AssetRegistry::UploadAllTextures()
{
    size_t uploaded = 0;
    textures_.ForEach([&](AssetHandle<TextureAsset> h, TextureAsset& tex)
    {
        if (tex.glHandle != 0) return; // already on GPU

        const std::string& relPath = textures_.GetPath(h);
        if (relPath.empty()) return;

        EnsureGPUTexture(h, relPath);
        ++uploaded;
    });

    if (uploaded > 0)
        spdlog::info("[AssetRegistry] UploadAllTextures: {} textures uploaded to GPU", uploaded);
}

void AssetRegistry::ReapplyTextureParams(AssetHandle<TextureAsset> h)
{
    auto* tex = textures_.Get(h);
    if (tex) tex->ApplyGLParams();
}

void AssetRegistry::SaveTextureMeta(AssetHandle<TextureAsset> h)
{
    auto* tex = textures_.Get(h);
    if (!tex) return;

    const std::string& relPath = textures_.GetPath(h);
    if (relPath.empty()) return;

    std::string absMetaPath = workingFolder_ + relPath + ".meta";
    // Normalize separators
    for (char& c : absMetaPath)
        if (c == '/') c = std::filesystem::path::preferred_separator;

    tex->SaveMeta(absMetaPath);
}

// -- Shader API ---------------------------------------------------------------

AssetHandle<ShaderAsset> AssetRegistry::GetShader(const std::string& baseName)
{
    auto h = shaders_.Find(baseName);
    if (h.IsValid()) return h;

    // Compile from disk
    auto vertSrc = ReadFile(shadersPath_ + baseName + ".vert");
    auto fragSrc = ReadFile(shadersPath_ + baseName + ".frag");

    if (vertSrc.empty() || fragSrc.empty())
    {
        spdlog::error("[AssetRegistry] Failed to load shader '{}'", baseName);
        return AssetHandle<ShaderAsset>::Null();
    }

    ShaderAsset sa;
    sa.vertSource = vertSrc;
    sa.fragSource = fragSrc;
    sa.pipeline.AddShaders({
        std::make_shared<Shader>(vertSrc.c_str(), GL_VERTEX_SHADER),
        std::make_shared<Shader>(fragSrc.c_str(), GL_FRAGMENT_SHADER)
    });
    sa.pipeline.Create();
    sa.programId = sa.pipeline.GetProgramId();
    sa.Introspect();

    auto handle = shaders_.Add(baseName, std::move(sa));
    auto* compiled = shaders_.Get(handle);
    spdlog::info("[AssetRegistry] Compiled shader '{}' (program {}, {} uniforms, {} user uniforms, {} attribs)",
                 baseName, compiled->programId,
                 compiled->uniforms.size(), compiled->userUniforms.size(),
                 compiled->attribs.size());
    return handle;
}

std::future<std::vector<PreloadedImage>> AssetRegistry::PreloadImagesAsync(
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
                spdlog::warn("[AssetRegistry::Async] Failed to preload '{}': {}", path, stbi_failure_reason());
            results.push_back(std::move(img));
        }
        spdlog::info("[AssetRegistry::Async] Preloaded {} images", results.size());
        return results;
    });
}

std::future<std::unordered_map<std::string, ShaderSource>>
AssetRegistry::PreloadShadersAsync(const std::string& shadersPath,
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
                spdlog::warn("[AssetRegistry::Async] Failed to preload shader '{}'", name);
        }
        spdlog::info("[AssetRegistry::Async] Preloaded {} shader sources", results.size());
        return results;
    });
}

void AssetRegistry::UploadShaders(const std::unordered_map<std::string, ShaderSource>& sources)
{
    for (const auto& [name, src] : sources)
    {
        if (shaders_.Find(name).IsValid())
            continue; // already compiled

        ShaderAsset sa;
        sa.vertSource = src.vert;
        sa.fragSource = src.frag;
        sa.pipeline.AddShaders({
            std::make_shared<Shader>(src.vert.c_str(), GL_VERTEX_SHADER),
            std::make_shared<Shader>(src.frag.c_str(), GL_FRAGMENT_SHADER)
        });
        sa.pipeline.Create();
        sa.programId = sa.pipeline.GetProgramId();
        sa.Introspect();

        GLuint pid = sa.programId;
        shaders_.Add(name, std::move(sa));
        spdlog::info("[AssetRegistry] Uploaded shader '{}' (program {})", name, pid);
    }
}

// -- Material API -------------------------------------------------------------

AssetHandle<MaterialAsset> AssetRegistry::GetMaterial(const std::string& relativePath)
{
    auto h = materials_.Find(relativePath);
    if (h.IsValid()) return h;

    std::string absPath = workingFolder_ + relativePath;
    return LoadMaterialFromDisk(absPath, relativePath);
}

AssetHandle<MaterialAsset> AssetRegistry::LoadMaterialFromDisk(const std::string& absolutePath,
                                                                const std::string& relativePath)
{
    auto existing = materials_.Find(relativePath);
    if (existing.IsValid()) return existing;

    MaterialAsset mat;
    if (!mat.Load(absolutePath))
    {
        spdlog::warn("[AssetRegistry] Cannot load material: {}", absolutePath);
        return AssetHandle<MaterialAsset>::Null();
    }

    auto handle = materials_.Add(relativePath, std::move(mat));
    spdlog::info("[AssetRegistry] Loaded material '{}' (texture: {}, shader: {})",
                 relativePath, materials_.Get(handle)->texturePath, materials_.Get(handle)->shaderName);
    return handle;
}

AssetHandle<MaterialAsset> AssetRegistry::GetOrCreateDefaultMaterial(const std::string& textureRelPath)
{
    // Convention: auto-material path is "auto::<texturePath>"
    std::string autoKey = "auto::" + textureRelPath;

    auto existing = materials_.Find(autoKey);
    if (existing.IsValid()) return existing;

    MaterialAsset mat;
    mat.texturePath = textureRelPath;
    mat.shaderName  = "sprite";

    // Resolve handles immediately
    mat.texture = textures_.Find(textureRelPath);
    mat.shader  = GetShader("sprite");

    auto handle = materials_.Add(autoKey, std::move(mat));
    spdlog::info("[AssetRegistry] Created auto-material for '{}'", textureRelPath);
    return handle;
}

AssetHandle<MaterialAsset> AssetRegistry::GetOrCreateProceduralMaterial(
    const std::string& shaderName,
    const std::unordered_map<std::string, UniformValue>& uniforms)
{
    std::string autoKey = "auto::procedural::" + shaderName;

    auto existing = materials_.Find(autoKey);
    if (existing.IsValid()) return existing;

    MaterialAsset mat;
    mat.shaderName = shaderName;
    // No texture -- procedural shader
    mat.shader = GetShader(shaderName);
    mat.uniforms = uniforms;

    auto handle = materials_.Add(autoKey, std::move(mat));
    spdlog::info("[AssetRegistry] Created procedural material for shader '{}'", shaderName);
    return handle;
}

void AssetRegistry::SaveMaterial(AssetHandle<MaterialAsset> h)
{
    auto* mat = materials_.Get(h);
    if (!mat) return;

    const std::string& relPath = materials_.GetPath(h);
    if (relPath.empty() || relPath.find("auto::") == 0) return; // don't persist auto-materials

    std::string absPath = workingFolder_ + relPath;
    mat->Save(absPath);
}

void AssetRegistry::ResolveMaterialHandles(AssetHandle<MaterialAsset> h)
{
    auto* mat = materials_.Get(h);
    if (!mat) return;

    if (!mat->texturePath.empty())
        mat->texture = GetTexture(mat->texturePath);
    if (!mat->shaderName.empty())
        mat->shader = GetShader(mat->shaderName);
}

// -- Pool access helpers ------------------------------------------------------

GLuint AssetRegistry::GetGLTextureHandle(AssetHandle<TextureAsset> h) const
{
    const auto* tex = textures_.Get(h);
    return tex ? tex->glHandle : 0;
}

GLuint AssetRegistry::GetGLShaderProgram(AssetHandle<ShaderAsset> h) const
{
    const auto* sa = shaders_.Get(h);
    return sa ? sa->programId : 0;
}

TextureAsset* AssetRegistry::GetTextureAssetByPath(const std::string& relativePath)
{
    auto h = textures_.Find(relativePath);
    return textures_.Get(h);
}

const TextureAsset* AssetRegistry::GetTextureAssetByPath(const std::string& relativePath) const
{
    auto h = textures_.Find(relativePath);
    return textures_.Get(h);
}

const std::string& AssetRegistry::GetSpritePath(const std::string& stem) const
{
    static const std::string kEmpty;
    auto it = imageIndex_.find(stem);
    if (it == imageIndex_.end())
    {
        spdlog::error("[AssetRegistry] Sprite '{}' not found in image index", stem);
        return kEmpty;
    }
    return it->second;
}

// -- Scanning -----------------------------------------------------------------

void AssetRegistry::ScanImages(const std::string& imagesRelPath)
{
    imageIndex_.clear();
    const std::string absDir = workingFolder_ + imagesRelPath;

    std::set<std::string> validMetaPaths;
    std::error_code ec;

    for (auto& entry : std::filesystem::recursive_directory_iterator(absDir, ec))
    {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext == ".meta") continue;
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" &&
            ext != ".bmp" && ext != ".tga" && ext != ".gif")
            continue;

        std::string stem    = entry.path().stem().string();
        std::string relPath = std::filesystem::relative(entry.path(), workingFolder_, ec).string();
        for (char& c : relPath) if (c == '\\') c = '/';

        if (imageIndex_.count(stem))
            spdlog::warn("[AssetRegistry] Duplicate image name '{}': '{}' shadows '{}'",
                         stem, relPath, imageIndex_[stem]);
        imageIndex_[stem] = relPath;

        // Load or create .meta, then create the TextureAsset (without GL upload yet)
        std::string metaPath = entry.path().string() + ".meta";
        validMetaPaths.insert(metaPath);

        TextureAsset tex;
        bool hadMeta = tex.LoadMeta(metaPath);

        // Read source dimensions
        int w = 0, h = 0, ch = 0;
        stbi_info(entry.path().string().c_str(), &w, &h, &ch);

        if (!hadMeta)
        {
            tex.sourceWidth  = w;
            tex.sourceHeight = h;
            tex.SaveMeta(metaPath);
            spdlog::info("[AssetRegistry] Created meta for '{}'", relPath);
        }
        else if (tex.sourceWidth != w || tex.sourceHeight != h)
        {
            tex.sourceWidth  = w;
            tex.sourceHeight = h;
            tex.SaveMeta(metaPath);
        }

        // Store the asset (GL handle will be populated on first use via GetTexture/LoadTextureFromDisk)
        textures_.Add(relPath, std::move(tex));
    }

    // Clean up orphaned .meta files
    for (auto& entry : std::filesystem::recursive_directory_iterator(absDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension().string() != ".meta") continue;
        if (validMetaPaths.find(entry.path().string()) == validMetaPaths.end())
        {
            spdlog::info("[AssetRegistry] Removing orphaned meta: {}", entry.path().string());
            std::filesystem::remove(entry.path(), ec);
        }
    }

    spdlog::info("[AssetRegistry] Indexed {} images from {}", imageIndex_.size(), absDir);
}

void AssetRegistry::ScanMaterials(const std::string& materialsRelPath)
{
    const std::string absDir = workingFolder_ + materialsRelPath;

    std::error_code ec;
    if (!std::filesystem::exists(absDir, ec)) return;

    for (auto& entry : std::filesystem::recursive_directory_iterator(absDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".material") continue;

        std::string relPath = std::filesystem::relative(entry.path(), workingFolder_, ec).string();
        for (char& c : relPath) if (c == '\\') c = '/';

        MaterialAsset mat;
        if (mat.Load(entry.path().string()))
        {
            materials_.Add(relPath, std::move(mat));
            spdlog::info("[AssetRegistry] Loaded material '{}'", relPath);
        }
    }
}

void AssetRegistry::ResolveAllMaterialHandles()
{
    materials_.ForEach([this](AssetHandle<MaterialAsset> h, MaterialAsset& mat)
    {
        if (!mat.texturePath.empty())
            mat.texture = textures_.Find(mat.texturePath);
        if (!mat.shaderName.empty())
            mat.shader = shaders_.Find(mat.shaderName);
    });
}

// -- Helpers ------------------------------------------------------------------

std::string AssetRegistry::ReadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        spdlog::error("[AssetRegistry] Failed to open file: {}", path);
        return {};
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

GLuint AssetRegistry::CreateGLTexture(unsigned char* pixels, int w, int h, int channels,
                                       const TextureAsset& meta)
{
    GLuint handle = 0;
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);

    // Wrap (FitToShape uses CLAMP_TO_EDGE on the GL side; tiling is handled by the renderer)
    GLenum wrap = GL_REPEAT;
    switch (meta.wrapMode)
    {
        case TextureWrapMode::Clamp:         wrap = GL_CLAMP_TO_EDGE;    break;
        case TextureWrapMode::FitToShape:    wrap = GL_CLAMP_TO_EDGE;    break;
        case TextureWrapMode::MirrorRepeat:  wrap = GL_MIRRORED_REPEAT;  break;
        default: break;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    // Filter
    switch (meta.filterMode)
    {
        case TextureFilterMode::Point:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case TextureFilterMode::Bilinear:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case TextureFilterMode::Trilinear:
        default:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format),
                 w, h, 0, format, GL_UNSIGNED_BYTE, pixels);

    if (meta.generateMipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return handle;
}

} // namespace ettycc
