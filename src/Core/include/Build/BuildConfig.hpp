#pragma once

namespace ettycc::build
{
    // One-time global build configuration, owned by ConfigurationsWindow.
    struct GlobalBuildConfig
    {
        char cmakeGenerator[128] = {};
        char vcvarsallPath[512]  = {};
        char vcpkgToolchain[512] = {};
        char coreLibPath[512]    = {};
        char coreIncludePath[512]= {};
    };

} // namespace ettycc::build
