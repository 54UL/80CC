#include <UI/BuildPanel.hpp>
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>

#undef max
#undef min

namespace ettycc
{
    BuildPanel::BuildPanel()
    {
        // Leave sourcePath_ empty — the CWD is typically the cmake build dir,
        // not the user's game source, so defaulting it causes confusion.
        // The user must browse to their game source root (the folder that
        // contains include/ and src/ with *GameModule classes).
    }

    BuildPanel::~BuildPanel()
    {
        if (buildThread_.joinable())
            buildThread_.join();
    }

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void BuildPanel::Draw()
    {
        ImGui::Begin("Build");

        // ── Project ───────────────────────────────────────────────────────────
        ImGui::SeparatorText("Project");

        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("##bld_name", projectName_, sizeof(projectName_));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Project name");

        // Browse button width — fixed so input stretches to fill the rest.
        constexpr float kBtnW = 52.0f;

        // pathField: [label]  [___input___________]  [...btn]
        // The ##-prefixed InputText ID hides the label from ImGui so it does
        // NOT render extra label text to the right of the widget, which would
        // push the browse button off-screen.
        // filter: pfd filter list passed to open_file when isFolder==false.
        //   e.g. { "CMake files", "*.cmake", "All files", "*" }
        auto pathField = [&](const char* label, const char* inputId, const char* btnId,
                             char* buf, size_t bufSz, bool isFolder,
                             std::vector<std::string> filter = { "All Files", "*" })
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
            ImGui::PushItemWidth(-kBtnW - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputText(inputId, buf, bufSz);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(btnId, ImVec2(kBtnW, 0)))
            {
                if (isFolder)
                {
                    auto dlg = pfd::select_folder(std::string("Select ") + label,
                                                  buf[0] ? buf : ".");
                    if (!dlg.result().empty())
                        strncpy(buf, dlg.result().c_str(), bufSz - 1);
                }
                else
                {
                    auto dlg = pfd::open_file(std::string("Select ") + label,
                                              buf[0] ? buf : ".", filter);
                    if (!dlg.result().empty())
                        strncpy(buf, dlg.result()[0].c_str(), bufSz - 1);
                }
            }
        };

        pathField("Source", "##bld_src", "...##bld_src_btn",
                  sourcePath_, sizeof(sourcePath_), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Game source root — must contain include/ and src/ directly.\n"
                              "e.g. assets/src  (not assets/)");

        pathField("Output", "##bld_out", "...##bld_out_btn",
                  outputPath_, sizeof(outputPath_), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Parent folder for the generated project.\n"
                              "Generator creates <Output>/<ProjectName>/");

        ImGui::SeparatorText("80CC Core");

        // Auto-detect Core lib + include on first render
        if (coreLibPath_[0] == '\0' || coreIncludePath_[0] == '\0')
        {
#ifdef _WIN32
            char exePath[512] = {};
            if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
            {
                namespace fs = std::filesystem;
                const fs::path exeDir = fs::path(exePath).parent_path();

                // Lib: look for 80CC_CORE.lib in common MSVC output paths
                if (coreLibPath_[0] == '\0')
                {
                    for (auto& rel : { "80CC_CORE.lib",
                                       "../Core/80CC_CORE.lib",
                                       "../../src/Core/80CC_CORE.lib" })
                    {
                        const fs::path c = exeDir / rel;
                        if (fs::exists(c))
                        {
                            strncpy(coreLibPath_, c.string().c_str(), sizeof(coreLibPath_) - 1);
                            break;
                        }
                    }
                }
                // Include: look for the Core include/ folder
                if (coreIncludePath_[0] == '\0')
                {
                    for (auto& rel : { "include",
                                       "../../src/Core/include",
                                       "../../../src/Core/include" })
                    {
                        const fs::path c = exeDir / rel;
                        if (fs::exists(c / "Engine.hpp"))  // sanity-check
                        {
                            strncpy(coreIncludePath_, c.string().c_str(),
                                    sizeof(coreIncludePath_) - 1);
                            break;
                        }
                    }
                }
            }
#endif
        }

        pathField("Core lib", "##bld_clib", "...##bld_clib_btn",
                  coreLibPath_, sizeof(coreLibPath_), false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Pre-built 80CC_CORE.lib / lib80CC_CORE.a\n"
                              "(auto-detected if next to the editor executable)");

