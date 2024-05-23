// TODO: IMPROVE AND REFACTOR MEANWHILE THIS STAYS AS A CLOWN ASS TOY
// FIRST ITERATION REMOVE WHEN COMPLETED....

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <string_view>

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace fs = std::filesystem;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class ClassFinder : public MatchFinder::MatchCallback
{
public:
    explicit ClassFinder(std::vector<std::string> &classes) : Classes(classes) {}

    void run(const MatchFinder::MatchResult &Result) override
    {
        if (const CXXRecordDecl *ClassDecl = Result.Nodes.getNodeAs<CXXRecordDecl>("classDecl"))
        {
            if (ClassDecl->isThisDeclarationADefinition())
            {
                const auto &className = ClassDecl->getNameAsString();
                std::cout << "Class found: " << className << "\n";

                // TODO: FILTER BY CLASS NAME PREFIX <NAME>GameModule
                Classes.push_back(ClassDecl->getNameAsString());
            }
        }
    }

private:
    std::vector<std::string> &Classes;
};

// Copies the whole folder tree structure
void CopyDirectory(const fs::path &src, const fs::path &dest)
{
    fs::create_directories(dest);
    fs::copy(src, dest, std::filesystem::copy_options::recursive);
}

void GenerateEntryPointSource(const std::vector<std::string> &classes, const std::string &mainFilePath, const std::string &outputPath)
{
    std::ifstream inFile(mainFilePath);

    if (!inFile.is_open())
    {
        std::cerr << "Could not open the main file.\n";
        return;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string mainFileContent = buffer.str();
    inFile.close();

    std::string userCodeTag = "//_80CC_USER_CODE";
    std::string_view buildInfo = "//80CC GAME ENGINE; THIS CODE WAS GENERATED DON'T EDIT THE SOURCE (PREVENT LOSSES!!!)\n";
    mainFileContent.insert(0, buildInfo);
    
    size_t pos = mainFileContent.find(userCodeTag);

    if (pos == std::string::npos)
    {
        std::cerr << "Tag not found in the main file.\n";
        return;
    }

    std::stringstream codeStream;

    // Generate the code to instance the module then add it to engine
    // codeStream << "std::vector<shared_ptr<GameModule>> modules = { \n";

    for (const auto &className : classes)
    {
        // TODO: IMPLEMENT ME!!!!
    }

    // codeStream << "};\n";
    // codeStream << "engine->RegisterModules();\n";

    mainFileContent.replace(pos, userCodeTag.length(), codeStream.str());

    std::ofstream outFile(outputPath);
    if (!outFile.is_open())
    {
        std::cerr << "Could not write to the output path.\n";
        return;
    }

    outFile << mainFileContent;
    outFile.close();
}

// MAIN TODOS::
// Make sure all source is in the compilation unit
// Generated build config (cmake cli)
// Compile using cmake (cmake cli)

int main(int argc, const char **argv)
{
    // TESTING HARDCODING.... (REMOVE =3)
    // TODO: ALSO USE path header to use default paths
    fs::path assetsSrc = "../../../assets/src/";
    fs::path dest = "../../../Executable/";

    std::vector<std::string> sourceFiles =
        {
            "/workspaces/ALPHA_V1/Executable/include/Modules/HelloWorldModule.hpp",
            "/workspaces/ALPHA_V1/Executable/src/Modules/HelloWorldModule.cpp"
        };

    std::vector<std::string> compilationArgs = {"-std=c++17"};

    FixedCompilationDatabase Compilations(".", compilationArgs);

    // Initialize the ClangTool
    ClangTool Tool(Compilations, sourceFiles);

    std::vector<std::string> classes;
    ClassFinder Finder(classes);

    MatchFinder Matchers;
    Matchers.addMatcher(cxxRecordDecl(isDefinition()).bind("classDecl"), &Finder);

    // Copy game modules source into entry/temp
    CopyDirectory(assetsSrc, dest);

    // Run the tool over the copied files
    Tool.run(newFrontendActionFactory(&Matchers).get());

    // Generate the entry point...
    std::string mainFilePath = "../../Entry/src/EntryPoint.cpp";
    std::string outputMainFile = "../../../Executable/src/GeneratedEntryPoint.cpp";

    GenerateEntryPointSource(classes, mainFilePath, outputMainFile);

    return 0;
}
