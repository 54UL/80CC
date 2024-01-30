#ifndef IRENDERABLE_HPP
#define IRENDERABLE_HPP

#include <glm/glm.h>
#include "RenderingContext.hpp"

namespace ettycc
{
    typedef struct
    {
        uint64_t id;
        Transform underylingTransform;

        virtual void SetId(uint64_t Id)
        {
            id = Id;
        }

        virtual uint64_t GetId()
        {
            return id;
        }

        virtual void SetTransform(const Transform &trans)
        {
            underylingTransform = trans;
        }

        virtual Transform GetTransform()
        {
            return underylingTransform;
        }

        virtual void Pass(const std::shared_ptr<RenderingContext> &ctx) = 0;
    } Renderable;

} // namespace ettycc

#endif