        pathField("Core include", "##bld_cinc", "...##bld_cinc_btn",
                  coreIncludePath_, sizeof(coreIncludePath_), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("80CC_CORE include/ directory — contains Engine.hpp, 80CC.hpp, etc.");

        // ── Configuration ─────────────────────────────────────────────────────
        ImGui::SeparatorText("Configuration");

        static const char* const kConfigs[] = { "Debug", "Release", "RelWithDebInfo" };
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("Config", &configIndex_, kConfigs, IM_ARRAYSIZE(kConfigs));
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::Checkbox("Clean build", &cleanBuild_);

        // cmake generator + vcvarsall — auto-detected once on Windows
        if (cmakeGenerator_[0] == '\0')
        {
#ifdef _WIN32
            // Use vswhere to find the latest VS install path.
            // vswhere ships with VS 2017+ installer at a fixed location.
            const char* vswhere =
                "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe";
            std::string vsInstallPath;

            if (std::filesystem::exists(vswhere))
            {
                FILE* p = _popen(
                    "\"C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe\""
                    " -latest -property installationPath 2>nul", "r");
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

            // Set vcvarsall.bat path from the discovered install dir
            if (!vsInstallPath.empty() && vcvarsallPath_[0] == '\0')
            {
                const std::string vcvars = vsInstallPath + "\\VC\\Auxiliary\\Build\\vcvarsall.bat";
                if (std::filesystem::exists(vcvars))
                    strncpy(vcvarsallPath_, vcvars.c_str(), sizeof(vcvarsallPath_) - 1);
            }

            // Prefer Ninja (bundled with VS 2019+) — avoids cmake needing to know
            // the exact VS generator name (e.g. VS 18 is not yet in cmake 3.26).
            // Ninja is found in VS's CMake extensions directory.
            bool ninjaFound = false;
            if (!vsInstallPath.empty())
            {
                const std::string ninjaInVS = vsInstallPath
                    + "\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe";
                ninjaFound = std::filesystem::exists(ninjaInVS);
            }
            if (!ninjaFound)
            {
                // Also check if ninja is in PATH
                FILE* p = _popen("ninja --version 2>nul", "r");
                if (p) { char b[32]; ninjaFound = (fgets(b, sizeof(b), p) != nullptr); _pclose(p); }
            }

            if (ninjaFound)
                strncpy(cmakeGenerator_, "Ninja", sizeof(cmakeGenerator_) - 1);
            else
                strncpy(cmakeGenerator_, "Visual Studio 17 2022", sizeof(cmakeGenerator_) - 1);

            // Auto-detect vcpkg toolchain file for find_package support.
            // Strategy:
            //   1. Read CMAKE_TOOLCHAIN_FILE from the host project's CMakeCache.txt
            //      (most reliable — the editor itself was built with it).
            //   2. VCPKG_ROOT environment variable.
            //   3. Common install locations.
            if (vcpkgToolchain_[0] == '\0')
            {
                namespace fs = std::filesystem;

                auto tryToolchainFile = [&](const std::string& path) -> bool
                {
                    if (path.empty()) return false;
                    if (fs::exists(path))
                    {
                        strncpy(vcpkgToolchain_, path.c_str(), sizeof(vcpkgToolchain_) - 1);
                        return true;
                    }
                    return false;
                };

                auto tryVcpkgRoot = [&](const std::string& root) -> bool
                {
                    if (root.empty()) return false;
                    return tryToolchainFile(root + "/scripts/buildsystems/vcpkg.cmake");
                };

                // 1. Parse CMAKE_TOOLCHAIN_FILE from the host project's CMakeCache.txt.
                //    The exe lives in a cmake build dir — walk upward looking for the cache.
                {
                    char exePath[512] = {};
                    if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
                    {
                        fs::path dir = fs::path(exePath).parent_path();
                        for (int depth = 0; depth < 6 && !dir.empty(); ++depth, dir = dir.parent_path())
                        {
                            const fs::path cache = dir / "CMakeCache.txt";
                            std::ifstream f(cache);
                            if (!f.is_open()) continue;

                            std::string line;
                            while (std::getline(f, line))
                            {
                                // CMAKE_TOOLCHAIN_FILE:FILEPATH=<path>
                                const std::string key = "CMAKE_TOOLCHAIN_FILE:FILEPATH=";
                                const auto pos = line.find(key);
                                if (pos != std::string::npos)
                                {
                                    const std::string tc = line.substr(pos + key.size());
                                    if (tryToolchainFile(tc)) break;
                                }
                            }
                            if (vcpkgToolchain_[0] != '\0') break;
                        }
                    }
                }

                // 2. VCPKG_ROOT env var.
                if (vcpkgToolchain_[0] == '\0')
                    if (const char* env = std::getenv("VCPKG_ROOT"))
                        tryVcpkgRoot(env);

                // 3. Common install locations + sibling dirs of the project root.
                if (vcpkgToolchain_[0] == '\0')
                {
                    const char* home = std::getenv("USERPROFILE");
                    std::vector<std::string> candidates = { "C:/vcpkg", "C:/tools/vcpkg",
                                                            "D:/vcpkg", "D:/repos2/vcpkg",
                                                            "E:/vcpkg" };
                    if (home)
                    {
                        candidates.push_back(std::string(home) + "/vcpkg");
                        candidates.push_back(std::string(home) + "/source/repos/vcpkg");
                    }
                    // Also try vcpkg as a sibling of the exe's repo root
                    {
                        char exePath2[512] = {};
                        if (GetModuleFileNameA(nullptr, exePath2, sizeof(exePath2)))
                        {
                            fs::path dir = fs::path(exePath2).parent_path();
                            for (int d = 0; d < 6 && !dir.empty(); ++d, dir = dir.parent_path())
                                candidates.push_back((dir / "vcpkg").string());
                        }
                    }
                    for (const auto& c : candidates)
                        if (tryVcpkgRoot(c)) break;
                }
            }
#endif
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Generator");
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##bld_gen", cmakeGenerator_, sizeof(cmakeGenerator_));
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("cmake -G value (e.g. \"Ninja\", \"Visual Studio 17 2022\").\n"
                              "Auto-detected: prefers Ninja when VS is installed.\n"
                              "Leave blank = cmake default (may pick NMake on Windows).");

        pathField("vcvarsall", "##bld_vcvars", "...##bld_vcvars_btn",
                  vcvarsallPath_, sizeof(vcvarsallPath_), false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Path to vcvarsall.bat — used to initialize the MSVC environment\n"
                              "before cmake runs (required for Ninja + MSVC on Windows).");

        // Warn inline if the user accidentally picked the vcpkg executable
        if (vcpkgToolchain_[0] != '\0')
        {
            const std::string tc(vcpkgToolchain_);
            const bool isExe = tc.size() > 4 &&
                               (tc.substr(tc.size() - 4) == ".exe" ||
                                tc.substr(tc.size() - 4) == ".EXE");
            if (isExe)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
                ImGui::TextUnformatted("  [!] This looks like vcpkg.exe — select vcpkg.cmake instead:");
                ImGui::TextUnformatted("      <vcpkg-root>/scripts/buildsystems/vcpkg.cmake");
                ImGui::PopStyleColor();
            }
        }

        pathField("vcpkg toolchain", "##bld_vcpkg", "...##bld_vcpkg_btn",
                  vcpkgToolchain_, sizeof(vcpkgToolchain_), false,
                  { "CMake toolchain", "*.cmake", "All Files", "*" });
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Path to vcpkg.cmake — NOT vcpkg.exe.\n"
                              "Full path: <vcpkg-root>/scripts/buildsystems/vcpkg.cmake\n"
                              "e.g. D:\\repos2\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake\n\n"
                              "Passed to cmake as -DCMAKE_TOOLCHAIN_FILE so that\n"
                              "find_package(spdlog/SDL2/etc.) resolve correctly.\n"
                              "Auto-detected from VCPKG_ROOT or common install paths.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Build button ──────────────────────────────────────────────────────
        const bool isRunning = running_.load();
        const bool canBuild  = !isRunning
                               && sourcePath_[0]      != '\0'
                               && outputPath_[0]      != '\0'
                               && coreLibPath_[0]     != '\0'
                               && coreIncludePath_[0] != '\0';

        if (!canBuild) ImGui::BeginDisabled();

        if (ImGui::Button(isRunning ? "Building..." : "  Build  ", ImVec2(-1, 0)))
        {
            if (buildThread_.joinable()) buildThread_.join();

            {
                std::lock_guard<std::mutex> lk(logMutex_);
                log_.clear();
                statusMsg_ = "Starting...";
            }
            progress_.store(0.0f);
            running_.store(true);
            scrollToBottom_.store(true);

            RunPipeline(sourcePath_, outputPath_,
                        projectName_[0] ? projectName_ : "Game",
                        kConfigs[configIndex_], cleanBuild_,
                        coreLibPath_, coreIncludePath_,
                        cmakeGenerator_, vcvarsallPath_, vcpkgToolchain_);
        }

        if (!canBuild) ImGui::EndDisabled();

        if (!isRunning && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if      (sourcePath_[0]      == '\0') ImGui::SetTooltip("Set source path first");
            else if (outputPath_[0]      == '\0') ImGui::SetTooltip("Set output path first");
            else if (coreLibPath_[0]     == '\0') ImGui::SetTooltip("Set Core lib path first");
            else if (coreIncludePath_[0] == '\0') ImGui::SetTooltip("Set Core include path first");
        }

        ImGui::Spacing();

        // ── Progress ──────────────────────────────────────────────────────────
        const float prog = progress_.load();
        if (isRunning || prog > 0.0f)
        {
            ImGui::ProgressBar(prog, ImVec2(-1, 0));
            std::string status;
            { std::lock_guard<std::mutex> lk(logMutex_); status = statusMsg_; }
            ImGui::TextDisabled("%s", status.c_str());
        }

        ImGui::Spacing();

        // ── Output log ────────────────────────────────────────────────────────
        std::vector<std::string> snapshot;
        bool scrollNow = false;
        {
            std::lock_guard<std::mutex> lk(logMutex_);
            snapshot = log_;
            if (scrollToBottom_.load()) { scrollNow = true; scrollToBottom_.store(false); }
        }

        ImGui::SeparatorText("Output");

        if (ImGui::SmallButton("Copy log"))
        {
            std::string joined;
            joined.reserve(snapshot.size() * 80);
            for (const auto& line : snapshot) { joined += line; joined += '\n'; }
            ImGui::SetClipboardText(joined.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear##log"))
        {
            std::lock_guard<std::mutex> lk(logMutex_);
            log_.clear();
        }

        const float logH = std::max(ImGui::GetContentRegionAvail().y - 6.0f, 60.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
        if (ImGui::BeginChild("BuildLog", ImVec2(-1, logH), false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (const auto& line : snapshot)
            {
                ImVec4 col{0.82f, 0.82f, 0.82f, 1.0f};
                if      (line.rfind("[ERROR]",   0) == 0) col = {1.0f,  0.30f, 0.30f, 1.0f};
                else if (line.rfind("[WARNING]", 0) == 0) col = {1.0f,  0.80f, 0.20f, 1.0f};
                else if (line.rfind("[80CC]",    0) == 0) col = {0.40f, 0.85f, 0.45f, 1.0f};
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            if (scrollNow)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // RunPipeline  (background thread)
    // -------------------------------------------------------------------------

    void BuildPanel::RunPipeline(std::string srcPath, std::string outPath,
                                 std::string projName, std::string cfgStr, bool doClean,
                                 std::string coreLib, std::string coreInc,
                                 std::string generator, std::string vcvarsall,
                                 std::string vcpkgToolchain)
    {
        buildThread_ = std::thread([this,
                                    srcPath        = std::move(srcPath),
                                    outPath        = std::move(outPath),
                                    projName       = std::move(projName),
                                    cfgStr         = std::move(cfgStr),
                                    doClean,
                                    coreLib        = std::move(coreLib),
                                    coreInc        = std::move(coreInc),
                                    generator      = std::move(generator),
                                    vcvarsall      = std::move(vcvarsall),
                                    vcpkgToolchain = std::move(vcpkgToolchain)]()
        {
            namespace fs = std::filesystem;

            auto pushLog = [this](std::string msg)
            {
                std::lock_guard<std::mutex> lk(logMutex_);
                log_.push_back(std::move(msg));
                scrollToBottom_.store(true);
            };

            auto setStatus = [this](std::string s)
            {
                std::lock_guard<std::mutex> lk(logMutex_);
                statusMsg_ = std::move(s);
            };

            // buildOut = <output>/<projName>   — the generated project root
            // buildBin = <output>/<projName>/build — cmake build dir
            //
            // RunGenerator is passed outPath (NOT buildOut) because RunGenerator
            // itself appends projName to form the project root.  Passing buildOut
            // would cause double-nesting: <outPath>/<projName>/<projName>/
            const fs::path buildOut = fs::path(outPath) / projName;
            const fs::path buildBin = buildOut / "build";


            // Run a subprocess, stream output line-by-line into the log.
            // On Windows with vcvarsall set, wraps the command in a temp .bat file so
            // MSVC environment variables are initialized before cmake/build runs.
            auto runCmd = [&](const std::string& cmd) -> bool
            {
#ifdef _WIN32
                // Write a temp batch file: init env, run cmd, exit with its code.

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
                if (!pipe) { pushLog("[ERROR] Failed to launch: " + cmd); return false; }
                char lineBuf[512];
                while (fgets(lineBuf, sizeof(lineBuf), pipe))
                {
                    std::string line(lineBuf);
                    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                        line.pop_back();
                    pushLog(std::move(line));
                }
#ifdef _WIN32
                const int rc = _pclose(pipe);
                std::error_code ec; fs::remove(batFile, ec);
                return rc == 0;
#else
                return pclose(pipe) == 0;
#endif
            };

            // ── Stage 1: Clean ────────────────────────────────────────────────
            if (doClean && fs::exists(buildBin))
            {
                setStatus("Cleaning...");
                pushLog("[80CC] Removing: " + buildBin.string());
                try { fs::remove_all(buildBin); }
                catch (const std::exception& e)
                { pushLog(std::string("[ERROR] Clean failed: ") + e.what()); }
            }

            try { fs::create_directories(buildBin); }
            catch (const std::exception& e)
            {
                pushLog(std::string("[ERROR] Cannot create output dir: ") + e.what());
                setStatus("Failed."); running_.store(false); return;
            }
            progress_.store(0.08f);

            // ── Stage 2: Generator (inline — no subprocess) ───────────────────
            setStatus("Running generator...");
            pushLog("[80CC] --- Stage 1/3: Generator ---");

            // Pass outPath (not buildOut) — RunGenerator appends projName itself.
            const bool genOk = ettycc::build::RunGenerator(
                srcPath, outPath, projName, coreLib, coreInc,
                /*templateHint=*/"",
                [&](const std::string& msg) { pushLog(msg); });

            if (!genOk)
            {
                pushLog("[ERROR] Generator failed — aborting.");
                setStatus("Generator failed."); running_.store(false); return;
            }
            progress_.store(0.38f);

            // ── Stage 3: cmake configure ──────────────────────────────────────
            setStatus("Configuring...");
            pushLog("[80CC] --- Stage 2/3: cmake configure ---");

            // Wipe CMakeCache.txt and CMakeFiles/ before configuring.
            // cmake refuses to reconfigure if the generator changed (e.g. NMake -> Ninja),
            // so we always remove the stale cache to guarantee a clean configure.
            {
                std::error_code ec;
                fs::remove(buildBin / "CMakeCache.txt", ec);
                fs::remove_all(buildBin / "CMakeFiles", ec);
            }

            // buildOut already has the generated CMakeLists.txt from the generator step.
            std::string configCmd = "cmake -S \"" + buildOut.string()
                + "\" -B \"" + buildBin.string()
                + "\" -DCMAKE_BUILD_TYPE=" + cfgStr;
            if (!generator.empty())
            {
                configCmd += " -G \"" + generator + "\"";
                // -A x64 is only valid for Visual Studio generators
                if (generator.find("Visual Studio") != std::string::npos)
                    configCmd += " -A x64";
            }
            if (!vcpkgToolchain.empty() && std::filesystem::exists(vcpkgToolchain))
                configCmd += " -DCMAKE_TOOLCHAIN_FILE=\"" + vcpkgToolchain + "\"";
            pushLog("[80CC] " + configCmd);

            if (!runCmd(configCmd))
                pushLog("[WARNING] cmake configure returned non-zero — build may still succeed.");

            progress_.store(0.60f);

            // ── Stage 4: cmake --build ────────────────────────────────────────
            setStatus("Compiling...");
            pushLog("[80CC] --- Stage 3/3: cmake build ---");

            const std::string buildCmd = "cmake --build \"" + buildBin.string()
                + "\" --config " + cfgStr + " --parallel";
            pushLog("[80CC] " + buildCmd);

            const bool ok = runCmd(buildCmd);
            progress_.store(1.0f);

            if (ok)
            {
                pushLog("[80CC] ===========================");
                pushLog("[80CC]  Build complete!");
                pushLog("[80CC] ===========================");
                setStatus("Done!");
            }
            else
            {
                pushLog("[ERROR] ===========================");
                pushLog("[ERROR]  Build FAILED.");
                pushLog("[ERROR] ===========================");
                setStatus("Failed.");
            }

            running_.store(false);
        });
    }

} // namespace ettycc
