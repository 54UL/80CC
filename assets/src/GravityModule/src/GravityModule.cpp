#include "GravityModule.hpp"
#include "GravityDynamicsSystem.hpp"
#include "FusionDataComponent.hpp"

#include <Engine.hpp>
#include <Scene/Scene.hpp>
#include <Scene/SceneNode.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <UI/ComponentRegistry.hpp>
#include <UI/EditorExtensionRegistry.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Random.hpp>
#include <Math/Constants.hpp>
#include <spdlog/spdlog.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>

// DLL exports
ETTYCC_MODULE(gravity::GravityGameModule)
ETTYCC_MODULE_IMGUI()

namespace gravity
{
    using namespace ettycc;

    GravityGameModule::GravityGameModule()
    {
        name_ = "GravityModule";
    }

    bool GravityGameModule::OnStart(const Engine* engine)
    {
        engine_ = engine;
        spdlog::info("[GravityModule] OnStart -- registering systems, components, and editor extensions");

        auto scene = engine->mainScene_;
        if (!scene)
        {
            spdlog::warn("[GravityModule] No active scene -- nothing to do");
            return true;
        }

        scene->RegisterSystem(std::make_unique<GravityDynamicsSystem>());
        RegisterComponents();
        RegisterEditorExtensions();

        spdlog::info("[GravityModule] Ready -- gravity dynamics active");
        return true;
    }

    void GravityGameModule::OnUpdate(const float /*deltaTime*/)
    {
        // The heavy lifting is done by GravityDynamicsSystem::OnUpdate via the ECS.
    }

    void GravityGameModule::OnDestroy()
    {
        spdlog::info("[GravityModule] OnDestroy -- cleaning up");

        UnregisterEditorExtensions();
        UnregisterComponents();

        if (!engine_ || !engine_->mainScene_) return;

        auto& registry = engine_->mainScene_->registry_;

        if (auto* pool = registry.TryGetPool<FusionDataComponent>())
        {
            auto entities = pool->Entities();
            for (auto e : entities)
                registry.Remove<FusionDataComponent>(e);
        }

        auto& systems = engine_->mainScene_->systems_;
        systems.erase(
            std::remove_if(systems.begin(), systems.end(),
                [](const std::unique_ptr<ISystem>& sys) {
                    return dynamic_cast<GravityDynamicsSystem*>(sys.get()) != nullptr;
                }),
            systems.end());

        spdlog::info("[GravityModule] Cleanup complete");
    }

