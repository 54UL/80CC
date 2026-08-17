#ifndef CAMERA_CONTROLLER_COMPONENT_HPP
#define CAMERA_CONTROLLER_COMPONENT_HPP

#include <Scene/PropertySystem.hpp>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    struct EditorPropertyVisitor;

    // -- CameraControllerComponent --------------------------------------------
    // Pure data component for camera pan/zoom control.  When attached to a node
    // that also has a Camera (RenderableNode), the engine's EditorCamera reads
    // and writes these fields, making them inspectable and serializable.
    //
    // This replaces the old pattern of EditorCamera owning position/zoom
    // internally -- now they live in the ECS and survive scene reloads.
    // -- CameraControllerComponent --------------------------------------------
    // Pure data: zoom configuration for a camera.  Position is NOT stored
    // here -- it lives in the SceneNode's transform_ (single source of truth).
    struct CameraControllerComponent
    {
        static constexpr const char* componentType = "CameraController";

        float     zoom       = 1.f;         // multiplicative zoom level
        float     zoomSpeed  = 1.12f;       // multiplier per scroll tick
        float     zoomMin    = 0.05f;
        float     zoomMax    = 500.f;

        void InspectProperties(EditorPropertyVisitor& v);

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp("zoom",      zoom),
               cereal::make_nvp("zoomSpeed", zoomSpeed),
               cereal::make_nvp("zoomMin",   zoomMin),
               cereal::make_nvp("zoomMax",   zoomMax));
        }
    };
}

#endif
