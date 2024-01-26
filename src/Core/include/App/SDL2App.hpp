#ifndef SDL_TINY_APP_H
#define SDL_TINY_APP_H
#include <App/App.hpp>

#include <stdint.h>
#include <80CC.hpp>
// #include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>

namespace etycc
{
    class SDL2App : public App
    {
    public:
        enum class AppEventType
        {
            KEYBOARD,
            MOUSE,
            JOYSTICK,
            WINDOW,
            QUIT
        };

        class AppEvent
        {
        private:
            /* data */
        public:
            AppEvent(/* args */) {}
            ~AppEvent() {}
        };

    private:
        SDL_Window *window_;
        SDL_GLContext glContext_;
        bool runningStatus_;
        SDL_Thread *eventThread_;
        SDL_mutex  *eventMutex_;

    public:
        SDL2App();
        ~SDL2App();

        int Init(int argc, char **argv) override;
        int Exec() override;
        void Dispose();

        void SetRunningStatus(bool running);
        bool IsRunning();

        SDL_Window* GetMainWindow();
        SDL_GLContext* GetGLContext();
        // Event listeners
        void AddEventListener(AppEvent type);
        
    private:
        static int EventCallback(void * data);
        void InitEventThread();
    };
}

#endif