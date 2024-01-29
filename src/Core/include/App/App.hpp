#ifndef ETYCC_APP_H
#define ETYCC_APP_H

/*
API SPECS:
WINDOW ABSTRACTIONS
KEYBOARD ABSTRACTION (RETURN KEYCODES)
MOUSE ABSTRACTIONS
*/

namespace etycc
{
    class App
    {
    public:
        virtual int Init(int argc, char **argv) = 0;
        virtual int Exec() = 0;
        // virtual int AddEventListener();// TODO MAKE IT PURE
    };
}

#endif