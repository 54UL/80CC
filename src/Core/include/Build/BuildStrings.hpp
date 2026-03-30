#pragma once

namespace ettycc::build::str
{
    // ── Window / section / field labels ──────────────────────────────────────
    inline constexpr const char* WIN_BUILD            = "Build";
    inline constexpr const char* WIN_CONFIGURATIONS   = "Configurations";

    inline constexpr const char* SEC_PROJECT          = "Project";
    inline constexpr const char* SEC_CONFIGURATION    = "Configuration";
    inline constexpr const char* SEC_80CC_CORE        = "80CC Core";
    inline constexpr const char* SEC_BUILD_SETTINGS   = "Build Settings";
    inline constexpr const char* SEC_ENGINE_GLOBALS   = "Engine Globals";
    inline constexpr const char* SEC_OUTPUT           = "Output";

    inline constexpr const char* FLD_PROJECT_NAME     = "Project name";
    inline constexpr const char* FLD_SOURCE           = "Source";
    inline constexpr const char* FLD_OUTPUT           = "Output";
    inline constexpr const char* FLD_CORE_LIB         = "Core lib";
    inline constexpr const char* FLD_CORE_INCLUDE     = "Core include";
    inline constexpr const char* FLD_GENERATOR        = "Generator";
    inline constexpr const char* FLD_VCVARSALL        = "vcvarsall";
    inline constexpr const char* FLD_VCPKG_TOOLCHAIN  = "vcpkg toolchain";

    // ── Button labels ─────────────────────────────────────────────────────────
    inline constexpr const char* BTN_BUILD            = "  Build  ";
    inline constexpr const char* BTN_BUILDING         = "Building...";
    inline constexpr const char* BTN_BROWSE           = "...";
    inline constexpr const char* BTN_COPY_LOG         = "Copy log";
    inline constexpr const char* BTN_CLEAR_LOG        = "Clear##log";
    inline constexpr const char* BTN_AUTO_DETECT      = "Auto-detect";
    inline constexpr const char* BTN_SAVE_CFG         = "Save##cfg";
    inline constexpr const char* BTN_IMPORT_CFG       = "Import...##cfg";
    inline constexpr const char* BTN_EXPORT_CFG       = "Export...##cfg";
    inline constexpr const char* BTN_SAVE_GLOBALS     = "Save Globals";

    // ── Log prefixes ──────────────────────────────────────────────────────────
    inline constexpr const char* LOG_80CC             = "[80CC]";
    inline constexpr const char* LOG_ERROR            = "[ERROR]";
    inline constexpr const char* LOG_WARNING          = "[WARNING]";

    // ── Status messages ───────────────────────────────────────────────────────
    inline constexpr const char* STS_STARTING         = "Starting...";
    inline constexpr const char* STS_CLEANING         = "Cleaning...";
    inline constexpr const char* STS_RUNNING_GEN      = "Running generator...";
    inline constexpr const char* STS_CONFIGURING      = "Configuring...";
    inline constexpr const char* STS_COMPILING        = "Compiling...";
    inline constexpr const char* STS_PACKING          = "Packing...";
    inline constexpr const char* STS_CLEANING_UP      = "Cleaning up...";
    inline constexpr const char* STS_DONE             = "Done!";
    inline constexpr const char* STS_FAILED           = "Failed.";
    inline constexpr const char* STS_GEN_FAILED       = "Generator failed.";

    // ── Tooltip texts ─────────────────────────────────────────────────────────
    inline constexpr const char* TIP_SOURCE =
        "Game source root — must contain include/ and src/ directly.\n"
        "e.g. assets/src  (not assets/)";

    inline constexpr const char* TIP_OUTPUT =
        "Parent folder for the generated project.\n"
        "Generator creates <Output>/<ProjectName>/";

    inline constexpr const char* TIP_CORE_LIB =
        "Pre-built 80CC_CORE.lib / lib80CC_CORE.a\n"
        "(auto-detected if next to the editor executable)";

    inline constexpr const char* TIP_CORE_INCLUDE =
        "80CC_CORE include/ directory — contains Engine.hpp, 80CC.hpp, etc.";

    inline constexpr const char* TIP_GENERATOR =
        "cmake -G value (e.g. \"Ninja\", \"Visual Studio 17 2022\").\n"
        "Auto-detected: prefers Ninja when VS is installed.\n"
        "Leave blank = cmake default (may pick NMake on Windows).";

    inline constexpr const char* TIP_VCVARSALL =
        "Path to vcvarsall.bat — used to initialize the MSVC environment\n"
        "before cmake runs (required for Ninja + MSVC on Windows).";

    inline constexpr const char* TIP_VCPKG =
        "Path to vcpkg.cmake — NOT vcpkg.exe.\n"
        "Full path: <vcpkg-root>/scripts/buildsystems/vcpkg.cmake\n"
        "e.g. D:\\repos2\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake\n\n"
        "Passed to cmake as -DCMAKE_TOOLCHAIN_FILE so that\n"
        "find_package(spdlog/SDL2/etc.) resolve correctly.\n"
        "Auto-detected from VCPKG_ROOT or common install paths.";

    inline constexpr const char* TIP_KEEP_GENERATED =
        "Keep the full generated project (source, CMakeLists, build/).\n"
        "When unchecked only the dist/ folder is kept after a successful build.";

    inline constexpr const char* TIP_BUILD_DISABLED_SRC  = "Set source path first";
    inline constexpr const char* TIP_BUILD_DISABLED_OUT  = "Set output path first";
    inline constexpr const char* TIP_BUILD_DISABLED_CLIB = "Set Core lib path first";
    inline constexpr const char* TIP_BUILD_DISABLED_CINC = "Set Core include path first";

    // ── Auto-detection paths ──────────────────────────────────────────────────
    inline constexpr const char* PATH_VSWHERE_EXE =
        "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe";

    inline constexpr const char* PATH_VSWHERE_CMD =
        "\"C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe\""
        " -latest -property installationPath 2>nul";

    inline constexpr const char* PATH_VS_NINJA_REL =
        "\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe";

    inline constexpr const char* PATH_VCVARSALL_REL =
        "\\VC\\Auxiliary\\Build\\vcvarsall.bat";

    // ── Generator names ───────────────────────────────────────────────────────
    inline constexpr const char* GEN_NINJA            = "Ninja";
    inline constexpr const char* GEN_VS_2022          = "Visual Studio 17 2022";

    // ── cmake / vcpkg constants ───────────────────────────────────────────────
    inline constexpr const char* CMAKE_TOOLCHAIN_KEY  = "CMAKE_TOOLCHAIN_FILE:FILEPATH=";
    inline constexpr const char* VCPKG_TOOLCHAIN_REL  = "/scripts/buildsystems/vcpkg.cmake";
    inline constexpr const char* NINJA_CHECK_CMD      = "ninja --version 2>nul";

    // ── Category names ────────────────────────────────────────────────────────
    inline constexpr const char* CAT_BUILD            = "Build";
    inline constexpr const char* CAT_GLOBALS          = "Globals";

    // ── vcpkg exe warning ─────────────────────────────────────────────────────
    inline constexpr const char* WARN_VCPKG_EXE_1 =
        "  [!] This looks like vcpkg.exe — select vcpkg.cmake instead:";
    inline constexpr const char* WARN_VCPKG_EXE_2 =
        "      <vcpkg-root>/scripts/buildsystems/vcpkg.cmake";

} // namespace ettycc::build::str
