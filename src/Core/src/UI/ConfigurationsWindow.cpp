#undef max
#undef min

#include <UI/ConfigurationsWindow.hpp>
#include <Build/BuildStrings.hpp>
#include <Build/BuildController.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Paths.hpp>
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include <string>

namespace ettycc
{
    ConfigurationsWindow::ConfigurationsWindow()
        : defaultConfigPath_(paths::BUILD_CONFIG_FILE)
    {
        LoadConfig(defaultConfigPath_);
    }

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::SaveConfig(const std::string& path)
    {
        nlohmann::json j;
        j["cmakeGenerator"]  = buildConfig_.cmakeGenerator;
        j["vcvarsallPath"]   = buildConfig_.vcvarsallPath;
        j["vcpkgToolchain"]  = buildConfig_.vcpkgToolchain;
        j["coreLibPath"]     = buildConfig_.coreLibPath;
        j["coreIncludePath"] = buildConfig_.coreIncludePath;

        std::ofstream file(path);
        if (file.is_open())
            file << j.dump(4);
    }

    bool ConfigurationsWindow::LoadConfig(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        try { file >> j; }
        catch (...) { return false; }

        auto copyStr = [](char* dst, size_t sz, const nlohmann::json& obj, const char* key)
        {
            if (obj.contains(key) && obj[key].is_string())
                strncpy(dst, obj[key].get<std::string>().c_str(), sz - 1);
        };

        copyStr(buildConfig_.cmakeGenerator,  sizeof(buildConfig_.cmakeGenerator),  j, "cmakeGenerator");
        copyStr(buildConfig_.vcvarsallPath,   sizeof(buildConfig_.vcvarsallPath),   j, "vcvarsallPath");
        copyStr(buildConfig_.vcpkgToolchain,  sizeof(buildConfig_.vcpkgToolchain),  j, "vcpkgToolchain");
        copyStr(buildConfig_.coreLibPath,     sizeof(buildConfig_.coreLibPath),     j, "coreLibPath");
        copyStr(buildConfig_.coreIncludePath, sizeof(buildConfig_.coreIncludePath), j, "coreIncludePath");

        // Don't mark as auto-detected — DrawBuildSettings will still fill
        // any fields that the saved config left blank.

        return true;
    }

    // -------------------------------------------------------------------------
    // PathField
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::PathField(const char* label, const char* inputId,
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
    // DrawBuildSettings
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::DrawBuildSettings()
    {
        ImGui::SeparatorText(build::str::SEC_BUILD_SETTINGS);

        // Auto-detect empty fields on first draw (loaded config may have left some blank)
        if (!autoDetectDone_)
        {
            build::AutoDetectBuildConfig(buildConfig_);
            autoDetectDone_ = true;
        }

        // Generator — InputText only, no browse button
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(build::str::FLD_GENERATOR);
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##cfg_gen", buildConfig_.cmakeGenerator,
                         sizeof(buildConfig_.cmakeGenerator));
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_GENERATOR);

        // vcvarsall — file browse
        PathField(build::str::FLD_VCVARSALL, "##cfg_vcvars", "...##cfg_vcvars_btn",
                  buildConfig_.vcvarsallPath, sizeof(buildConfig_.vcvarsallPath), false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_VCVARSALL);

