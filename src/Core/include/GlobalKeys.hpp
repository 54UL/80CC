#pragma once

// GlobalKeys — canonical string constants for all Globals key-value pairs.
//
// Every prefix, key, environment variable, and JSON anchor used by the
// engine's Globals system is defined here so callers never repeat magic
// string literals.
//
// Usage:
//   globals->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);

namespace ettycc::gk
{
    // ── JSON serialization ─────────────────────────────────────────────────
    inline constexpr const char* JSON_ROOT     = "engine_data";
    inline constexpr const char* JSON_FILENAME = "80CC.json";

    // ── Environment variables ──────────────────────────────────────────────
    inline constexpr const char* ENV_ASSETS_ROOT = "ASSETS_80CC";

    // ── Prefixes (resource groups) ─────────────────────────────────────────
    namespace prefix
    {
        inline constexpr const char* APP     = "app";
        inline constexpr const char* PATHS   = "paths";
        inline constexpr const char* SPRITES = "sprites";
        inline constexpr const char* SHADERS = "shaders";
        inline constexpr const char* STATE   = "state";
        inline constexpr const char* ENGINE  = "engine";  // runtime engine paths
    }

    // ── Keys ──────────────────────────────────────────────────────────────
    namespace key
    {
        // app
        inline constexpr const char* APP_TITLE      = "title";
        inline constexpr const char* APP_FLAGS       = "flags";
        inline constexpr const char* APP_RESOLUTION  = "resolution";

        // paths
        inline constexpr const char* PATH_CONFIG     = "config";
        inline constexpr const char* PATH_IMAGES     = "images";
        inline constexpr const char* PATH_SCENES     = "scenes";
        inline constexpr const char* PATH_SHADERS    = "shaders";
        inline constexpr const char* PATH_TEMPLATES  = "templates";
        inline constexpr const char* PATH_MATERIALS  = "materials";

        // sprites
        inline constexpr const char* SPRITE_NOT_FOUND = "not-found";

        // state
        inline constexpr const char* STATE_LAST_SCENE = "last_scene";

        // engine  (populated at runtime by Engine::ConfigResource)
        inline constexpr const char* ENGINE_EXE_DIR     = "exe_dir";      // directory containing the editor executable
        inline constexpr const char* ENGINE_WORKING_DIR = "working_dir";  // resolved assets/working folder
    }

} // namespace ettycc::gk
