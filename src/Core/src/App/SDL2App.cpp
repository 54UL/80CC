#include <App/SDL2App.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <functional>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

namespace ettycc
{
    SDL2App::SDL2App(const char * windowTitle) : runningStatus_(true), windowTitle_(windowTitle)
    {
        currentAppTime_ =0;
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
        window_ = SDL_CreateWindow(windowTitle_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_RENDERER_PRESENTVSYNC);
        if (!window_)
        {
            // Handle window creation error
            SDL_Quit();
            return 1;
        }

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

  

        currentEngine_->Init();
          // From here you can start using IMGUI + GL
        for(auto execution : executionPipelines_)
        {
            execution->Init();
        }
        return 0;
    }

    void SDL2App::RenderingInit() {
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
		glEnable(GL_DEPTH_TEST);
        
        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
     
        ImGui_ImplSDL2_InitForOpenGL(window_, glContext_);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        SDL_GetWindowSize(window_,&mainWindowSize_.x,&mainWindowSize_.y);

        // ImGuiIO& io = ImGui::GetIO();
        // (void)io;
    }
    
    void SDL2App::PrepareFrame()
    {
        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window_);
        ImGui::NewFrame();
        // This is most for imgui so....
        currentEngine_->PrepareFrame();
        
        // ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing);
        // // ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
        // ImGui::Begin("80CC [DEV EDITOR]");//ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize

        for(auto execution : executionPipelines_)
        {
            execution->UpdateUI();
        }
        // ImGui::End();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void SDL2App::PresentFrame()
    {
        // User code render calls
        currentEngine_->PresentFrame();

        // ImGui render call
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        SDL_GL_SwapWindow(window_);
    }

    // WARNING: MAIN ENTRY POINT/THREAD (TECHNICALLY)
    int SDL2App::Exec()
    {
        // Calculate delta time
        uint32_t prevTicks =  SDL_GetTicks();
        uint32_t currentTicks = prevTicks;
        while (this->IsRunning())
        {
            currentTicks = SDL_GetTicks();
            currentDeltaTime_= (currentTicks - prevTicks) ;
            currentAppTime_ += currentDeltaTime_;
            
            AppInput();
            currentEngine_->Update();
            PrepareFrame();
            PresentFrame();
            
            prevTicks = currentTicks;
        }
        return 0;
    }

    void SDL2App::Dispose()
    {
        // Cleanup
        // int eventThreadReturnValue;
        // SDL_WaitThread(eventThread_, &eventThreadReturnValue);

        // Destroy the mutex
        // SDL_DestroyMutex(eventMutex_);

        // Cleanup ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // Destroy gl and window...
        SDL_GL_DeleteContext(glContext_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    float SDL2App::GetDeltaTime()
    {
        return currentDeltaTime_;
    }
    float  SDL2App::GetCurrentTime()
    {
        return currentAppTime_;
    }

    glm::ivec2 SDL2App::GetMainWindowSize()
    {
        return mainWindowSize_;
    }

    void SDL2App::SetUnderlyingEngine(std::shared_ptr<EnginePipeline> engine)
    {
        currentEngine_ = engine;
    }

    void SDL2App::AddExecutionPipeline(std::shared_ptr<ExecutionPipeline> editor)
    {
        executionPipelines_.emplace_back(editor);
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
            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (event.type)
            {
            case SDL_QUIT:
                SetRunningStatus(0);
                return;

            case SDL_KEYDOWN:
                data[0] = event.key.keysym.sym;
                currentEngine_->ProcessInput(PlayerInputType::KEYBOARD, data);
                break;

            case SDL_MOUSEMOTION:
                data[0] = event.motion.x;
                data[1] = event.motion.y;
                currentEngine_->ProcessInput(PlayerInputType::MOUSE, data);
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
