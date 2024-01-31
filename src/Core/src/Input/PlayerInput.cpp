#include <Input/PlayerInput.hpp>

namespace ettycc
{
    PlayerInput::PlayerInput(): rightAxe(glm::vec2(0.0f)),leftAxe(glm::vec2(0.0f))
    {
    }

    PlayerInput::~PlayerInput()
    {
    }

    void PlayerInput::ProcessInput(PlayerInputType type, uint64_t *data)
    {
        InputDirection currentDir = InputDirection::NONE;
        const char* WASDKeyCodes = "wasd";

        switch (type)
        {
        case PlayerInputType::KEYBOARD:

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
                leftAxe.y = 1;
                break;
            case InputDirection::DOWN:
                leftAxe.y = -1;
                break;
            case InputDirection::LEFT:
                leftAxe.x = -1;
                break;
            case InputDirection::RIGHT:
                leftAxe.x = 1;
                break;
            }

            break;

        case PlayerInputType::MOUSE:
            // rightAxe = glm::vec2(data[(int)InputDataOffsets::X], data[(int)InputDataOffsets::Y]);
            xpos = (int)data[(int)InputDataOffsets::X] >0?(int)data[(int)InputDataOffsets::X]:0;
            ypos = (int)data[(int)InputDataOffsets::Y] > 0?(int)data[(int)InputDataOffsets::Y]:0;

            break;

        default:
            leftAxe = glm::vec2(0, 0);
            break;
        }
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
}

