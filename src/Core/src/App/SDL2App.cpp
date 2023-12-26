#include <App/SDL2App.hpp>
#include <spdlog/spdlog.h>
// #include <imgui.h>

namespace etycc
{
    SDL2App::SDL2App(/* args */)
    {
    }

    SDL2App::~SDL2App()
    {
    }

    void SDL2App::Init(int argc, char **argv)
    {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            // Handle SDL initialization error
            return;
        }

        // Create SDL window and OpenGL context
        window_ = SDL_CreateWindow("80CC[engine][unkown game name]", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
        if (!window_)
        {
            // Handle window creation error
            SDL_Quit();
            return;
        }

        // Set OpenGL attributes for bgfx
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        // Create OpenGL context
        glContext_ = SDL_GL_CreateContext(window_);
        if (!glContext_)
        {
            // Handle context creation error
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return;
        }

        // Initialize bgfx
        // bgfx::PlatformData pd;
        // pd.ndt = SDL_GL_GetProcAddress;
        // pd.nwh = SDL_GL_GetCurrentWindow();
        // bgfx::setPlatformData(pd);

        // if (!bgfx::init(bgfx::RendererType::OpenGL))
        // {
        //     // Handle bgfx initialization error
        //     SDL_GL_DeleteContext(glContext_);
        //     SDL_DestroyWindow(window_);
        //     SDL_Quit();
        //     return;
        // }
    }

    int SDL2App::Exec()
    {
        // Your application loop goes here

        SDL_Event event;
        bool quit = false;

        while (!quit)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }
            }

            spdlog::info("rendering code goes here...");
            // Swap the SDL window buffer
            SDL_GL_SwapWindow(window_);
        }

        // Cleanup
        // bgfx::shutdown();
        SDL_GL_DeleteContext(glContext_);
        SDL_DestroyWindow(window_);
        SDL_Quit();

        return 0;
    }
}