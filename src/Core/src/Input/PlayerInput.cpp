#include <Input/PlayerInput.hpp>
#include <spdlog/spdlog.h>

// ALL THIS INPUT SYSTEM IS A TRASH AND NEEDS TO BE REWORKED FROM SCRATCH
// IT IS JUST A PROOF OF CONCEPT TO TEST THE ENGINE SYSTEMS
// create a multi threaded dispatcher alongside with the thread system (handle threads)

namespace ettycc
{
    PlayerInput::PlayerInput() : rightAxe(glm::vec2(0.0f)), leftAxe(glm::vec2(0.0f))
    {
    }

    PlayerInput::~PlayerInput()
    {
    }

    void PlayerInput::ProcessInput(PlayerInputType type, uint64_t *data)
    {
        InputDirection currentDir = InputDirection::NONE;
        const char *WASDKeyCodes = "wsda"; // todo: this gotta be readed from a config...

        switch (type)
        {
        case PlayerInputType::KEYBOARD_DOWN:

            for (uint8_t i = 0; i < DIRECTION_KEY_COUNT; i++)
            {
                if (WASDKeyCodes[i] == data[(uint64_t)InputDataOffsets::KEY])
                {
                    currentDir = static_cast<InputDirection>(i);
                    break;
                }
            }

            switch (currentDir)
            {
            case InputDirection::UP:
                leftAxe.y = 1.0f;
                break;
            case InputDirection::DOWN:
                leftAxe.y = -1.0f;
                break;
            case InputDirection::LEFT:
                leftAxe.x = -1.0f;
                break;
            case InputDirection::RIGHT:
                leftAxe.x = 1.0f;
                break;
            }
            break;
        case PlayerInputType::MOUSE_XY:
            xpos = (int)data[(int)InputDataOffsets::X] > 0 ? (int)data[(int)InputDataOffsets::X] : 0;
            ypos = (int)data[(int)InputDataOffsets::Y] > 0 ? (int)data[(int)InputDataOffsets::Y] : 0;
            break;
        
            case PlayerInputType::MOUSE_BUTTON_DOWN:
            pressedKeys[0] = (int)data[(int)InputDataOffsets::X];
            break;

        case PlayerInputType::MOUSE_BUTTON_UP:
            // if the relased key is in the pressed keys reset the state...
            pressedKeys[0] = (int)data[(int)InputDataOffsets::X] == pressedKeys[0] ? 0 : (int)data[(int)InputDataOffsets::X];
            break;
        case PlayerInputType::MOUSE_WHEEL:
            wheelX = (int)data[(int)InputDataOffsets::WHEEL_X];
            wheelY = (int)data[(int)InputDataOffsets::WHEEL_Y];
            break;

        default:
            break;
        }
    }

    void PlayerInput::ResetState()
    {
        // THIS HAPPENDS EVERY TIME AT THE END OF THE FRAME
        leftAxe = glm::vec2(0.0f, 0.0f);
        rightAxe = glm::vec2(0.0f, 0.0f);
        wheelX = 0.0f;
        wheelY = 0.0f;

        // for (uint8_t i = 0; i < MAX_PRESSED_KEYS; i++)
        // {
        //     pressedKeys[i] = 0;
        // }
    }

    glm::vec2 PlayerInput::GetLeftAxis() const {
        return leftAxe;
    }

    int PlayerInput::GetWheelY() const
    {
        return wheelY;
    }

    glm::vec2 PlayerInput::GetRightAxis() const
    {
        return rightAxe;
    }

    glm::ivec2 PlayerInput::GetMousePos() const
    {
        return glm::ivec2(xpos, ypos);
    }

    bool PlayerInput::GetMouseButton(MouseButton button) const
    {
        return pressedKeys[static_cast<size_t>(InputDataOffsets::MOUSE_BUTTONS)] == static_cast<uint32_t>(button);
    }
}
