#include <Input/PlayerInput.hpp>

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
            // TODO: Actually insert normal key pressing(might be implemented with an pressedKeys array to test against multiple pressed keys...), the code above is only to convert a set of keys to axis lol

            break;

        case PlayerInputType::MOUSE_XY:
            // rightAxe = glm::vec2(data[(int)InputDataOffsets::X], data[(int)InputDataOffsets::Y]);
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

        default:
            break;
        }
    }

    void PlayerInput::ResetState()
    {
        // NOT THE BEST SOLUTION BUT IT WORKS....
        leftAxe = glm::vec2(0.0f, 0.0f);
        // pressedKeys[0]  = 0;
    }

    glm::vec2 PlayerInput::GetLeftAxis()
    {
        return leftAxe;
    }

    glm::vec2 PlayerInput::GetRightAxis()
    {
        return rightAxe;
    }

    glm::ivec2 PlayerInput::GetMousePos()
    {
        return glm::ivec2(xpos, ypos);
    }

    bool PlayerInput::GetMouseButton(int buttonIndex)
    {
        return pressedKeys[0] == buttonIndex;
    }
}
