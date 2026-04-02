#pragma once
#include <cstdint>

namespace ettycc::ecs {
    using Entity = uint64_t;
    inline constexpr Entity NullEntity = 0;
}
