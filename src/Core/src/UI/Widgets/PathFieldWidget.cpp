#include <UI/Widgets/PathFieldWidget.hpp>
#include <portable-file-dialogs.h>


namespace ettycc {
namespace widgets {

    bool PathField(const char* label, const char* inputId, const char* btnId,
                   char* buf, size_t bufSz, bool isFolder,
                   std::vector<std::string> filter)
    {
        bool changed = false;
        constexpr float kBtnW = 52.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::PushItemWidth(-kBtnW - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputText(inputId, buf, bufSz))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(btnId, ImVec2(kBtnW, 0)))
        {
            if (isFolder)
            {
                auto dlg = pfd::select_folder(std::string("Select ") + label,
                                              buf[0] ? buf : ".");
                if (!dlg.result().empty())
                {
                    strncpy(buf, dlg.result().c_str(), bufSz - 1);
                    buf[bufSz - 1] = '\0';
                    changed = true;
                }
            }
            else
            {
                auto dlg = pfd::open_file(std::string("Select ") + label,
                                          buf[0] ? buf : ".", filter);
                if (!dlg.result().empty())
                {
                    strncpy(buf, dlg.result()[0].c_str(), bufSz - 1);
                    buf[bufSz - 1] = '\0';
                    changed = true;
                }
            }
        }
        return changed;
    }

    bool PathField(const char* label, std::string& path, bool isFolder,
                   std::vector<std::string> filter)
    {
        char buf[512] = {};
        strncpy(buf, path.c_str(), sizeof(buf) - 1);

        // Generate unique ImGui IDs from the label
        char inputId[128], btnId[128];
        snprintf(inputId, sizeof(inputId), "##%s_input", label);
        snprintf(btnId,   sizeof(btnId),   "...##%s_btn", label);

        bool changed = PathField(label, inputId, btnId, buf, sizeof(buf), isFolder, filter);
        if (changed)
            path = buf;
        return changed;
    }

} // namespace widgets
} // namespace ettycc
