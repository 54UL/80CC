#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

// ECS NOTE:
// NodeComponent is kept as an empty marker base for backward-compatibility
// only (e.g. cereal registrations that have not been migrated yet).
// Components no longer inherit from this class; all runtime behavior has
// moved to ISystem subclasses.  This header will be removed once all
// remaining references are cleaned up.

namespace ettycc
{
    struct EditorPropertyVisitor; // forward — avoids pulling imgui into every TU

    class NodeComponent
    {
    public:
        virtual void InspectProperties(EditorPropertyVisitor& /*v*/) {}
        virtual ~NodeComponent() = default;
    };
}

#endif
