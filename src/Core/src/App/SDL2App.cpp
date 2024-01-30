#include <App/SDL2App.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <functional>

// #include <imgui.h>
// #include <backends/imgui_impl_sdl2.h>
// #include <backends/imgui_impl_opengl3.h>

namespace ettycc
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

        renderEngine_.InitGraphicsBackend();

        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_EGL);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
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

        RenderingInit();

        RenderingEngineDemo();
        // Initialize event thread to handle window input
        // this->InitEventThread();

        return 0;
    }

    void SDL2App::RenderingInit() {
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_BACK);
        
        // Setup ImGui
        // IMGUI_CHECKVERSION();
        // ImGui::CreateContext();
 
        // ImGui_ImplSDL2_InitForOpenGL(window_, glContext_);
        // ImGui_ImplOpenGL3_Init("#version 330 core");
    }

    void SDL2App::PrepareFrame()
    {
        //Get io for imgui
        // ImGuiIO& io = ImGui::GetIO();
        // (void)io;
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable docking

        // Start the ImGui frame
        // ImGui_ImplOpenGL3_NewFrame();
        // ImGui_ImplSDL2_NewFrame(window_);
        // ImGui::NewFrame();

        // ImGui content goes here
        // Dockspace
        // ImGui::DockSpaceOverViewport();

        // // Your dockable windows
        // if (ImGui::BeginDock("Dockable Window 1"))
        // {
        //     // Content of the first dockable window
        //     ImGui::Text("Hello, this is Dockable Window 1!");
        //     ImGui::EndDock();
        // }

        // if (ImGui::BeginDock("Dockable Window 2"))
        // {
        //     // Content of the second dockable window
        //     ImGui::Text("Hello, this is Dockable Window 2!");
        //     ImGui::EndDock();
        // }
        
        // Example: A simple window
        // ImGui::Begin("80CC has ImGui!");
        // ImGui::Text("This is a simple ImGui example, press to quit");
        // if (ImGui::Button("Quit")) {
        //     this->SetRunningStatus(false);
        // }
        // ImGui::End();

        // Clear renderer...
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void SDL2App::PresentFrame()
    {
        // User code rendering
        renderEngine_.Pass();

        // ImGui rendering
        // ImGui::Render();
        // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }

    void SDL2App::RenderingEngineDemo()
    {
        int width, height;
        SDL_GetWindowSize(window_, &width, &height);
        renderEngine_.SetScreenSize(width, height);

        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(width,height,90,0.01f);
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>();
        mainCamera->underylingTransform.setGlobalPosition(glm::vec3(0,0,-2));
        //   mainCamera->underylingTransform.setGlobalRotation(glm::vec3(0,-90,0));
        ghostCamera_ = std::make_shared<GhostCamera>(&inputSystem_, mainCamera);

        renderEngine_.AddRenderable(mainCamera);
        renderEngine_.AddRenderable(someSprite);
    } 
    
    // WARNING: MAIN ENTRY POINT/THREAD (TECHNICALLY)
    int SDL2App::Exec()
    {
        while (this->IsRunning())
        {
            AppInput();
            AppLogic(); //todo move this to another thread???
            PrepareFrame();
            PresentFrame();
        }
        return 0;
    }

    void SDL2App::AppLogic() 
    {
        ghostCamera_->Update(0);
    }

    void SDL2App::Dispose()
    {
        // Cleanup
        // int eventThreadReturnValue;
        // SDL_WaitThread(eventThread_, &eventThreadReturnValue);

        // Destroy the mutex
        // SDL_DestroyMutex(eventMutex_);

        // Cleanup ImGui
        // ImGui_ImplOpenGL3_Shutdown();
        // ImGui_ImplSDL2_Shutdown();
        // ImGui::DestroyContext();

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


    void SDL2App::AppInput()
    {
        SDL_Event event;
        uint64_t data[2];

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
                data[0] = event.key.keysym.sym;
                inputSystem_.ProcessInput(PlayerInputType::KEYBOARD, data);
                break;
            case SDL_MOUSEMOTION:
                // Handle Mouse Motion Event
                // handleMouseMotionEvent(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                data[0] = event.motion.xrel;
                data[1] = event.motion.yrel;
                std::cout << "xrel:" << event.motion.xrel<< " yrel:"<<event.motion.yrel <<"\n";

                inputSystem_.ProcessInput(PlayerInputType::MOUSE, data);

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


            case SDL_MOUSEWHEEL:
                // Handle Mouse Wheel Event
                // handleMouseWheelEvent(event.wheel.x, event.wheel.y);
                break;
            }
        }
        SDL_Delay(1);
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
