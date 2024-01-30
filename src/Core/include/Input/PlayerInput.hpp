#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP
#include <glm/glm.h>

namespace ettycc
{
    enum class PlayerInputType
    {
        KEYBOARD,
        MOUSE,
        JOYSTICK,
        TOUCH
    };
    enum class InputDirection = {UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3};
    enum class InputDataOffsets
    {
        KEY = 0,
        X = 0,
        Y = 1,
        X_Y = 2
    };

    class PlayerInput
    {
    public:
        constexpr const uint8_t DIRECTION_KEY_COUNT = 4;
        constexpr uint8_t WASDKeyCodes[DIRECTION_KEY_COUNT] = {'W', 'A', 'S', 'D'}; //TODO READ THIS FROM A FILE...

    private:
        glm::vec2 leftAxe;  // left hand control
        glm::vec2 rightAxe; // right hand control
        bool invertAxes;

        char currentKey; 
        bool pressed;
    public:
        PlayerInput();
        ~PlayerInput();

        void ProcessInput(PlayerInputType type, const uint64_t *data);

        // INPUT API...
        glm::vec2 GetLeftAxis(); // WASD OR ARROWS...
        glm::vec2 GetRightAxis(); 

        // glm::vec2 GetMouseAceleration();
        // uint64_t  GetMouseButtonDown();   
        // uint64_t  GetMouseButtonUp();

        // char GetKeyDown();
        // char GetKeyUp();
    };
} // namespace ettycc


#endif