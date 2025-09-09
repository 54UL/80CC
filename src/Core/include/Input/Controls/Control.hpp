#ifndef INPUT_CONTROLS_CONTRO_HPP
#define INPUT_CONTROLS_CONTRO_HPP

namespace ettycc
{
    class Control
    {
    public:
        virtual void Update(float deltaTime) = 0;
        virtual void LateUpdate(float deltaTime) = 0; // no real usage yet...
    };
}


#endif
