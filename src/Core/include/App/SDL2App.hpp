#ifndef SDL_TINY_APP_H
#define SDL_TINY_APP_H
#include <App/App.hpp>
#include <memory>
#include <stdint.h>
#include <80CC.hpp>
#include <GL/glew.h>

#include <GL/gl.h>
#include <SDL.h>
#include <SDL_opengl.h>

// RENDERING TEST INCLUDES
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Input/Controls/GhostCamera.hpp>

#include <Input/PlayerInput.hpp>
// #include <Input/Controls/GhostCamera.hpp>

namespace ettycc
{
    class SDL2App : public App
    {
    private:
        SDL_Window *window_;
        SDL_GLContext glContext_;
        
        bool runningStatus_;
        SDL_Thread *eventThread_;
        SDL_mutex  *eventMutex_;

        // DEPENDENCIES:
        Rendering renderEngine_;
        PlayerInput inputSystem_;
        std::shared_ptr<GhostCamera> ghostCamera_;
        float currentDeltaTime_;
        
    public:
         SDL2App();
        ~SDL2App();

    public:
        int Init(int argc, char **argv) override;
        int Exec() override;
        void Dispose();
        SDL_Window* GetMainWindow();
        SDL_GLContext* GetGLContext();

    private:
        void AppInput();
        void SetRunningStatus(bool running);
        bool IsRunning();
        // TODO: INIT EVENT THREAD IS NOT USED ANYMORE
        
        // App execution pipeline
        void AppLogic();
        void RenderingInit();
        void PrepareFrame();
        void PresentFrame();
        
        // RENDERING TESTING 
        void RenderingEngineDemo();
    };
}

#endif