#ifndef IRENDERABLE_HPP
#define IRENDERABLE_HPP
#include "RenderingContext.hpp"

#include <memory>
#include <glm/glm.hpp>
#include <Scene/Transform.hpp>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Renderable
    {
    public:
        Transform underylingTransform;
        uint64_t id;
        bool enabled;
        

    public:
        Renderable() {
           
        }

        ~Renderable(){

        }

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

        virtual void Pass(const std::shared_ptr<RenderingContext> &ctx,float time) = 0;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(underylingTransform), CEREAL_NVP(id), CEREAL_NVP(enabled));
        }
    };

} // namespace ettycc

#endif