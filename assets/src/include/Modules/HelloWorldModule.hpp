#ifndef HELLO_WORLD_MODULE_HPP
#define HELLO_WORLD_MODULE_HPP

// HelloWorldGameModule — example game entry point.
//
// This is the FIRST module registered with the engine in standalone mode.
// Think of it as the root of your game: it owns the top-level state machine
// (main menu → loading → playing → paused → game over) and decides which
// scene to load at each stage.
//
// Every other game-specific module (HUD, inventory, AI director, etc.) should
// be kicked off from here, either by loading scenes that contain those modules
// or by explicitly calling engine APIs.

#include <80CC.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    class HelloWorldGameModule : public GameModule
    {
    public:
        // ── Game state ────────────────────────────────────────────────────────
        enum class State
        {
            BOOT,       // first frame, one-time initialization
            MAIN_MENU,  // title / menu screen
            LOADING,    // transitioning between scenes
            PLAYING,    // in-game
            PAUSED,     // overlay pause menu
            GAME_OVER   // death / end-of-game screen
        };

        HelloWorldGameModule()
        {
            name_    = "HelloWorldGameModule";
            state_   = State::BOOT;
            engine_  = nullptr;
            timer_   = 0.0f;
        }

        ~HelloWorldGameModule() override = default;

        // ── GameModule interface ──────────────────────────────────────────────

        bool OnStart(const Engine* engine) override
        {
            // NOTE: Engine::OnStart takes const Engine* to keep the interface
            // stable, but game modules often need to mutate engine state.
            // Store as non-const via cast — this is intentional and safe here
            // because the module and engine share the same lifetime.
            engine_ = const_cast<Engine*>(engine);

            spdlog::info("[{}] OnStart — booting game", name_);

            // Load the main menu scene.
            // Scenes live in assets/scenes/ relative to the working folder.
            LoadScene("default_scene.json");

            state_ = State::MAIN_MENU;
            spdlog::info("[{}] State -> MAIN_MENU", name_);
            return true;
        }

        void OnUpdate(const float deltaTime) override
        {
            if (!engine_) return;
            timer_ += deltaTime;

            switch (state_)
            {
            case State::MAIN_MENU:
                UpdateMainMenu(deltaTime);
                break;

            case State::LOADING:
                UpdateLoading(deltaTime);
                break;

            case State::PLAYING:
                UpdatePlaying(deltaTime);
                break;

            case State::PAUSED:
                UpdatePaused(deltaTime);
                break;

            case State::GAME_OVER:
                UpdateGameOver(deltaTime);
                break;

            default:
                break;
            }
        }

        void OnDestroy() override
        {
            spdlog::info("[{}] OnDestroy", name_);
        }

        // ── Public game API ───────────────────────────────────────────────────

        void StartGame()
        {
            spdlog::info("[{}] StartGame — loading level", name_);
            pendingScene_ = "default_scene.json"; // swap with your first level scene
            state_        = State::LOADING;
            timer_        = 0.0f;
        }

        void PauseGame()
        {
            if (state_ != State::PLAYING) return;
            spdlog::info("[{}] Game paused", name_);
            state_ = State::PAUSED;
        }

        void ResumeGame()
        {
            if (state_ != State::PAUSED) return;
            spdlog::info("[{}] Game resumed", name_);
            state_ = State::PLAYING;
        }

        void GoToMainMenu()
        {
            spdlog::info("[{}] Returning to main menu", name_);
            LoadScene("default_scene.json");
            state_ = State::MAIN_MENU;
            timer_ = 0.0f;
        }

        State GetState() const { return state_; }

    private:
        Engine* engine_;
        State   state_;
        float   timer_;           // time spent in the current state
        std::string pendingScene_; // scene queued for LOADING state

        // ── State handlers ────────────────────────────────────────────────────

        void UpdateMainMenu(float /*dt*/)
        {
            // TODO: render a proper title screen / menu via the scene system.
            // For now just log once per second as a heartbeat.
            if (static_cast<int>(timer_) % 5 == 0 && timer_ > 0.5f)
            {
                spdlog::debug("[{}] MAIN_MENU — waiting for player input (timer={:.1f}s)",
                              name_, timer_);
            }

            // Example: auto-start after 3 seconds (remove once you have real input)
            if (timer_ >= 3.0f)
                StartGame();
        }

        void UpdateLoading(float dt)
        {
            // Give the engine one frame to settle after scene load,
            // then transition to PLAYING.
            if (timer_ < dt * 2.0f)
            {
                LoadScene(pendingScene_);
            }
            else
            {
                spdlog::info("[{}] Load complete — State → PLAYING", name_);
                state_ = State::PLAYING;
                timer_ = 0.0f;
            }
        }

        void UpdatePlaying(float /*dt*/)
        {
            // Core gameplay loop — delegate to scene components / other modules.
            // Example: listen for pause input via engine_->GetInputSystem().
        }

        void UpdatePaused(float /*dt*/)
        {
            // Show pause overlay, handle resume / quit.
        }

        void UpdateGameOver(float dt)
        {
            // After 5 seconds on the game-over screen, return to menu.
            if (timer_ >= 5.0f)
                GoToMainMenu();
        }

        // ── Helpers ───────────────────────────────────────────────────────────

        void LoadScene(const std::string& sceneName)
        {
            if (!engine_) return;
            spdlog::info("[{}] Loading scene: {}", name_, sceneName);
            engine_->LoadScene(sceneName); // uses default assets/scenes/ path
        }
    };

} // namespace ettycc

#endif
