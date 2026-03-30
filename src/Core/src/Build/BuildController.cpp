#undef max
#undef min

#include <Build/BuildController.hpp>
#include <Build/BuildStrings.hpp>
#include <Build/ProjectGenerator.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ettycc::build
{
    // -------------------------------------------------------------------------
    // AutoDetectBuildConfig
    // -------------------------------------------------------------------------

    void AutoDetectBuildConfig(GlobalBuildConfig& cfg)
    {
#ifdef _WIN32
        namespace fs = std::filesystem;

        // ── Core lib + Core include detection ─────────────────────────────────
        if (cfg.coreLibPath[0] == '\0' || cfg.coreIncludePath[0] == '\0')
        {
            char exePath[512] = {};
            if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
            {
                const fs::path exeDir = fs::path(exePath).parent_path();

                if (cfg.coreLibPath[0] == '\0')
                {
                    for (auto& rel : { "80CC_CORE.lib",
                                       "../Core/80CC_CORE.lib",
                                       "../../src/Core/80CC_CORE.lib" })
                    {
                        const fs::path c = exeDir / rel;
                        if (fs::exists(c))
                        {
                            strncpy(cfg.coreLibPath, c.string().c_str(),
                                    sizeof(cfg.coreLibPath) - 1);
                            break;
                        }
                    }
                }

                if (cfg.coreIncludePath[0] == '\0')
                {
                    for (auto& rel : { "include",
                                       "../../src/Core/include",
                                       "../../../src/Core/include" })
                    {
                        const fs::path c = exeDir / rel;
                        if (fs::exists(c / "Engine.hpp"))
                        {
                            strncpy(cfg.coreIncludePath, c.string().c_str(),
                                    sizeof(cfg.coreIncludePath) - 1);
                            break;
                        }
                    }
                }
            }
        }

        // ── cmake generator + vcvarsall detection ─────────────────────────────
        if (cfg.cmakeGenerator[0] == '\0')
        {
            std::string vsInstallPath;

            if (fs::exists(str::PATH_VSWHERE_EXE))
            {
                FILE* p = _popen(str::PATH_VSWHERE_CMD, "r");
                if (p)
                {
                    char buf[512] = {};
                    if (fgets(buf, sizeof(buf), p))
                    {
                        vsInstallPath = buf;
                        while (!vsInstallPath.empty() &&
                               (vsInstallPath.back() == '\n' || vsInstallPath.back() == '\r'))
                            vsInstallPath.pop_back();
                    }
                    _pclose(p);
                }
            }

            if (!vsInstallPath.empty() && cfg.vcvarsallPath[0] == '\0')
            {
                const std::string vcvars = vsInstallPath + str::PATH_VCVARSALL_REL;
                if (fs::exists(vcvars))
                    strncpy(cfg.vcvarsallPath, vcvars.c_str(), sizeof(cfg.vcvarsallPath) - 1);
            }

            bool ninjaFound = false;
            if (!vsInstallPath.empty())
            {
                const std::string ninjaInVS = vsInstallPath + str::PATH_VS_NINJA_REL;
                ninjaFound = fs::exists(ninjaInVS);
            }
            if (!ninjaFound)
            {
                FILE* p = _popen(str::NINJA_CHECK_CMD, "r");
                if (p)
                {
                    char b[32];
                    ninjaFound = (fgets(b, sizeof(b), p) != nullptr);
                    _pclose(p);
                }
            }

            if (ninjaFound)
                strncpy(cfg.cmakeGenerator, str::GEN_NINJA,  sizeof(cfg.cmakeGenerator) - 1);
            else
                strncpy(cfg.cmakeGenerator, str::GEN_VS_2022, sizeof(cfg.cmakeGenerator) - 1);
        }

        // ── vcpkg toolchain detection ──────────────────────────────────────────
        if (cfg.vcpkgToolchain[0] == '\0')
        {
            auto tryToolchainFile = [&](const std::string& path) -> bool
            {
                if (path.empty()) return false;
                if (fs::exists(path))
                {
                    strncpy(cfg.vcpkgToolchain, path.c_str(),
                            sizeof(cfg.vcpkgToolchain) - 1);
                    return true;
                }
                return false;
            };

            auto tryVcpkgRoot = [&](const std::string& root) -> bool
            {
                if (root.empty()) return false;
                return tryToolchainFile(root + str::VCPKG_TOOLCHAIN_REL);
            };

            // 1. Parse CMAKE_TOOLCHAIN_FILE from CMakeCache.txt near the exe.
            {
                char exePath[512] = {};
                if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
                {
                    fs::path dir = fs::path(exePath).parent_path();
                    for (int depth = 0; depth < 6 && !dir.empty();
                         ++depth, dir = dir.parent_path())
                    {
                        const fs::path cache = dir / "CMakeCache.txt";
                        std::ifstream f(cache);
                        if (!f.is_open()) continue;

                        std::string line;
                        while (std::getline(f, line))
                        {
                            const std::string key = str::CMAKE_TOOLCHAIN_KEY;
                            const auto pos = line.find(key);
                            if (pos != std::string::npos)
                            {
                                const std::string tc = line.substr(pos + key.size());
                                if (tryToolchainFile(tc)) break;
                            }
                        }
                        if (cfg.vcpkgToolchain[0] != '\0') break;
                    }
                }
            }

            // 2. VCPKG_ROOT env var.
            if (cfg.vcpkgToolchain[0] == '\0')
                if (const char* env = std::getenv("VCPKG_ROOT"))
                    tryVcpkgRoot(env);

            // 3. Common install locations + sibling dirs.
            if (cfg.vcpkgToolchain[0] == '\0')
            {
                const char* home = std::getenv("USERPROFILE");
                std::vector<std::string> candidates = {
                    "C:/vcpkg", "C:/tools/vcpkg",
                    "D:/vcpkg", "D:/repos2/vcpkg",
                    "E:/vcpkg"
                };
                if (home)
                {
                    candidates.push_back(std::string(home) + "/vcpkg");
                    candidates.push_back(std::string(home) + "/source/repos/vcpkg");
                }
                {
                    char exePath2[512] = {};
                    if (GetModuleFileNameA(nullptr, exePath2, sizeof(exePath2)))
                    {
                        fs::path dir = fs::path(exePath2).parent_path();
                        for (int d = 0; d < 6 && !dir.empty();
                             ++d, dir = dir.parent_path())
                            candidates.push_back((dir / "vcpkg").string());
                    }
                }
                for (const auto& c : candidates)
                    if (tryVcpkgRoot(c)) break;
            }
        }
#endif // _WIN32
    }

    // -------------------------------------------------------------------------
    // BuildController
    // -------------------------------------------------------------------------

    BuildController::BuildController()  = default;
    BuildController::~BuildController()
    {
        if (buildThread_.joinable())
            buildThread_.join();
    }

    void BuildController::PushLog(std::string msg)
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        log_.push_back(std::move(msg));
        scrollToBottom_.store(true);
    }

    void BuildController::SetStatus(std::string s)
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        statusMsg_ = std::move(s);
    }

    std::vector<std::string> BuildController::GetLogSnapshot()
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        return log_;
    }

    bool BuildController::ConsumeScrollRequest()
    {
        if (scrollToBottom_.load())
        {
            scrollToBottom_.store(false);
            return true;
        }
        return false;
    }

    std::string BuildController::GetStatus()
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        return statusMsg_;
    }

    void BuildController::ClearLog()
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        log_.clear();
    }

    void BuildController::StartPipeline(const BuildRequest& req, const GlobalBuildConfig& cfg)
    {
        if (buildThread_.joinable())
            buildThread_.join();

        {
            std::lock_guard<std::mutex> lk(logMutex_);
            log_.clear();
            statusMsg_ = str::STS_STARTING;
        }
        progress_.store(0.0f);
        running_.store(true);
        scrollToBottom_.store(true);

        RunPipeline(req, cfg);
    }

    // -------------------------------------------------------------------------
    // RunPipeline  (background thread)
    // -------------------------------------------------------------------------

    void BuildController::RunPipeline(BuildRequest req, GlobalBuildConfig cfg)
    {
        buildThread_ = std::thread([this,
                                    req = std::move(req),
                                    cfg = std::move(cfg)]()
        {
            namespace fs = std::filesystem;

            const std::string& srcPath        = req.sourcePath;
            const std::string& outPath        = req.outputPath;
            const std::string& projName       = req.projectName;
            const std::string& cfgStr         = req.cfgStr;
            const bool         doClean        = req.doClean;
            const bool         keepGenerated  = req.keepGenerated;
            const std::string  coreLib        = cfg.coreLibPath;
            const std::string  coreInc        = cfg.coreIncludePath;
            const std::string  generator      = cfg.cmakeGenerator;
            const std::string  vcvarsall      = cfg.vcvarsallPath;
            const std::string  vcpkgToolchain = cfg.vcpkgToolchain;

            const fs::path buildOut = fs::path(outPath) / projName;
            const fs::path buildBin = buildOut / "build";

            auto runCmd = [&](const std::string& cmd) -> bool
            {
#ifdef _WIN32
                const fs::path batFile = buildBin / "_80cc_run.bat";
                {
                    std::ofstream bat(batFile);
                    bat << "@echo off\r\n";
                    if (!vcvarsall.empty())
                        bat << "call \"" << vcvarsall << "\" x64 > nul 2>&1\r\n";
                    bat << cmd << "\r\n";
                    bat << "exit /b %errorlevel%\r\n";
                }
                FILE* pipe = _popen(("\"" + batFile.string() + "\" 2>&1").c_str(), "r");
#else
                FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
#endif
                if (!pipe)
                {
                    PushLog(std::string(str::LOG_ERROR) + " Failed to launch: " + cmd);
                    return false;
                }
                char lineBuf[512];
                while (fgets(lineBuf, sizeof(lineBuf), pipe))
                {
                    std::string line(lineBuf);
                    while (!line.empty() &&
                           (line.back() == '\n' || line.back() == '\r'))
                        line.pop_back();
                    PushLog(std::move(line));
                }
#ifdef _WIN32
                const int rc = _pclose(pipe);
                std::error_code ec;
                fs::remove(batFile, ec);
                return rc == 0;
#else
                return pclose(pipe) == 0;
#endif
            };

            // ── Stage 1: Clean ────────────────────────────────────────────────
            if (doClean && fs::exists(buildBin))
            {
                SetStatus(str::STS_CLEANING);
                PushLog(std::string(str::LOG_80CC) + " Removing: " + buildBin.string());
                try { fs::remove_all(buildBin); }
                catch (const std::exception& e)
                {
                    PushLog(std::string(str::LOG_ERROR) + " Clean failed: " + e.what());
                }
            }

            try { fs::create_directories(buildBin); }
            catch (const std::exception& e)
            {
                PushLog(std::string(str::LOG_ERROR) + " Cannot create output dir: " + e.what());
                SetStatus(str::STS_FAILED);
                running_.store(false);
                return;
            }
            progress_.store(0.08f);

            // ── Stage 2: Generator (inline — no subprocess) ───────────────────
            SetStatus(str::STS_RUNNING_GEN);
            PushLog(std::string(str::LOG_80CC) + " --- Stage 1/3: Generator ---");

            const bool genOk = ettycc::build::RunGenerator(
                srcPath, outPath, projName, coreLib, coreInc,
                /*templateHint=*/"",
                [&](const std::string& msg) { PushLog(msg); });

            if (!genOk)
            {
                PushLog(std::string(str::LOG_ERROR) + " Generator failed — aborting.");
                SetStatus(str::STS_GEN_FAILED);
                running_.store(false);
                return;
            }
            progress_.store(0.38f);

            // ── Stage 3: cmake configure ──────────────────────────────────────
            SetStatus(str::STS_CONFIGURING);
            PushLog(std::string(str::LOG_80CC) + " --- Stage 2/3: cmake configure ---");

            {
                std::error_code ec;
                fs::remove(buildBin / "CMakeCache.txt", ec);
                fs::remove_all(buildBin / "CMakeFiles", ec);
            }

            std::string configCmd = "cmake -S \"" + buildOut.string()
                + "\" -B \"" + buildBin.string()
                + "\" -DCMAKE_BUILD_TYPE=" + cfgStr;
            if (!generator.empty())
            {
                configCmd += " -G \"" + generator + "\"";
                if (generator.find("Visual Studio") != std::string::npos)
                    configCmd += " -A x64";
            }
            if (!vcpkgToolchain.empty() && fs::exists(vcpkgToolchain))
                configCmd += " -DCMAKE_TOOLCHAIN_FILE=\"" + vcpkgToolchain + "\"";
            PushLog(std::string(str::LOG_80CC) + " " + configCmd);

            if (!runCmd(configCmd))
                PushLog(std::string(str::LOG_WARNING) +
                        " cmake configure returned non-zero — build may still succeed.");

            progress_.store(0.60f);

            // ── Stage 4: cmake --build ────────────────────────────────────────
            SetStatus(str::STS_COMPILING);
            PushLog(std::string(str::LOG_80CC) + " --- Stage 3/3: cmake build ---");

            const std::string buildCmd = "cmake --build \"" + buildBin.string()
                + "\" --config " + cfgStr + " --parallel";
            PushLog(std::string(str::LOG_80CC) + " " + buildCmd);

            const bool ok = runCmd(buildCmd);
            progress_.store(0.90f);

            if (!ok)
            {
                PushLog(std::string(str::LOG_ERROR) + " ===========================");
                PushLog(std::string(str::LOG_ERROR) + "  Build FAILED.");
                PushLog(std::string(str::LOG_ERROR) + " ===========================");
                SetStatus(str::STS_FAILED);
                running_.store(false);
                return;
            }

            // ── Stage 4: Pack (cmake --install -> dist/) ──────────────────────
            SetStatus(str::STS_PACKING);
            PushLog(std::string(str::LOG_80CC) + " --- Stage 4/4: Pack ---");

            const fs::path distDir = buildOut / "dist";
            {
                std::error_code ec;
                fs::remove_all(distDir, ec);
            }

            const std::string installCmd = "cmake --install \""
                + buildBin.string() + "\" --prefix \""
                + distDir.string() + "\" --config " + cfgStr;
            PushLog(std::string(str::LOG_80CC) + " " + installCmd);
            runCmd(installCmd); // non-fatal if install rules are missing

            progress_.store(1.0f);

            // ── Stage 5: Cleanup (unless keepGenerated) ───────────────────────
            if (!keepGenerated && fs::exists(distDir))
            {
                SetStatus(str::STS_CLEANING_UP);
                PushLog(std::string(str::LOG_80CC) + " --- Stage 5/5: Cleanup ---");
                std::error_code ec;
                for (auto& entry : fs::directory_iterator(buildOut, ec))
                {
                    if (entry.path().filename() == "dist") continue;
                    fs::remove_all(entry.path(), ec);
                    if (ec)
                        PushLog(std::string(str::LOG_WARNING) +
                                " Could not remove: " + entry.path().string());
                }
                PushLog(std::string(str::LOG_80CC) +
                        " Generated project cleaned up — only dist/ kept.");
            }

            PushLog(std::string(str::LOG_80CC) + " ===========================");
            PushLog(std::string(str::LOG_80CC) + "  Build complete!");
            if (fs::exists(distDir))
                PushLog(std::string(str::LOG_80CC) + "  Distribution: " + distDir.string());
            PushLog(std::string(str::LOG_80CC) + " ===========================");
            SetStatus(str::STS_DONE);

            running_.store(false);
        });
    }

} // namespace ettycc::build
