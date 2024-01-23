#ifndef ETYCC_APP_H
#define ETYCC_APP_H

namespace etycc
{
    class App
    {
    public:
        virtual int Init(int argc, char **argv) = 0;
        virtual int Exec() = 0;
    };
}

#endif