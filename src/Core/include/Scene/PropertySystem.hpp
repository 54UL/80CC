#ifndef PROPERTY_SYSTEM_HPP
#define PROPERTY_SYSTEM_HPP

// ── Property system ───────────────────────────────────────────────────────────
//
// Components expose their serializable / inspectable fields with one-liner macros
// inside a templated Inspect(Visitor& v) method.  The same function drives:
//
//   • Editor inspector  — instantiated with EditorPropertyVisitor  (imgui widgets)
//   • Scene serializer  — instantiated with CerealVisitor<Archive> (cereal read/write)
//
// Any Visitor must implement:
//   template<typename T>
//   void Property(const char* label, T& value, uint32_t flags = PROP_NONE);
//
// ── Macro usage ───────────────────────────────────────────────────────────────
//
//   template<typename Visitor>
//   void Inspect(Visitor& v)
//   {
//       PROP   (mass_,          "Mass");
//       PROP_RO(rigidBodyId_,   "ID");            // read-only in editor
//       PROP_NS(runtimePtr_,    "Runtime Ptr");   // skip serialization
//       PROP_F (scale_,         "Scale", PROP_READ_ONLY | PROP_NO_SERIAL);
//   }
//
// ── Game-build stripping ──────────────────────────────────────────────────────
// When EDITOR_BUILD is not defined the PROP macros expand to nothing, and
// InspectProperties() on NodeComponent is compiled out entirely.  Components
// therefore have zero editor overhead in shipping builds.

#include <cstdint>
#include <cereal/archives/json.hpp>
#include <glm/glm.hpp>

// ── GLM cereal support ────────────────────────────────────────────────────────
// Teach cereal how to serialize glm vector/quaternion types so that
// CerealVisitor can handle PROP(someVec3_, ...) without extra boilerplate.
namespace glm
{
    template<class Archive> void serialize(Archive& ar, vec2& v)
    { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y)); }

    template<class Archive> void serialize(Archive& ar, vec3& v)
    { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z)); }

    template<class Archive> void serialize(Archive& ar, vec4& v)
    { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w)); }

    template<class Archive> void serialize(Archive& ar, quat& q)
    { ar(cereal::make_nvp("x", q.x), cereal::make_nvp("y", q.y), cereal::make_nvp("z", q.z), cereal::make_nvp("w", q.w)); }
} // namespace glm

namespace ettycc
{
    // ── Property flags ────────────────────────────────────────────────────────
    enum PropFlag : uint32_t
    {
        PROP_NONE      = 0,
        PROP_READ_ONLY = 1 << 0,  // visible in editor but widget is disabled
        PROP_HIDDEN    = 1 << 1,  // kept in serialization, hidden in editor
        PROP_NO_SERIAL = 1 << 2,  // shown in editor but excluded from save/load
    };

    // ── CerealVisitor ─────────────────────────────────────────────────────────
    // Drop-in replacement for hand-written cereal save/load bodies.
    // Use inside a component's save()/load() or serialize():
    //
    //   template<class Archive>
    //   void serialize(Archive& ar) {
    //       CerealVisitor<Archive> cv{ar};
    //       Inspect(cv);           // single source of truth
    //   }
    template<typename Archive>
    struct CerealVisitor
    {
        Archive& ar;

        template<typename T>
        void Property(const char* name, T& value, uint32_t flags = PROP_NONE)
        {
            if (flags & PROP_NO_SERIAL) return;
            ar(cereal::make_nvp(name, value));
        }

        // No-op: section labels are editor-only, cereal ignores them.
        void Section(const char* /*label*/) {}
    };

} // namespace ettycc

// Editor is always enabled for now (no separate game target yet).
// Remove the override below once the build system defines EDITOR_BUILD properly.
#ifndef EDITOR_BUILD
#  define EDITOR_BUILD
#endif

// ── Macros ────────────────────────────────────────────────────────────────────
#ifdef EDITOR_BUILD
#  define PROP(field, label)           v.Property(label, field)
#  define PROP_RO(field, label)        v.Property(label, field, ettycc::PROP_READ_ONLY)
#  define PROP_NS(field, label)        v.Property(label, field, ettycc::PROP_NO_SERIAL)
#  define PROP_F(field, label, flags)  v.Property(label, field, flags)
// Visual separator inside an Inspect() body — shows a labelled divider in the editor.
// Expands to nothing in game builds and in cereal visitors.
#  define PROP_SECTION(label)          v.Section(label)
#else
#  define PROP(field, label)
#  define PROP_RO(field, label)
#  define PROP_NS(field, label)
#  define PROP_F(field, label, flags)
#  define PROP_SECTION(label)
#endif

#endif
