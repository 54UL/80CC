#include <UI/ConfigurationsWindow.hpp>
#include <UI/Widgets/PathFieldWidget.hpp>
#include <Build/BuildStrings.hpp>
#include <Build/BuildController.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Paths.hpp>
#include <Engine.hpp>
#include <Graphics/Rendering/RenderLayerConfig.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <spdlog/spdlog.h>
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

    ConfigurationsWindow::~ConfigurationsWindow()
    {
        SaveConfig(defaultConfigPath_);
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

        // Don't mark as auto-detected -- DrawBuildSettings will still fill
        // any fields that the saved config left blank.

        return true;
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

        // Generator -- InputText only, no browse button
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(build::str::FLD_GENERATOR);
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##cfg_gen", buildConfig_.cmakeGenerator,
                         sizeof(buildConfig_.cmakeGenerator));
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_GENERATOR);

        // vcvarsall -- file browse
        widgets::PathField(build::str::FLD_VCVARSALL, "##cfg_vcvars", "...##cfg_vcvars_btn",
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

        // vcpkg toolchain -- file browse, filter *.cmake
        widgets::PathField(build::str::FLD_VCPKG_TOOLCHAIN, "##cfg_vcpkg", "...##cfg_vcpkg_btn",
                  buildConfig_.vcpkgToolchain, sizeof(buildConfig_.vcpkgToolchain), false,
                  { "CMake toolchain", "*.cmake", "All Files", "*" });
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_VCPKG);

        // Core lib -- file browse
        widgets::PathField(build::str::FLD_CORE_LIB, "##cfg_clib", "...##cfg_clib_btn",
                  buildConfig_.coreLibPath, sizeof(buildConfig_.coreLibPath), false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_CORE_LIB);

        // Core include -- folder browse
        widgets::PathField(build::str::FLD_CORE_INCLUDE, "##cfg_cinc", "...##cfg_cinc_btn",
                  buildConfig_.coreIncludePath, sizeof(buildConfig_.coreIncludePath), true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", build::str::TIP_CORE_INCLUDE);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -- Action buttons ----------------------------------------------------
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
        auto engine = GetDependency(Engine);

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
            engine->StoreGlobals(gk::JSON_FILENAME);
    }

    // -------------------------------------------------------------------------
    // DrawPhysics
    // -------------------------------------------------------------------------

    void ConfigurationsWindow::DrawPhysics()
    {
        ImGui::SeparatorText("Physics Engine");

        auto engine = GetDependency(Engine);
        if (!engine)
        {
            ImGui::TextDisabled("Engine not available");
            return;
        }

        auto& registry = engine->physicsRegistry_;
        auto available  = registry.GetAvailable();

        if (available.empty())
        {
            ImGui::TextDisabled("No physics implementations registered");
            return;
        }

        // Find the currently active implementation name
        const char* currentName = engine->physicsWorld_
                                  ? engine->physicsWorld_->GetName()
                                  : "None";

        // Sync combo index only on first draw (don't overwrite user's selection)
        if (physicsSelected_ < 0)
        {
            for (int i = 0; i < static_cast<int>(available.size()); ++i)
                if (available[i] == currentName)
                    physicsSelected_ = i;
        }

        // Clamp to valid range (handles -1 initial and registry changes)
        if (physicsSelected_ < 0 || physicsSelected_ >= static_cast<int>(available.size()))
            physicsSelected_ = 0;

        ImGui::Text("Active: %s", currentName);
        if (engine->physicsWorld_ && engine->physicsWorld_->IsMultithreaded())
            ImGui::SameLine(), ImGui::TextDisabled("(multithreaded)");

        ImGui::Spacing();

        // Combo to pick implementation
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Implementation");
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::BeginCombo("##physics_impl", available[physicsSelected_].c_str()))
        {
            for (int i = 0; i < static_cast<int>(available.size()); ++i)
            {
                const bool selected = (i == physicsSelected_);
                if (ImGui::Selectable(available[i].c_str(), selected))
                    physicsSelected_ = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Hot-swap button
        const bool isSame = (available[physicsSelected_] == currentName);
        if (isSame) ImGui::BeginDisabled();

        if (ImGui::Button("Apply & Restart Physics"))
        {
            const std::string& chosen = available[physicsSelected_];
            spdlog::info("[ConfigurationsWindow] switching physics to: {}", chosen);

            // 1. Wait for any in-flight async physics step
            engine->DrainPhysicsFuture();

            // 2. Snapshot body velocities before releasing
            struct BodySnapshot { ecs::Entity entity; glm::vec3 linearVel; };
            std::vector<BodySnapshot> snapshots;

            if (engine->mainScene_)
            {
                // Soft bodies first -- their clusters hold contacts to rigid bodies.
                auto& sbPool = engine->mainScene_->registry_.Pool<SoftBodyComponent>();
                for (size_t i = 0; i < sbPool.Size(); ++i)
                {
                    auto renderable = sbPool.Components()[i].GetRenderable();
                    if (renderable)
                        engine->renderEngine_.RemoveRenderable(renderable);
                    sbPool.Components()[i].ReleaseBody();
                }

                auto& rbPool = engine->mainScene_->registry_.Pool<RigidBodyComponent>();
                for (size_t i = 0; i < rbPool.Size(); ++i)
                {
                    auto& rb = rbPool.Components()[i];
                    if (rb.IsInitialized() && rb.IsDynamic())
                        snapshots.push_back({ rbPool.Entities()[i], rb.GetLinearVelocity() });
                    rb.ReleaseBody();
                }
            }

            // 3. Now safe to destroy the old world and create the new one
            const glm::vec3 prevGravity = engine->physicsWorld_
                ? engine->physicsWorld_->GetGravity() : glm::vec3(0.f);
            engine->physicsWorld_.reset();
            engine->physicsWorld_ = registry.Create(chosen);
            if (engine->physicsWorld_)
            {
                engine->physicsWorld_->Init();
                engine->physicsWorld_->SetGravity(prevGravity);

                // Re-init scene: rebuilds index and re-creates all bodies
                if (engine->mainScene_)
                {
                    engine->mainScene_->Init(*engine);

                    // Restore body velocities so orbits/motion continue
                    auto& rbPool = engine->mainScene_->registry_.Pool<RigidBodyComponent>();
                    for (auto& snap : snapshots)
                    {
                        auto* rb = rbPool.Get(snap.entity);
                        if (rb && rb->IsInitialized())
                            rb->SetLinearVelocity(snap.linearVel);
                    }
                }

                // Persist selection so it survives restart
                auto globals = GetDependency(Globals);
                if (globals)
                    globals->Set(gk::prefix::STATE, gk::key::STATE_PHYSICS_BACKEND, chosen);

                spdlog::info("[ConfigurationsWindow] physics switched to: {}", chosen);
            }
        }

        if (isSame) ImGui::EndDisabled();

        if (isSame)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(already active)");
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Switching physics will re-initialize all bodies in the scene.");
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

        // -- Left sidebar ------------------------------------------------------
        if (ImGui::BeginChild("##cfg_left", ImVec2(200, -1), true))
        {
            const bool buildSel   = (selectedCategory_ == Category::Build);
            const bool globalsSel = (selectedCategory_ == Category::Globals);

            if (ImGui::Selectable(build::str::CAT_BUILD, buildSel))
                selectedCategory_ = Category::Build;

            if (ImGui::Selectable(build::str::CAT_GLOBALS, globalsSel))
                selectedCategory_ = Category::Globals;

            const bool physicsSel = (selectedCategory_ == Category::Physics);
            if (ImGui::Selectable("Physics", physicsSel))
                selectedCategory_ = Category::Physics;

            const bool renderingSel = (selectedCategory_ == Category::Rendering);
            if (ImGui::Selectable("Rendering", renderingSel))
                selectedCategory_ = Category::Rendering;

        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -- Right panel -------------------------------------------------------
        if (ImGui::BeginChild("##cfg_right", ImVec2(-1, -1), true))
        {
            switch (selectedCategory_)
            {
                case Category::Build:     DrawBuildSettings(); break;
                case Category::Globals:   DrawGlobals();       break;
                case Category::Physics:   DrawPhysics();       break;
                case Category::Rendering: DrawRendering();     break;
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // DrawRendering -- Render layer management
    // -------------------------------------------------------------------------
    void ConfigurationsWindow::DrawRendering()
    {
        auto& config = GetDependency(Engine)->renderLayerConfig_;

        ImGui::SeparatorText("Render Layers");
        ImGui::TextDisabled("Layers define rendering groups. Objects on a layer are "
                            "only drawn by cameras whose culling mask includes that layer.");

        ImGui::Spacing();

        // -- Layer list --------------------------------------------------------
        const int count = config.GetActiveCount();
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);

            // Layer index badge
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%2d", i);
            ImGui::SameLine();

            // Editable name (layer 0 "Default" is read-only)
            char nameBuf[64] = {};
            strncpy(nameBuf, config.GetName(i).c_str(), sizeof(nameBuf) - 1);

            if (i == 0)
            {
                ImGui::BeginDisabled();
                ImGui::SetNextItemWidth(200.f);
                ImGui::InputText("##name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_ReadOnly);
                ImGui::EndDisabled();
            }
            else
            {
                ImGui::SetNextItemWidth(200.f);
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                    config.SetName(i, nameBuf);
            }

            // Move up/down buttons
            ImGui::SameLine();
            if (i > 1)
            {
                if (ImGui::SmallButton("Up"))
                    config.SwapLayers(i, i - 1);
            }
            else
                ImGui::Dummy(ImVec2(24, 0));

            ImGui::SameLine();
            if (i > 0 && i < count - 1)
            {
                if (ImGui::SmallButton("Down"))
                    config.SwapLayers(i, i + 1);
            }
            else if (i > 0)
                ImGui::Dummy(ImVec2(36, 0));

            // Remove button (not for layer 0)
            if (i > 0)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
                if (ImGui::SmallButton("X"))
                    config.RemoveLayer(i);
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -- Add new layer -----------------------------------------------------
        ImGui::SetNextItemWidth(200.f);
        bool enterPressed = ImGui::InputText("##new_layer", newLayerName_, sizeof(newLayerName_),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Add Layer") || enterPressed) && newLayerName_[0] != '\0')
        {
            int idx = config.AddLayer(newLayerName_);
            if (idx >= 0)
                spdlog::info("[Rendering] Added layer '{}' at index {}", newLayerName_, idx);
            else
                spdlog::warn("[Rendering] Max layers reached ({})", RenderLayerConfig::kMaxLayers);
            newLayerName_[0] = '\0';
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -- Save / Load buttons -----------------------------------------------
        auto globals = GetDependency(Globals);
        const std::string layerPath = globals->GetWorkingFolder() + "config/render_layers.json";

        if (ImGui::Button("Save Layers"))
        {
            config.Save(layerPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Layers"))
        {
            config.Load(layerPath);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Layers are saved to: %s", layerPath.c_str());
    }


} // namespace ettycc
