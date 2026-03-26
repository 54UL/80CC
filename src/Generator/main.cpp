// 80CC Generator — standalone CLI wrapper around ettycc::build::RunGenerator.
//
// Usage:
//   Generator <source_dir> <output_dir> <project_name> <core_lib> <core_include> [<entry_template_dir>]
//
//   source_dir      — root of the user's game source (contains include/ and src/)
//   output_dir      — parent folder; Generator creates <output_dir>/<project_name>/
//   project_name    — name of the generated project / executable
//   core_lib        — path to the pre-built 80CC_CORE.lib / lib80CC_CORE.a
//   core_include    — path to the 80CC_CORE include/ directory
//   entry_template  — (optional) directory that contains the main.cpp template
//
// The cmake configure + build step is handled by the caller (DevEditor or CI).

#include <Build/ProjectGenerator.hpp>
#include <iostream>

int main(int argc, const char** argv)
{
    if (argc < 6)
    {
        std::cerr << "Usage: Generator <source_dir> <output_dir> <project_name>"
                     " <core_lib> <core_include> [<entry_template_dir>]\n";
        return 1;
    }

    const std::string sourceDir    = argv[1];
    const std::string outputDir    = argv[2];
    const std::string projectName  = argv[3];
    const std::string coreLib      = argv[4];
    const std::string coreInc      = argv[5];
    const std::string templateHint = (argc >= 7) ? argv[6] : "";

    auto logFn = [](const std::string& msg) { std::cout << msg << "\n"; std::cout.flush(); };

    logFn("[80CC] === 80CC Generator ===");

    const bool ok = ettycc::build::RunGenerator(sourceDir, outputDir, projectName,
                                                 coreLib, coreInc, templateHint, logFn);
    if (ok)
        logFn("[80CC] === Generation complete ===");
    else
        std::cerr << "[ERROR] Generation failed.\n";

    return ok ? 0 : 1;
}
