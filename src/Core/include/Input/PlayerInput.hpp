#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP
#include <glm/glm.hpp>

namespace ettycc
{
    enum class PlayerInputType
    {
        KEYBOARD,
        MOUSE,
        JOYSTICK,
        TOUCH
    };
    enum class InputDirection  {UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, NONE = 4};
    enum class InputDataOffsets : uint64_t
    {
        KEY = 0,
        X = 0,
        Y = 1,
        X_Y = 2
    };

    class PlayerInput
    {
    public:
         const uint8_t DIRECTION_KEY_COUNT = 4;

    private:
        glm::vec2 leftAxe;  // left hand control
        glm::vec2 rightAxe; // right hand control
        bool invertAxes;
        int xpos,ypos;
        char currentKey; 
        bool pressed;
    public:
        PlayerInput();
        ~PlayerInput();

        void ProcessInput(PlayerInputType type, uint64_t *data);

        // INPUT API...
        glm::vec2 GetLeftAxis(); // WASD OR ARROWS...
        glm::vec2 GetRightAxis(); 
        glm::ivec2 GetMousePos(); 

        // glm::vec2 GetMouseAceleration();
        // uint64_t  GetMouseButtonDown();   
        // uint64_t  GetMouseButtonUp();

        // char GetKeyDown();
        // char GetKeyUp();
    };
} // namespace ettycc


#endif