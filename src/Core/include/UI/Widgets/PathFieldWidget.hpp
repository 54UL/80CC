#ifndef PATH_FIELD_WIDGET_HPP
#define PATH_FIELD_WIDGET_HPP

#include <imgui.h>
#include <string>
#include <vector>

namespace ettycc {
namespace widgets {

    // Renders a label + InputText + "..." browse button.
    // Returns true if the path was changed (typed or browsed).
    // `filter` follows portable-file-dialogs format: {"Description", "*.ext1 *.ext2", ...}
    bool PathField(const char* label, const char* inputId, const char* btnId,
                   char* buf, size_t bufSz, bool isFolder = false,
                   std::vector<std::string> filter = {});

    // Convenience overload for std::string.
    bool PathField(const char* label, std::string& path, bool isFolder = false,
                   std::vector<std::string> filter = {});

} // namespace widgets
} // namespace ettycc

#endif // PATH_FIELD_WIDGET_HPP
