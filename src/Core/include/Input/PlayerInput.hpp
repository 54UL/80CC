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
        WHEEL_Y = 1,
        MOUSE_BUTTONS = 0
    };

    enum class MouseButton : uint64_t
    {
        LEFT = 1,
        RIGHT = 2,
        MIDDLE = 3
    };

    class PlayerInput
    {
    public:
         static constexpr uint8_t DIRECTION_KEY_COUNT = 4;
         static constexpr uint8_t MAX_PRESSED_KEYS    = 8;

    private:
        glm::vec2 leftAxe;  // left hand control
        glm::vec2 rightAxe; // right hand control
        bool invertAxes;
        int xpos,ypos;
        char currentKey; 
        bool pressed;
        int wheelY;
        int wheelX;

        uint32_t pressedKeys[MAX_PRESSED_KEYS]; // this is supposed to hold up to 8 pressed keys or whatever...

    public:
        PlayerInput();
        ~PlayerInput();

        void ProcessInput(PlayerInputType type, uint64_t *data);
        void ResetState();

        // INPUT API...
        [[nodiscard]] glm::vec2 GetLeftAxis() const;
        [[nodiscard]] glm::vec2 GetRightAxis() const;
        [[nodiscard]] glm::ivec2 GetMousePos() const;
        [[nodiscard]] int GetWheelY() const;
        [[nodiscard]] bool GetMouseButton(MouseButton button) const;
    };
} // namespace ettycc


#endif