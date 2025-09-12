#include <Input/PlayerInput.hpp>
#include <spdlog/spdlog.h>

// ALL THIS INPUT SYSTEM IS A TRASH AND NEEDS TO BE REWORKED FROM SCRATCH
// IT IS JUST A PROOF OF CONCEPT TO TEST THE ENGINE SYSTEMS
// create a multi threaded dispatcher alongside with the thread system (handle threads)

namespace ettycc {
    PlayerInput::PlayerInput() {
    }

    PlayerInput::~PlayerInput() {
    }

    void PlayerInput::SetMousePos(int x, int y) {
        state_.mousePos = glm::ivec2(x, y);
    }

    void PlayerInput::SetMouseDelta(int xrel, int yrel) {
        state_.mouseDelta = glm::vec2(xrel, yrel);
    }

    void PlayerInput::SetMouseButton(int button, bool pressed) {
        if (button >= 1 && button <= 3) state_.mouseButtons[button - 1] = pressed;
    }

    void PlayerInput::SetWheel(int x, int y) {
        state_.wheelX = x;
        state_.wheelY = y;
    }

    void PlayerInput::SetKey(int keycode, bool pressed) {
        state_.keyStates[keycode] = pressed;
    }

    void PlayerInput::ResetState() {
        state_.mouseDelta = glm::vec2(0, 0);
        state_.wheelX = 0;
        state_.wheelY = 0;
    }

    glm::ivec2 PlayerInput::GetMousePos() const {
        return state_.mousePos;
    }
    glm::vec2 PlayerInput::GetMouseDelta() const {
        return state_.mouseDelta;
    }

    bool PlayerInput::GetMouseButton(int button) const {
        if (button >= 1 && button <= 3) return state_.mouseButtons[button - 1];
        return false;
    }

    int PlayerInput::GetWheelY() const {
        return state_.wheelY;
    }

    int PlayerInput::GetWheelX() const {
        return state_.wheelX;
    }

    bool PlayerInput::IsKeyPressed(int keycode) const {
        auto it = state_.keyStates.find(keycode);
        return it != state_.keyStates.end() && it->second;
    }
}
