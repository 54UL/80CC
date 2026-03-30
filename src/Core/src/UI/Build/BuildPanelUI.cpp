
#include <UI/Build/BuildPanelUI.hpp>
#include <Build/BuildStrings.hpp>
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <algorithm>
#include <cstring>

#undef max
#undef min

namespace ettycc
{
    BuildPanelUI::BuildPanelUI(build::GlobalBuildConfig& globalCfg)
        : globalCfg_(globalCfg)
    {
        // Source path left empty intentionally — the user must browse to their
        // game source root (the folder containing include/ and src/).
    }

    BuildPanelUI::~BuildPanelUI() = default;

    // -------------------------------------------------------------------------
    // PathField
    // -------------------------------------------------------------------------

    void BuildPanelUI::PathField(const char* label, const char* inputId,
                                  const char* btnId, char* buf, size_t bufSz,
                                  bool isFolder, std::vector<std::string> filter)
    {
        constexpr float kBtnW = 52.0f;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::PushItemWidth(-kBtnW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText(inputId, buf, bufSz);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(btnId, ImVec2(kBtnW, 0)))
        {
            if (isFolder)
            {
                auto dlg = pfd::select_folder(std::string("Select ") + label,
                                              buf[0] ? buf : ".");
                if (!dlg.result().empty())
                    strncpy(buf, dlg.result().c_str(), bufSz - 1);
            }
            else
            {
                auto dlg = pfd::open_file(std::string("Select ") + label,
                                          buf[0] ? buf : ".", filter);
                if (!dlg.result().empty())
                    strncpy(buf, dlg.result()[0].c_str(), bufSz - 1);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void BuildPanelUI::Draw()
    {
        ImGui::Begin(build::str::WIN_BUILD);

        // ── Project ───────────────────────────────────────────────────────────
        ImGui::SeparatorText(build::str::SEC_PROJECT);

        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("##bld_name", projectName_, sizeof(projectName_));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(build::str::FLD_PROJECT_NAME);

        PathField(build::str::FLD_SOURCE, "##bld_src", "...##bld_src_btn",
                  sourcePath_, sizeof(sourcePath_), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_SOURCE);

        PathField(build::str::FLD_OUTPUT, "##bld_out", "...##bld_out_btn",
                  outputPath_, sizeof(outputPath_), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_OUTPUT);

        // ── Configuration ─────────────────────────────────────────────────────
        ImGui::SeparatorText(build::str::SEC_CONFIGURATION);

        static const char* const kConfigs[] = { "Debug", "Release", "RelWithDebInfo" };
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("Config", &configIndex_, kConfigs, IM_ARRAYSIZE(kConfigs));
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::Checkbox("Clean build", &cleanBuild_);
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::Checkbox("Keep generated", &keepGenerated_);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_KEEP_GENERATED);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Build button ──────────────────────────────────────────────────────
        const bool isRunning = controller_.IsRunning();
        const bool canBuild  = !isRunning
                               && sourcePath_[0]               != '\0'
                               && outputPath_[0]               != '\0'
                               && globalCfg_.coreLibPath[0]    != '\0'
                               && globalCfg_.coreIncludePath[0]!= '\0';

        if (!canBuild) ImGui::BeginDisabled();

        if (ImGui::Button(isRunning ? build::str::BTN_BUILDING : build::str::BTN_BUILD,
                          ImVec2(-1, 0)))
        {
            build::BuildRequest req;
            req.sourcePath   = sourcePath_;
            req.outputPath   = outputPath_;
            req.projectName  = projectName_[0] ? projectName_ : "Game";
            req.cfgStr       = kConfigs[configIndex_];
            req.doClean      = cleanBuild_;
            req.keepGenerated= keepGenerated_;

            controller_.StartPipeline(req, globalCfg_);
        }

        if (!canBuild) ImGui::EndDisabled();

        if (!isRunning && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if      (sourcePath_[0]               == '\0') ImGui::SetTooltip("%s", build::str::TIP_BUILD_DISABLED_SRC);
            else if (outputPath_[0]               == '\0') ImGui::SetTooltip("%s", build::str::TIP_BUILD_DISABLED_OUT);
            else if (globalCfg_.coreLibPath[0]    == '\0') ImGui::SetTooltip("%s", build::str::TIP_BUILD_DISABLED_CLIB);
            else if (globalCfg_.coreIncludePath[0]== '\0') ImGui::SetTooltip("%s", build::str::TIP_BUILD_DISABLED_CINC);
        }

        ImGui::Spacing();

        // ── Progress ──────────────────────────────────────────────────────────
        const float prog = controller_.GetProgress();
        if (isRunning || prog > 0.0f)
        {
            ImGui::ProgressBar(prog, ImVec2(-1, 0));
            const std::string status = controller_.GetStatus();
            ImGui::TextDisabled("%s", status.c_str());
        }

        ImGui::Spacing();

        // ── Output log ────────────────────────────────────────────────────────
        const std::vector<std::string> snapshot = controller_.GetLogSnapshot();
        const bool scrollNow = controller_.ConsumeScrollRequest();

        ImGui::SeparatorText(build::str::SEC_OUTPUT);

        if (ImGui::SmallButton(build::str::BTN_COPY_LOG))
        {
            std::string joined;
            joined.reserve(snapshot.size() * 80);
            for (const auto& line : snapshot) { joined += line; joined += '\n'; }
            ImGui::SetClipboardText(joined.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(build::str::BTN_CLEAR_LOG))
            controller_.ClearLog();

        const float logH = std::max(ImGui::GetContentRegionAvail().y - 6.0f, 60.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
        if (ImGui::BeginChild("BuildLog", ImVec2(-1, logH), false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (const auto& line : snapshot)
            {
                ImVec4 col{0.82f, 0.82f, 0.82f, 1.0f};
                if      (line.rfind(build::str::LOG_ERROR,   0) == 0) col = {1.0f,  0.30f, 0.30f, 1.0f};
                else if (line.rfind(build::str::LOG_WARNING, 0) == 0) col = {1.0f,  0.80f, 0.20f, 1.0f};
                else if (line.rfind(build::str::LOG_80CC,    0) == 0) col = {0.40f, 0.85f, 0.45f, 1.0f};
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            if (scrollNow)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
    }

} // namespace ettycc
