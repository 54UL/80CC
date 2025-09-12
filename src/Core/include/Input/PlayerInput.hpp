#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP
#include <glm/glm.hpp>
#include <unordered_map>

namespace ettycc
{
    struct InputState {
        glm::ivec2 mousePos{0, 0};
        glm::vec2 mouseDelta{0, 0};
        bool mouseButtons[3]{false, false, false}; // left, right, middle
        int wheelY{0};
        int wheelX{0};
        std::unordered_map<int, bool> keyStates; // keycode -> pressed
    };

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
        MIDDLE = 2,
        RIGHT = 3
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

    public:
        PlayerInput();
        ~PlayerInput();

         void SetMousePos(int x, int y);

         void SetMouseDelta(int xrel, int yrel);

         void SetMouseButton(int button, bool pressed);

         void SetWheel(int x, int y);

         void SetKey(int keycode, bool pressed);

        void ResetState();

        // Getters
        glm::ivec2 GetMousePos() const;
        glm::vec2 GetMouseDelta() const;
        bool GetMouseButton(int button) const;
        int GetWheelY() const;
        int GetWheelX() const;
        bool IsKeyPressed(int keycode) const;

    private:
        InputState state_;
    };
} // namespace ettycc


#endif