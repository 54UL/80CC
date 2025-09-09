#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP
#include <glm/glm.hpp>

namespace ettycc
{
    enum class PlayerInputType
    {
        KEYBOARD_DOWN,
        KEYBOARD_UP,
        MOUSE_XY,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_WHEEL,
        JOYSTICK,
        TOUCH
    };
    
    enum class InputDirection
    {
        UP = 0,
        DOWN = 1,
        LEFT = 2,
        RIGHT = 3,
        NONE = 4
    };
    enum class InputDataOffsets : uint64_t
    {
        KEY = 0,
        X = 0,
        Y = 1,
        X_Y = 2,
        WHEEL_X = 0,
        WHEEL_Y = 1
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
        int wheelY;

        uint32_t pressedKeys[8];

    public:
        PlayerInput();
        ~PlayerInput();

        void ProcessInput(PlayerInputType type, uint64_t *data);
        void ResetState();

        // INPUT API...
        glm::vec2 GetLeftAxis(); // WASD OR ARROWS...
        glm::vec2 GetRightAxis(); 
        glm::ivec2 GetMousePos(); 
        bool GetMouseButton(int buttonIndex);

        // glm::vec2 GetMouseAceleration();
        // uint64_t  GetMouseButtonDown();   
        // uint64_t  GetMouseButtonUp();

        // char GetKeyDown();
        // char GetKeyUp();
    };
} // namespace ettycc


#endif