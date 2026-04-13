#ifndef RENDERING_CONTEXT_HPP
#define RENDERING_CONTEXT_HPP
#include <glm/glm.hpp>
#include <Graphics/Rendering/Frustum.hpp>

namespace ettycc
{
    typedef struct
    {
        glm::mat4 Projection;
        glm::mat4 View;
        glm::vec2 ScreenSize;
        Frustum   frustum;
    } RenderingContext;

} // namespace ettycc

#endif
