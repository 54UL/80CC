#include <App/SDL2App.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

namespace etycc
{
    SDL2App::SDL2App(/* args */)
    {
    }

    SDL2App::~SDL2App()
    {
    }

    int SDL2App::Init(int argc, char **argv)
    {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            // Handle SDL initialization error
            return 0;
        }

        // Create SDL window and OpenGL context
        window_ = SDL_CreateWindow("80CC[engine][unknown game name]", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window_)
        {
            // Handle window creation error
            SDL_Quit();
            return 1;
        }

        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_EGL);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        // Create OpenGL context
        glContext_ = SDL_GL_CreateContext(window_);
        // Make the OpenGL context current
        SDL_GL_MakeCurrent(window_, glContext_);

        spdlog::warn("OpenGL Version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

        if (!glContext_) {
            spdlog::error("Error creating OpenGL context: {}",SDL_GetError());
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }

        // Initialize GLEW
        GLenum glewError = glewInit();
        if (glewError != GLEW_OK) {
            const char* glewErrorString = reinterpret_cast<const char*>(glewGetErrorString(glewError));
            std::cerr << "Error initializing GLEW: " << glewGetErrorString(glewError) << std::endl;
            SDL_GL_DeleteContext(glContext_);
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }

        // Check if OpenGL 4.0 is supported
        if (!GLEW_VERSION_4_1)
        {
            // Handle lack of OpenGL 4.0 support
            spdlog::error("OpenGL 3.3 not supported");
            SDL_GL_DeleteContext(glContext_);
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }
        // Your additional OpenGL setup code can go here
        return 0;
    }

    // MAIN ENTRY POINT!!!!
    int SDL2App::Exec()
    {
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

            // Your rendering code goes here

            // Swap the SDL window buffer
            SDL_GL_SwapWindow(window_);
        }

        // Cleanup
        SDL_GL_DeleteContext(glContext_);
        SDL_DestroyWindow(window_);
        SDL_Quit();

        return 0;
    }
}
