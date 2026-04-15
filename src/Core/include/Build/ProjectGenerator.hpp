#pragma once

// Header-only project generator utility.
//
// Full pipeline  (RunGenerator):
//   1. ScanModules        — find *GameModule classes; track which headers contain them
//   2. CopySourceFiles    — mirror user .hpp/.h/.cpp into output/include/ and output/src/
//   3. CopyCoreFiles      — copy 80CC_CORE.lib + Core headers into output/external/
//   4. GenerateEntryPoint — inject #includes + module registration into main.cpp template
//   5. GenerateCMakeLists — write a standalone CMakeLists.txt pointing at external/
//
// No engine dependencies — only std:: headers.

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace ettycc::build
{
    using LogFn = std::function<void(const std::string&)>;

    // ─────────────────────────────────────────────────────────────────────────
    // Data
    // ─────────────────────────────────────────────────────────────────────────

    struct ScanResult
    {
        // Class names matching *GameModule — used for module registration code.
        std::vector<std::string> moduleClasses;

        // Headers that CONTAIN at least one *GameModule class.
        // Paths are absolute. Used to generate #includes in main.cpp.
        std::vector<std::string> moduleHeaders;

        // Every .hpp/.h file found in the source tree — used for CopySourceFiles.
        std::vector<std::string> allHeaders;

        bool        ok = true;
        std::string error;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Scanning
    // ─────────────────────────────────────────────────────────────────────────

    inline ScanResult ScanModules(const std::filesystem::path& sourceDir,
                                   LogFn log = nullptr)
    {
        ScanResult result;
        const std::regex classPattern(R"(\bclass\s+(\w+GameModule)\b)");

        if (!std::filesystem::exists(sourceDir))
        {
            result.ok    = false;
            result.error = "Source directory does not exist: " + sourceDir.string();
            return result;
        }

        for (auto& entry : std::filesystem::recursive_directory_iterator(sourceDir))
        {
            if (!entry.is_regular_file()) continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".hpp" && ext != ".h") continue;

            result.allHeaders.push_back(entry.path().string());

            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            const std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());

            std::vector<std::string> found;
            auto it = std::sregex_iterator(content.begin(), content.end(), classPattern);
            for (; it != std::sregex_iterator(); ++it)
                found.push_back((*it)[1].str());

            if (!found.empty())
            {
                result.moduleHeaders.push_back(entry.path().string());
                for (auto& cls : found)
                {
                    result.moduleClasses.push_back(cls);
                    if (log) log("[80CC] Found module: " + cls
                                 + " in " + entry.path().filename().string());
                }
            }
        }
        return result;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Entry-point code generation
    // ─────────────────────────────────────────────────────────────────────────

    // Generate the #include block for main.cpp.
    // Tries candidate include roots in order; uses the first that produces a
    // path without leading "..".  This handles both layouts:
    //   <src>/include/Modules/Foo.hpp  (canonical)
    //   <src>/src/include/Modules/Foo.hpp  (nested src/ layout)
    inline std::string BuildIncludesBlock(const ScanResult& scan,
                                           const std::filesystem::path& sourceDir)
    {
        namespace fs = std::filesystem;

        const std::vector<fs::path> roots = {
            sourceDir / "include",
            sourceDir / "src" / "include",
            sourceDir,
        };

        std::ostringstream ss;
        for (const auto& h : scan.moduleHeaders)
        {
            fs::path bestRel;
            for (const auto& root : roots)
            {
                std::error_code ec;
                const fs::path rel = fs::relative(h, root, ec);
                if (!ec && rel.string().rfind("..", 0) != 0)
                {
                    bestRel = rel;
                    break;
                }
            }
            if (bestRel.empty())
                bestRel = fs::path(h).filename();  // last-resort fallback

            ss << "#include \"" << bestRel.generic_string() << "\"\n";
        }
        return ss.str();
    }

    // Generate the module-registration block for main.cpp.
    inline std::string BuildModulesBlock(const ScanResult& scan)
    {
        std::ostringstream ss;
        ss << "std::vector<std::shared_ptr<GameModule>> modules = {\n";
        for (const auto& cls : scan.moduleClasses)
            ss << "    std::make_shared<" << cls << ">(),\n";
        ss << "};\nengineInstance->RegisterModules(modules);\n";
        return ss.str();
    }

    // Search common locations for the Entry template main.cpp.
    inline std::filesystem::path FindEntryTemplate(const std::filesystem::path& hintDir = {})
    {
        namespace fs = std::filesystem;

        std::vector<fs::path> candidates;
        if (!hintDir.empty())
            candidates.push_back(hintDir / "main.cpp");

        candidates.insert(candidates.end(), {
            fs::current_path() / "src/Entry/src/main.cpp",
            fs::current_path() / "../src/Entry/src/main.cpp",
            fs::current_path() / "../../src/Entry/src/main.cpp",
            fs::current_path() / "../../../src/Entry/src/main.cpp",
        });

        if (const char* env = std::getenv("80CC_ENTRY_TEMPLATE"))
            candidates.insert(candidates.begin(), fs::path(env));

        for (const auto& c : candidates)
        {
            std::error_code ec;
            const auto canonical = fs::weakly_canonical(c, ec);
            if (!ec && fs::exists(canonical)) return canonical;
        }
        return {};
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Asset copy
    // ─────────────────────────────────────────────────────────────────────────

    // Copy runtime assets from assetsSourceDir into outputDir/assets/.
    // The src/ subdirectory is excluded — it contains game source, not runtime files.
    // Any future folder (audio/, fonts/, ...) is included automatically.
    inline void CopyAssets(const std::filesystem::path& assetsSourceDir,
                            const std::filesystem::path& outputDir,
                            LogFn log = nullptr)
    {
        namespace fs = std::filesystem;
        if (assetsSourceDir.empty() || !fs::exists(assetsSourceDir))
        {
            if (log) log("[WARNING] Assets directory not found, skipping: "
                         + assetsSourceDir.string());
            return;
        }

        const fs::path dest = outputDir / "assets";
        fs::create_directories(dest);

        for (auto& entry : fs::recursive_directory_iterator(assetsSourceDir))
        {
            const fs::path rel = fs::relative(entry.path(), assetsSourceDir);
            // Skip src/ — game source code is not a runtime asset
            if (!rel.empty() && *rel.begin() == "src") continue;
            if (entry.is_directory()) continue;

            const fs::path d = dest / rel;
            fs::create_directories(d.parent_path());
            fs::copy_file(entry.path(), d, fs::copy_options::overwrite_existing);
        }

        if (log) log("[80CC] Assets copied -> " + dest.string());
    }

    // Inject includes + module registration into the Entry template -> output/main.cpp.
    inline bool GenerateEntryPoint(const ScanResult& scan,
                                    const std::filesystem::path& sourceDir,
                                    const std::filesystem::path& entryTemplate,
                                    const std::filesystem::path& outputDir,
                                    LogFn log = nullptr)
    {
        namespace fs = std::filesystem;

        std::ifstream in(entryTemplate);
        if (!in.is_open())
        {
            if (log) log("[ERROR] Cannot open entry template: " + entryTemplate.string());
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        in.close();

        // Prepend platform includes needed by the asset-init block
        content.insert(0,
            "// [80CC GENERATED] -- do not edit, re-run the generator.\n"
            "#ifdef _WIN32\n"
            "#ifndef NOMINMAX\n"
            "#define NOMINMAX\n"
            "#endif\n"
            "#ifndef WIN32_LEAN_AND_MEAN\n"
            "#define WIN32_LEAN_AND_MEAN\n"
            "#endif\n"
            "#include <windows.h>\n"
            "#else\n"
            "#include <unistd.h>\n"
            "#endif\n"
            "#include <filesystem>\n"
            "#include <cstdlib>\n"
        );

        // Strip the dev-only main.hpp include — it only exists to define the
        // _80CC_USER_* macros as empty stubs for the editor build. The generator
        // replaces those tags with real code so the include is not needed here.
        {
            const std::string needle = "#include <main.hpp>";
            const size_t pos = content.find(needle);
            if (pos != std::string::npos)
            {
                // Remove the line including its trailing newline.
                size_t end = pos + needle.size();
                if (end < content.size() && content[end] == '\n') ++end;
                content.erase(pos, end - pos);
            }
        }

        auto injectTag = [&](const std::string& tag, const std::string& block)
        {
            const size_t pos = content.find(tag);
            if (pos != std::string::npos)
                content.replace(pos, tag.size(), block);
            else if (log)
                log("[WARNING] Tag not found in template: " + tag);
        };

        injectTag("_80CC_USER_INCLUDES;", BuildIncludesBlock(scan, sourceDir));
        injectTag("_80CC_USER_CODE;",     BuildModulesBlock(scan));

        // Inject a chdir to the exe's directory so that assets/ is always found
        // as a simple relative path — no env vars, no absolute paths baked in.
        const std::string assetInit =
            "// [80CC] Set working directory to the executable's location.\n"
            "// assets/ placed next to the binary is then found via the default relative path.\n"
            "#ifdef _WIN32\n"
            "    { char _p[MAX_PATH]={}; ::GetModuleFileNameA(nullptr,_p,MAX_PATH);\n"
            "      ::SetCurrentDirectoryA(\n"
            "        std::filesystem::path(_p).parent_path().string().c_str()); }\n"
            "#else\n"
            "    { char _p[PATH_MAX]={}; ssize_t _n=::readlink(\"/proc/self/exe\",_p,PATH_MAX);\n"
            "      if(_n>0){_p[_n]=0; ::chdir(\n"
            "        std::filesystem::path(_p).parent_path().string().c_str());} }\n"
            "#endif";
        injectTag("_80CC_ASSET_INIT;", assetInit);

        fs::create_directories(outputDir);
        std::ofstream out(outputDir / "main.cpp");
        if (!out.is_open())
        {
            if (log) log("[ERROR] Cannot write main.cpp to: " + outputDir.string());
            return false;
        }
        out << content;

        if (log) log("[80CC] Entry point written: " + (outputDir / "main.cpp").string());
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Core library + headers copy
    // ─────────────────────────────────────────────────────────────────────────

    // Copy the pre-built Core library and its include headers into
    // outputDir/external/lib/  and  outputDir/external/include/
    // so the generated project is fully self-contained.
    inline bool CopyCoreFiles(const std::string& coreLibPath,
                               const std::string& coreIncludePath,
                               const std::filesystem::path& outputDir,
                               LogFn log = nullptr)
    {
        namespace fs = std::filesystem;

        // ── Library ───────────────────────────────────────────────────────────
        if (!coreLibPath.empty())
        {
            if (!fs::exists(coreLibPath))
            {
                if (log) log("[WARNING] Core lib not found, skipping copy: " + coreLibPath);
            }
            else
            {
                const fs::path libDest = outputDir / "external" / "lib"
                                         / fs::path(coreLibPath).filename();
                fs::create_directories(libDest.parent_path());
                fs::copy_file(coreLibPath, libDest, fs::copy_options::overwrite_existing);
                if (log) log("[80CC] Core lib  -> " + libDest.string());
            }
        }

        // ── Include headers ───────────────────────────────────────────────────
        if (!coreIncludePath.empty())
        {
            if (!fs::exists(coreIncludePath))
            {
                if (log) log("[WARNING] Core include dir not found: " + coreIncludePath);
            }
            else
            {
                const fs::path incDest = outputDir / "external" / "include";
                fs::create_directories(incDest);
                for (auto& entry : fs::recursive_directory_iterator(coreIncludePath))
                {
                    if (!entry.is_regular_file()) continue;
                    const fs::path rel  = fs::relative(entry.path(), coreIncludePath);
                    const fs::path dest = incDest / rel;
                    fs::create_directories(dest.parent_path());
                    fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                }
                if (log) log("[80CC] Core include -> " + incDest.string());
            }
        }

        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CMakeLists.txt generation
    // ─────────────────────────────────────────────────────────────────────────

    // Write a standalone CMakeLists.txt into outputDir.
    // The project links against external/lib/80CC_CORE.lib and includes external/include/
    // — both copied by CopyCoreFiles, no absolute host paths baked in.
    inline bool GenerateCMakeLists(const std::filesystem::path& outputDir,
                                    const std::string& projectName,
                                    LogFn log = nullptr)
    {
        namespace fs = std::filesystem;
        fs::create_directories(outputDir);

        std::ostringstream cmake;
        cmake <<
R"(cmake_minimum_required(VERSION 3.20)

# [80CC GENERATED] — do not edit, re-run the generator.
project()" << projectName << R"( CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── 80CC Core (pre-built — copied into external/ by the Generator) ───────────
find_library(80CC_CORE_LIB
    NAMES 80CC_CORE lib80CC_CORE
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/external/lib"
    NO_DEFAULT_PATH
    REQUIRED)

# ── vcpkg packages (same as 80CC_CORE dependencies) ──────────────────────────
find_package(spdlog           CONFIG REQUIRED)
find_package(SDL2                    REQUIRED)
find_package(glm              CONFIG REQUIRED)
find_package(GLEW                    REQUIRED)
find_package(nlohmann_json    CONFIG REQUIRED)
find_package(cereal           CONFIG REQUIRED)
find_package(unofficial-enet  CONFIG REQUIRED)
find_package(Bullet           CONFIG REQUIRED)
find_package(OpenAL           CONFIG REQUIRED)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE GAME_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include/*.hpp")

add_executable(${PROJECT_NAME}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    ${GAME_SOURCES}
    ${GAME_HEADERS})

# Standalone game build — excludes the editor (DevEditor, imgui, etc.)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE COMPILE_80CC_STAND_ALONE_EXECUTABLE
    PRIVATE COMPILED_EXEC_NAME="${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
    PRIVATE ${BULLET_INCLUDE_DIRS}
    PRIVATE ${SDL2_INCLUDE_DIRS}
    PRIVATE ${GLEW_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE "${80CC_CORE_LIB}"
    PRIVATE spdlog::spdlog
    PRIVATE ${SDL2_LIBRARIES}
    PRIVATE GLEW::GLEW
    PRIVATE glm::glm
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE cereal::cereal
    PRIVATE unofficial::enet::enet
    PRIVATE ${BULLET_LIBRARIES}
    PUBLIC OpenAL::OpenAL)

# ── Copy assets next to the binary after every build ─────────────────────────
# $<TARGET_FILE_DIR:...> resolves correctly for all generators (Ninja, VS, etc.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to binary directory...")
endif()

# ── Install rules (cmake --install -> dist/) ──────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION ".")
endif()
# Copy all DLLs that ended up next to the binary (vcpkg runtime deps on Windows)
if(WIN32)
    install(DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
        DESTINATION "."
        FILES_MATCHING PATTERN "*.dll")
endif()
)";

        std::ofstream out(outputDir / "CMakeLists.txt");
        if (!out.is_open())
        {
            if (log) log("[ERROR] Cannot write CMakeLists.txt to: " + outputDir.string());
            return false;
        }
        out << cmake.str();

        if (log) log("[80CC] CMakeLists.txt written: " + (outputDir / "CMakeLists.txt").string());
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Source copy
    // ─────────────────────────────────────────────────────────────────────────

    inline void CopySourceFiles(const ScanResult& scan,
                                 const std::filesystem::path& sourceDir,
                                 const std::filesystem::path& outputDir,
                                 LogFn log = nullptr)
    {
        namespace fs = std::filesystem;
        fs::create_directories(outputDir);

        // Headers (all of them — moduleHeaders is a subset already in allHeaders)
        for (const auto& h : scan.allHeaders)
        {
            const fs::path rel  = fs::relative(h, sourceDir);
            const fs::path dest = outputDir / rel;
            fs::create_directories(dest.parent_path());
            fs::copy_file(h, dest, fs::copy_options::overwrite_existing);
        }

        // .cpp / .cc sources
        for (auto& entry : fs::recursive_directory_iterator(sourceDir))
        {
            if (!entry.is_regular_file()) continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".cc") continue;
            const fs::path rel  = fs::relative(entry.path(), sourceDir);
            const fs::path dest = outputDir / rel;
            fs::create_directories(dest.parent_path());
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
        }

        if (log) log("[80CC] Source files copied to: " + outputDir.string());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Convenience entry point
    // ─────────────────────────────────────────────────────────────────────────

    // Full pipeline:
    //   scan -> copy sources -> copy Core lib+headers -> generate main.cpp -> generate CMakeLists.txt
    //
    // coreLibPath     — path to the pre-built 80CC_CORE.lib / lib80CC_CORE.a
    // coreIncludePath — path to the Core include/ directory
    //
    // Both are COPIED into outputDir/external/ so the project is fully self-contained.
    inline bool RunGenerator(const std::string& sourceDirStr,
                              const std::string& outputDirStr,
                              const std::string& projectName,
                              const std::string& coreLibPath,
                              const std::string& coreIncludePath,
                              const std::string& entryTemplateHint = "",
                              LogFn              log               = nullptr)
    {
        namespace fs = std::filesystem;

        const fs::path sourceDir = sourceDirStr;
        const fs::path outputDir = fs::path(outputDirStr) / projectName;

        if (log) log("[80CC] Source  : " + sourceDir.string());
        // if (log) log("[80CC] Assets  : " + (assetsDir.empty() ? "(not set)" : assetsDir.string()));
        if (log) log("[80CC] Output  : " + outputDir.string());
        if (log) log("[80CC] CoreLib : " + (coreLibPath.empty() ? "(not set)" : coreLibPath));

        // ── 1. Scan ───────────────────────────────────────────────────────────
        if (log) log("[80CC] Scanning modules...");
        const ScanResult scan = ScanModules(sourceDir, log);
        if (!scan.ok)
        {
            if (log) log("[ERROR] " + scan.error);
            return false;
        }
        if (log) log("[80CC] Modules found : " + std::to_string(scan.moduleClasses.size()));
        if (log) log("[80CC] Headers found : " + std::to_string(scan.allHeaders.size()));

        // ── 2. Copy user source files ─────────────────────────────────────────
        try { CopySourceFiles(scan, sourceDir, outputDir, log); }
        catch (const std::exception& e)
        {
            if (log) log(std::string("[ERROR] Copy failed: ") + e.what());
            return false;
        }

        // ── 3. Copy Core lib + headers -> external/ ────────────────────────────
        try { CopyCoreFiles(coreLibPath, coreIncludePath, outputDir, log); }
        catch (const std::exception& e)
        {
            if (log) log(std::string("[ERROR] Core copy failed: ") + e.what());
            // Non-fatal: cmake will fail at find_library but project structure is correct.
        }

        // ── 4. Generate main.cpp entry point ──────────────────────────────────
        const fs::path tpl = FindEntryTemplate(entryTemplateHint.empty()
                                                ? fs::path{}
                                                : fs::path(entryTemplateHint));
        if (tpl.empty())
        {
            if (log) log("[WARNING] Entry template (main.cpp) not found — "
                         "set 80CC_ENTRY_TEMPLATE env var or pass a hint path.");
        }
        else if (!GenerateEntryPoint(scan, sourceDir, tpl, outputDir, log))
        {
            return false;
        }

        // ── 5. Generate CMakeLists.txt ────────────────────────────────────────
        if (!GenerateCMakeLists(outputDir, projectName, log))
            return false;

        // ── 6. Copy assets into project root ──────────────────────────────────
        // cmake's POST_BUILD rule will then copy them from here to the binary dir.
        if (!sourceDir.empty())
        {
            try { CopyAssets(sourceDir, outputDir, log); }
            catch (const std::exception& e)
            { if (log) log(std::string("[WARNING] Assets copy failed: ") + e.what()); }
        }
        else
        {
            if (log) log("[WARNING] No assets directory set — game may not find resources at runtime.");
        }

        return true;
    }

} // namespace ettycc::build