    // -- Editor extensions -----------------------------------------------------
    void GravityGameModule::RegisterEditorExtensions()
    {
        auto* eng = const_cast<Engine*>(engine_);

        // Menu item: "Gravity Attractor..." in the built-in scenes menu
        eng->editorExtensions_.RegisterMenuItem(name_, "Gravity Attractor...",
            [this](Engine& /*engine*/)
            {
                if (ImGui::MenuItem("Gravity Attractor..."))
                {
                    pendingConfig_ = {};
                    popupOpen_ = true;
                }
            });

        // Config popup: full gravity scene configuration
        eng->editorExtensions_.RegisterPopup(name_, "Gravity Config Popup",
            [this](Engine& engine)
            {
                if (popupOpen_)
                {
                    ImGui::OpenPopup("##GravitySceneCfg");
                    popupOpen_ = false;
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);

                if (!ImGui::BeginPopupModal("##GravitySceneCfg", nullptr,
                                             ImGuiWindowFlags_AlwaysAutoResize))
                    return;

                ImGui::TextUnformatted("Gravity Attractor");
                ImGui::Separator();
                ImGui::Spacing();

                auto& c = pendingConfig_;
                ImGui::DragInt("Box Count", &c.boxCount, 10, 1, 50000);
                float prevOrbit = c.orbitRadius;
                ImGui::DragFloat("Orbit Radius", &c.orbitRadius, 0.1f, 0.5f, 50.0f);
                if (c.orbitRadius != prevOrbit)
                {
                    c.innerRadius = c.orbitRadius * 0.16f;
                    c.outerRadius = c.orbitRadius * 6.0f;
                }
                ImGui::DragFloat("Attractor Strength", &c.attractorStrength, 0.5f, 1.0f, 500.0f);
                ImGui::DragFloat("Box Size", &c.boxSize, 0.005f, 0.02f, 2.0f, "%.3f");
                ImGui::Spacing();
                ImGui::TextUnformatted("Attractor Radii");
                ImGui::DragFloat("Inner Radius", &c.innerRadius, 0.05f, 0.01f, 100.0f, "%.2f");
                ImGui::SetItemTooltip("Singularity clamp -- prevents infinite force near center.\nKeep small relative to orbit.");
                ImGui::DragFloat("Outer Radius", &c.outerRadius, 0.5f, 0.5f, 500.0f, "%.1f");
                ImGui::SetItemTooltip("Max gravitational reach. Bodies outside this are ignored.");
                ImGui::Spacing();
                ImGui::TextUnformatted("Fusion");
                ImGui::DragFloat("Overlap Factor", &c.fusionOverlap, 0.05f, 1.0f, 5.0f, "%.2f");
                ImGui::SetItemTooltip("Must be > 1.0 (Box2D prevents penetration).\nHigher = easier fusion.");
                ImGui::DragFloat("Cooldown (s)", &c.fusionCooldown, 0.01f, 0.0f, 5.0f, "%.2f");
                ImGui::Spacing();
                ImGui::TextUnformatted("N-Body Gravity");
                ImGui::Checkbox("Enable N-Body", &c.nBodyEnabled);
                ImGui::SetItemTooltip("Every body attracts every other body.\nO(n^2) -- keep box count reasonable.");
                if (c.nBodyEnabled)
                {
                    ImGui::DragFloat("G (constant)", &c.nBodyG, 0.01f, 0.001f, 100.0f, "%.3f");
                    ImGui::SetItemTooltip("Gravitational constant -- scales all body-body forces.");
                    ImGui::DragFloat("Softening", &c.nBodySoftening, 0.01f, 0.01f, 10.0f, "%.3f");
                    ImGui::SetItemTooltip("Prevents singularity when bodies are very close.\nSmaller = stronger close-range pull.");
                }
                ImGui::Spacing();
                ImGui::TextUnformatted("Collision Energy (Q*)");
                ImGui::DragFloat("Q Fusion", &c.qFusion, 0.5f, 0.1f, 100.0f, "%.1f");
                ImGui::SetItemTooltip("Below this specific energy: bodies fuse (accretion).\nLow = easy fusion even at moderate speeds.");
                ImGui::DragFloat("Q Shatter", &c.qShatter, 1.0f, 1.0f, 500.0f, "%.1f");
                ImGui::SetItemTooltip("Above this: catastrophic disruption (explosion).\nBetween fusion & shatter: fracture/chipping.");
                ImGui::DragInt("Shatter Fragments", &c.shatterFragments, 1, 2, 20);
                ImGui::DragFloat("Rock Roughness", &c.rockRoughness, 0.01f, 0.0f, 1.0f, "%.2f");
                ImGui::SetItemTooltip("Procedural rock jitter. 0 = perfect circle, 1 = very jagged.");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

                if (ImGui::Button("Create", ImVec2(buttonWidth, 0)))
                {
                    // Load a fresh scene, then set up gravity
                    engine.LoadBuiltInScene(SampleScene::Default);
                    SetupGravityScene(c);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            });
    }

    void GravityGameModule::UnregisterEditorExtensions()
    {
        const_cast<Engine*>(engine_)->editorExtensions_.UnregisterAll(name_);
    }

    // -- Scene setup -----------------------------------------------------------
    void GravityGameModule::SetupGravityScene(const GravitySceneConfig& cfg)
    {
        if (!engine_ || !engine_->mainScene_) return;

        auto* eng = const_cast<Engine*>(engine_);
        auto scene = eng->mainScene_;

        scene->sceneName_ = "gravity-scene";
        eng->physicsWorld_->SetGravity({0.f, 0.0f, 0.f});

        // Ensure GravityDynamicsSystem exists and configure it
        bool hasSystem = false;
        for (auto& sys : scene->systems_)
        {
            if (auto* gds = dynamic_cast<GravityDynamicsSystem*>(sys.get()))
            {
                gds->ApplyConfig(cfg);
                hasSystem = true;
                break;
            }
        }
        if (!hasSystem)
        {
            auto gds = std::make_unique<GravityDynamicsSystem>();
            gds->ApplyConfig(cfg);
            scene->RegisterSystem(std::move(gds));
        }

        auto root = scene->root_node_;

        // Gravity attractor at the origin
        {
            auto attractorNode = std::make_shared<SceneNode>("gravity-attractor");
            scene->registry_.Add<GravityAttractorComponent>(
                attractorNode->GetId(),
                GravityAttractorComponent{glm::vec3(0.f), cfg.attractorStrength,
                                          cfg.innerRadius, cfg.outerRadius});
            root->AddChild(attractorNode);
        }

        // Orbiting rocks
        auto rng = GetDependency(RNG);
        const float halfBox = cfg.boxSize;

        for (int i = 0; i < cfg.boxCount; ++i)
        {
            const float angle = (math::kTwoPi * i) / static_cast<float>(cfg.boxCount);

            const glm::vec3 pos(rng->Float(0.0f, glm::cos(angle) * cfg.orbitRadius),
                                rng->Float(0.0f, glm::sin(angle) * cfg.orbitRadius),
                                0.0f);

            static int rockIdx = 0;
            static uint32_t seedCounter = 1;
            int segs = glm::clamp(static_cast<int>(glm::sqrt(1.0f) * 4.f), 8, 24);
            auto rockShape = SpriteShape::MakeRock(segs, cfg.rockRoughness, seedCounter++);
            auto sprite = Sprite::MakeWithShader(rockShape, "rock_procedural");
            sprite->underylingTransform.setGlobalPosition(pos);
            sprite->underylingTransform.setGlobalScale(glm::vec3(halfBox));

            auto node = std::make_shared<SceneNode>("rock-" + std::to_string(rockIdx++));
            scene->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});
            scene->registry_.Add<RigidBodyComponent>(
                node->GetId(), RigidBodyComponent{1.0f, glm::vec3(halfBox), pos});
            scene->registry_.Add<FusionDataComponent>(node->GetId(), FusionDataComponent{});
            root->AddChild(node);

            const float orbitalSpeed = glm::sqrt(cfg.attractorStrength / cfg.orbitRadius);
            const glm::vec3 tangent(-glm::sin(angle), glm::cos(angle), 0.0f);

            auto* rb = scene->registry_.Get<RigidBodyComponent>(node->GetId());
            if (rb) rb->SetLinearVelocity(tangent * orbitalSpeed);
        }

        spdlog::info("[GravityModule] Gravity scene created -- {} rocks, attractor strength={:.1f}",
                     cfg.boxCount, cfg.attractorStrength);
    }

    // -- Component registration ------------------------------------------------
    void GravityGameModule::RegisterComponents()
    {
        auto& reg = const_cast<Engine*>(engine_)->componentRegistry_;

        reg.Register({
            FusionDataComponent::componentType, "Fusion Data", "Module",
            badge::kModule,
            [](const std::shared_ptr<SceneNode>& n) {
                return n->HasComponent<FusionDataComponent>();
            },
            [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                n->AddComponent<FusionDataComponent>(FusionDataComponent{});
                eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
            },
            [](const std::shared_ptr<SceneNode>& n, Engine&) {
                n->RemoveComponent<FusionDataComponent>();
            },
            [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                if (auto* c = n->GetComponent<FusionDataComponent>()) c->InspectProperties(v);
            }
        });
    }

    void GravityGameModule::UnregisterComponents()
    {
        const_cast<Engine*>(engine_)->componentRegistry_.Unregister(FusionDataComponent::componentType);
    }

} // namespace gravity
