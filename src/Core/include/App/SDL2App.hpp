#ifndef SDL_TINY_APP_H
#define SDL_TINY_APP_H
#include <App/App.hpp>

#include <stdint.h>
#include <80CC.hpp>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>

namespace etycc
{
    class SDL2App : public App
    {
    private:
        SDL_Window *window_;
        SDL_GLContext glContext_;

    public:
        SDL2App(/* args */);
        ~SDL2App();

        int Init(int argc, char **argv) override;
        int Exec() override;
    };
}

#endif