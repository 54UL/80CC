#include <Input/PlayerInput.hpp>

namespace ettycc
{

    PlayerInput::PlayerInput()
    {
    }

    PlayerInput::~PlayerInput()
    {
    }

    void PlayerInput::ProcessInput(PlayerInputType type, const uint64_t *data)
    {
        InputDirection currentDir;
        const char* WASDKeyCodes = "WASD";

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
            rightAxe = glm::vec2(data[(int)InputDataOffsets::X], data[(int)InputDataOffsets::Y]);
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
}