        // vcpkg exe warning
        if (buildConfig_.vcpkgToolchain[0] != '\0')
        {
            const std::string tc(buildConfig_.vcpkgToolchain);
            const bool isExe = tc.size() > 4 &&
                               (tc.substr(tc.size() - 4) == ".exe" ||
                                tc.substr(tc.size() - 4) == ".EXE");
            if (isExe)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
                ImGui::TextUnformatted(build::str::WARN_VCPKG_EXE_1);
                ImGui::TextUnformatted(build::str::WARN_VCPKG_EXE_2);
                ImGui::PopStyleColor();
            }
        }

        // vcpkg toolchain — file browse, filter *.cmake
        PathField(build::str::FLD_VCPKG_TOOLCHAIN, "##cfg_vcpkg", "...##cfg_vcpkg_btn",
                  buildConfig_.vcpkgToolchain, sizeof(buildConfig_.vcpkgToolchain), false,
                  { "CMake toolchain", "*.cmake", "All Files", "*" });
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_VCPKG);

        // Core lib — file browse
        PathField(build::str::FLD_CORE_LIB, "##cfg_clib", "...##cfg_clib_btn",
                  buildConfig_.coreLibPath, sizeof(buildConfig_.coreLibPath), false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_CORE_LIB);

        // Core include — folder browse
        PathField(build::str::FLD_CORE_INCLUDE, "##cfg_cinc", "...##cfg_cinc_btn",
                  buildConfig_.coreIncludePath, sizeof(buildConfig_.coreIncludePath), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_CORE_INCLUDE);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Action buttons ────────────────────────────────────────────────────
        if (ImGui::Button(build::str::BTN_AUTO_DETECT))
            build::AutoDetectBuildConfig(buildConfig_);

        ImGui::SameLine();

        if (ImGui::Button(build::str::BTN_SAVE_CFG))
            SaveConfig(defaultConfigPath_);

        ImGui::SameLine();

        if (ImGui::Button(build::str::BTN_IMPORT_CFG))
        {
            auto dlg = pfd::open_file("Import build configuration",
                                      ".",
                                      { "JSON config", "*.json", "All files", "*" });
            if (!dlg.result().empty())
                LoadConfig(dlg.result()[0]);
        }

        ImGui::SameLine();

        if (ImGui::Button(build::str::BTN_EXPORT_CFG))
        {
            auto dlg = pfd::save_file("Export build configuration",
                                      "build_config.json",
                                      { "JSON config", "*.json", "All files", "*" });
            if (!dlg.result().empty())
                SaveConfig(dlg.result());
        }

        // Show the path currently used for auto-save
        ImGui::Spacing();
        ImGui::TextDisabled("Config: %s", defaultConfigPath_.c_str());
    }

    // -------------------------------------------------------------------------
    // DrawGlobals
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::DrawGlobals()
    {
        ImGui::SeparatorText(build::str::SEC_ENGINE_GLOBALS);

        // Search filter
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##filter", globalsFilter_, sizeof(globalsFilter_));
        ImGui::Spacing();

        auto resources = GetDependency(Globals);
        if (!resources)
        {
            ImGui::TextDisabled("Resources not available");
            return;
        }

        const std::string filterStr(globalsFilter_);

        if (ImGui::BeginTable("##globals_table", 3,
                              ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg   |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(-1, -ImGui::GetFrameHeightWithSpacing() - 4.0f)))
        {
            ImGui::TableSetupColumn("Prefix", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Key",    ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableHeadersRow();

            int rowIdx = 0;
            resources->ForEach([&](const std::string& prefix,
                                   const std::string& key,
                                   std::string& value)
            {
                // Apply filter
                if (!filterStr.empty())
                {
                    const bool matchPrefix = prefix.find(filterStr) != std::string::npos;
                    const bool matchKey    = key.find(filterStr)    != std::string::npos;
                    if (!matchPrefix && !matchKey) return;
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(prefix.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(key.c_str());

                ImGui::TableSetColumnIndex(2);

                // Editable value cell
                char valueBuf[512] = {};
                strncpy(valueBuf, value.c_str(), sizeof(valueBuf) - 1);

                ImGui::PushItemWidth(-1);
                const std::string inputId = "##gval_" + std::to_string(rowIdx);
                if (ImGui::InputText(inputId.c_str(), valueBuf, sizeof(valueBuf)))
                {
                    resources->Set(prefix, key, valueBuf);
                    value = valueBuf; // keep the reference in sync
                }
                ImGui::PopItemWidth();

                ++rowIdx;
            });

            ImGui::EndTable();
        }

        if (ImGui::Button(build::str::BTN_SAVE_GLOBALS))
            resources->Store(gk::JSON_FILENAME);
    }

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::Draw()
    {
        if (!open_) return;

        ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(build::str::WIN_CONFIGURATIONS, &open_))
        {
            ImGui::End();
            return;
        }

        // ── Left sidebar ──────────────────────────────────────────────────────
        if (ImGui::BeginChild("##cfg_left", ImVec2(200, -1), true))
        {
            const bool buildSel   = (selectedCategory_ == Category::Build);
            const bool globalsSel = (selectedCategory_ == Category::Globals);

            if (ImGui::Selectable(build::str::CAT_BUILD, buildSel))
                selectedCategory_ = Category::Build;

            if (ImGui::Selectable(build::str::CAT_GLOBALS, globalsSel))
                selectedCategory_ = Category::Globals;
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // ── Right panel ───────────────────────────────────────────────────────
        if (ImGui::BeginChild("##cfg_right", ImVec2(-1, -1), true))
        {
            switch (selectedCategory_)
            {
                case Category::Build:   DrawBuildSettings(); break;
                case Category::Globals: DrawGlobals();       break;
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

} // namespace ettycc
