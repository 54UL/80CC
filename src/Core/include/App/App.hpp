#ifndef ETYCC_APP_H
#define ETYCC_APP_H

#include <glm/glm.hpp>
#include <App/EnginePipeline.hpp>
#include <App/ExecutionPipeline.hpp>

#include <memory>

/*
API SPECS:
WINDOW ABSTRACTIONS
KEYBOARD ABSTRACTION (RETURN KEYCODES)
MOUSE ABSTRACTIONS
*/

namespace ettycc
{
    enum class AppEventType
    {
        INPUT,
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

    class App
    {
    public:
        virtual int Init(int argc, char **argv) = 0;
        virtual int Exec() = 0;
        virtual float GetDeltaTime() = 0;
        virtual float GetCurrentTime() = 0;

        virtual glm::ivec2 GetMainWindowSize() = 0;
        virtual void SetUnderlyingEngine(std::shared_ptr<EnginePipeline> engine) = 0;
        virtual void AddExecutionPipeline(std::shared_ptr<ExecutionPipeline> executionItem) = 0;        
    };
}

#endif