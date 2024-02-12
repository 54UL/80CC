#ifndef DEPENDECY_HPP
#define DEPENDECY_HPP

#include <Engine.hpp>
#include <memory>

// BRIEF:
// SINGLETONS AND ALL SHARED STATIC DEPENDENCIES ACROSS ALL THE WHOLE CORE(I SAID)
#define RegisterDependency(type, instance) Dependency::getInstance().registerInstance(""#type"", instance)
#define GetDependency(type)  Dependency::getInstance().resolve<type>(""#type"")

namespace ettycc
{  
    // SIMPLE DEPENDENCY SYSTEM...

    class Dependency
    {
    public:
        static Dependency &getInstance()
        {
            static Dependency instance;
            return instance;
        }

        template <typename T>
        void registerInstance(const std::string &key, std::shared_ptr<T> instance)
        {
            // key.replace("\"","");

            instances[key] = std::static_pointer_cast<void>(instance);
        }

        template <typename T>
        std::shared_ptr<T> resolve(const std::string &key)
        {
            auto it = instances.find(key);
            if (it != instances.end())
            {
                return std::static_pointer_cast<T>(it->second);
            }
            return nullptr;
        }

    private:
        Dependency() = default;
        Dependency(const Dependency &) = delete;
        Dependency &operator=(const Dependency &) = delete;

        std::unordered_map<std::string, std::shared_ptr<void>> instances;
    };
} // namespace ettycc


#endif