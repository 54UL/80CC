#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

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

class ClassFinder : public MatchFinder::MatchCallback {
public:
    explicit ClassFinder(std::vector<std::string>& classes) : Classes(classes) {}

    void run(const MatchFinder::MatchResult &Result) override {
        if (const CXXRecordDecl *ClassDecl = Result.Nodes.getNodeAs<CXXRecordDecl>("classDecl")) {
            if (ClassDecl->isThisDeclarationADefinition()) {
                Classes.push_back(ClassDecl->getNameAsString());
            }
        }
    }

private:
    std::vector<std::string>& Classes;
};

void copyFilesWithStructure(const fs::path &src, const fs::path &dest) {
    fs::create_directories(dest);
    for (const auto &entry : fs::recursive_directory_iterator(src)) {
        const auto &path = entry.path();
        auto relativePath = fs::relative(path, src);
        fs::copy(path, dest / relativePath);
    }
}

void generateEngineAddCalls(const std::vector<std::string>& classes, const std::string& mainFilePath) {
    std::ifstream inFile(mainFilePath);
    if (!inFile.is_open()) {
        std::cerr << "Could not open the main file.\n";
        return;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string mainFileContent = buffer.str();
    inFile.close();

    std::string userCodeTag = "// USER_CODE";
    size_t pos = mainFileContent.find(userCodeTag);
    if (pos == std::string::npos) {
        std::cerr << "Tag not found in the main file.\n";
        return;
    }

    std::ostringstream codeStream;
    codeStream << "void addClassesToEngine(Engine* engine) {\n";
    for (const auto& className : classes) {
        codeStream << "    engine->add(" << className << ");\n";
    }
    codeStream << "}\n";

    mainFileContent.replace(pos, userCodeTag.length(), codeStream.str());

    std::ofstream outFile(mainFilePath);
    if (!outFile.is_open()) {
        std::cerr << "Could not write to the main file.\n";
        return;
    }
    outFile << mainFileContent;
    outFile.close();
}

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, llvm::cl::GeneralCategory);
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    std::vector<std::string> classes;
    ClassFinder Finder(classes);

    MatchFinder Matchers;
    Matchers.addMatcher(cxxRecordDecl(isDefinition()).bind("classDecl"), &Finder);

    // Copy files preserving the directory structure
    fs::path src = "/tmp/sources";
    fs::path dest = "/tmp/destination";
    copyFilesWithStructure(src, dest);

    // Run the tool over the copied files
    Tool.run(newFrontendActionFactory(&Matchers).get());

    // Generate an entry point
    std::string mainFilePath = "/tmp/destination/main.cpp"; // Adjust the path to your main file
    generateEngineAddCalls(classes, mainFilePath);

    return 0;
}


