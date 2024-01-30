#ifndef ETYCC_APP_H
#define ETYCC_APP_H

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
        // virtual int AddEventListener();// TODO MAKE IT PURE
    };
}

#endif