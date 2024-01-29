#include <App/SDL2App.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <functional>


namespace etycc
{
    SDL2App::SDL2App(/* args */):runningStatus_(true)
    {
    }

    SDL2App::~SDL2App()
    {
        this->Dispose();
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
        window_ = SDL_CreateWindow("80CC", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window_)
        {
            // Handle window creation error
            SDL_Quit();
            return 1;
        }

        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_EGL);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        // Create OpenGL context and make it the current context
        glContext_ = SDL_GL_CreateContext(window_);
        SDL_GL_MakeCurrent(window_, glContext_);

        if (!glContext_)
        {
            spdlog::error("Error creating OpenGL context: {}", SDL_GetError());
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }
        else
        {
            spdlog::warn("OpenGL Version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
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
        if (!GLEW_VERSION_3_3)
        {
            // Handle lack of OpenGL 4.0 support
            spdlog::error("OpenGL 3.3 not supported");
            SDL_GL_DeleteContext(glContext_);
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return 1;
        }

        OpenGLInit();

        // Initialize event thread to handle window input
        this->InitEventThread();

        return 0;
    }

    void SDL2App::OpenGLInit(){
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    }

    void SDL2App::PrepareFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT);
    } 

    // WARNING: MAIN ENTRY POINT (TECHNICALLY)
    int SDL2App::Exec()
    {
        while (this->IsRunning())
        {
            AppInput();
            PrepareFrame();
            // rendering code...
            // here goes all rendering callbacks...
            SDL_GL_SwapWindow(window_);
        }
        return 0;
    }

    void SDL2App::Dispose()
    {
        // Cleanup
        int eventThreadReturnValue;
        SDL_WaitThread(eventThread_, &eventThreadReturnValue);

        // Destroy the mutex
        SDL_DestroyMutex(eventMutex_);

        // Destroy gl and window...
        SDL_GL_DeleteContext(glContext_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void SDL2App::SetRunningStatus(bool running)
    {
        SDL_LockMutex(eventMutex_);

        this->runningStatus_ = running;

        SDL_UnlockMutex(eventMutex_);
    }

    bool SDL2App::IsRunning()
    {
        return this->runningStatus_;
    }

    int SDL2App::EventCallback(void *instance)
    {
        SDL2App *appInstance = static_cast<SDL2App *>(instance);

        SDL_Event event;
        while (appInstance->IsRunning())
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    appInstance->SetRunningStatus(0);
                    return 0;

                case SDL_KEYDOWN:
                    // Handle Key Down Event
                    // handleKeyDownEvent(event.key.keysym.sym);
                    std::cout << "keydown:" << event.key.keysym.sym << "\n";
                    break;

                case SDL_KEYUP:
                    // Handle Key Up Event
                    // handleKeyUpEvent(event.key.keysym.sym);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // Handle Mouse Button Down Event
                    // handleMouseButtonDownEvent(event.button.button, event.button.x, event.button.y);
                    break;

                case SDL_MOUSEBUTTONUP:
                    // Handle Mouse Button Up Event
                    // handleMouseButtonUpEvent(event.button.button, event.button.x, event.button.y);
                    break;

                case SDL_MOUSEMOTION:
                    // Handle Mouse Motion Event
                    // handleMouseMotionEvent(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                    break;

                case SDL_MOUSEWHEEL:
                    // Handle Mouse Wheel Event
                    // handleMouseWheelEvent(event.wheel.x, event.wheel.y);
                    break;

                case SDL_JOYBUTTONDOWN:
                    // Handle Joystick Button Down Event
                    // handleJoystickButtonDownEvent(event.jbutton.which, event.jbutton.button);
                    break;

                case SDL_JOYBUTTONUP:
                    // Handle Joystick Button Up Event
                    // handleJoystickButtonUpEvent(event.jbutton.which, event.jbutton.button);
                    break;

                case SDL_JOYAXISMOTION:
                    // Handle Joystick Axis Motion Event
                    // handleJoystickAxisMotionEvent(event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                    break;

                case SDL_JOYHATMOTION:
                    // Handle Joystick Hat Motion Event
                    // handleJoystickHatMotionEvent(event.jhat.which, event.jhat.hat, event.jhat.value);
                    break;

                    // case SDL_WINDOWEVENT:
                    //     switch (event.window.event)
                    //     {
                    //     case SDL_WINDOWEVENT_RESIZED:
                    //         // handleWindowResizedEvent(event.window.data1, event.window.data2);
                    //         break;
                    //     default:
                    //         break;
                    //     }

                    //     break;
                }
            }
            SDL_Delay(1);
        }
        return 0;
    }

    void SDL2App::AppInput()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                SetRunningStatus(0);
                return;

            case SDL_KEYDOWN:
                // Handle Key Down Event
                // handleKeyDownEvent(event.key.keysym.sym);
                std::cout << "keydown:" << event.key.keysym.sym << "\n";
                break;

            case SDL_KEYUP:
                // Handle Key Up Event
                // handleKeyUpEvent(event.key.keysym.sym);
                break;

            case SDL_MOUSEBUTTONDOWN:
                // Handle Mouse Button Down Event
                // handleMouseButtonDownEvent(event.button.button, event.button.x, event.button.y);
                break;

            case SDL_MOUSEBUTTONUP:
                // Handle Mouse Button Up Event
                // handleMouseButtonUpEvent(event.button.button, event.button.x, event.button.y);
                break;

            case SDL_MOUSEMOTION:
                // Handle Mouse Motion Event
                // handleMouseMotionEvent(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                break;

            case SDL_MOUSEWHEEL:
                // Handle Mouse Wheel Event
                // handleMouseWheelEvent(event.wheel.x, event.wheel.y);
                break;
            }
        }
        SDL_Delay(1);
        
    }

    // TODO: INIT EVENT THREAD IS NOT USED ANYMORE
    void SDL2App::InitEventThread()
    {
        // Create a thread for event handling
        eventThread_ = SDL_CreateThread(SDL2App::EventCallback, "EventThread", this);
        if (eventThread_ == NULL)
        {
            // MNE_Log("Cannot create event thread....\n");
        }

        // Create a mutex
        eventMutex_ = SDL_CreateMutex();
        if (!eventMutex_)
        {
            std::cerr << "Failed to create mutex: " << SDL_GetError() << std::endl;
        }
    }

    SDL_Window *SDL2App::GetMainWindow()
    {
        return this->window_;
    }

    SDL_GLContext *SDL2App::GetGLContext()
    {
        return &this->glContext_;
    }
}
