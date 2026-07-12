#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}

static bool s_showCreateScriptPopup = false;
static char s_newScriptName[256] = "";
static char s_newScriptPath[256] = "Source/Application/Object/Script/";

static void GenerateScriptFiles(const std::string& name, const std::string& path) {
    std::filesystem::create_directories(path);
    std::string hFile = path + name + ".h";
    std::string cppFile = path + name + ".cpp";

    std::ofstream hout(hFile);
    if (hout.is_open()) {
        hout << "#pragma once\n";
        hout << "#include \"../../../Framework/ECS/Components/Data/NativeScriptData.h\"\n\n";
        hout << "class " << name << " : public NativeScript {\n";
        hout << "public:\n";
        hout << "    void Start() override;\n";
        hout << "    void Update(float deltaTime) override;\n";
        hout << "    void ImGuiUpdate() override;\n";
        hout << "};\n";
    }

    std::ofstream cout(cppFile);
    if (cout.is_open()) {
        cout << "#include \"../../../Pch.h\"\n";
        cout << "#include \"" << name << ".h\"\n\n";
        cout << "void " << name << "::Start() {\n}\n\n";
        cout << "void " << name << "::Update() {\n}\n\n";
        cout << "void " << name << "::ImGuiUpdate() {\n}\n";
    }
}


void Editor::DrawInspector() {
    ImGui::Begin("Inspector");
    if (s_selectedObject) 
    {
        // Name Edit
        char nameBuffer[256];
        strcpy_s(nameBuffer, s_selectedObject->GetName().c_str());
        if (ImGui::InputText("Name", nameBuffer, 256)) {
            s_selectedObject->SetName(nameBuffer);
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            s_selectedObject->Destroy();
            s_selectedObject = nullptr;
        }

        if (s_selectedObject) {
            ImGui::SameLine();
            if (ImGui::Button("Save as Prefab")) {
                ImGui::OpenPopup("SavePrefabPopup");
            }

            if (ImGui::BeginPopup("SavePrefabPopup")) {
                static char prefabName[256] = "";
                if (prefabName[0] == '\0') {
                    strcpy_s(prefabName, s_selectedObject->GetName().c_str());
                }
                ImGui::InputText("Filename", prefabName, 256);
                ImGui::Text(".json will be appended");
                if (ImGui::Button("Save")) {
                    auto sceneObj = GetCurrentScenePtr();
                    if (sceneObj) {
                        nlohmann::json pj = sceneObj->SerializeGameObject(s_selectedObject);
                        std::string path = "Asset/Data/Prefab/" + std::string(prefabName) + ".json";
                        std::ofstream o(path);
                        if (o.is_open()) {
                            o << std::setw(4) << pj << std::endl;
                            Logger::Instance().AddLog(Logger::LogLevel::Info, ("Prefab saved: " + path).c_str());
                        } else {
                            Logger::Instance().AddLog(Logger::LogLevel::Error, ("Failed to save Prefab: " + path).c_str());
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();

                        // Components
            auto entity = s_selectedObject->GetEntityID();
            auto& ecs = GameManager::Instance().GetECS();

            if (auto* pTrans = ecs.TryGetComponent<TransformData>(entity)) {
                if (ImGui::CollapsingHeader("TransformData", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& t = *pTrans;
                    ImGui::DragFloat3("Position", &t.m_position.x, 0.1f);
                    
                    Math::Vector3 rotDeg = t.m_rotation;
                    rotDeg.x = DirectX::XMConvertToDegrees(rotDeg.x);
                    rotDeg.y = DirectX::XMConvertToDegrees(rotDeg.y);
                    rotDeg.z = DirectX::XMConvertToDegrees(rotDeg.z);
                    if (ImGui::DragFloat3("Rotation", &rotDeg.x, 0.5f)) {
                        t.m_rotation.x = DirectX::XMConvertToRadians(rotDeg.x);
                        t.m_rotation.y = DirectX::XMConvertToRadians(rotDeg.y);
                        t.m_rotation.z = DirectX::XMConvertToRadians(rotDeg.z);
                    }
                    ImGui::DragFloat3("Scale", &t.m_scale.x, 0.1f);
                }
            }

            if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity)) {
                if (ImGui::CollapsingHeader("ModelRenderData", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& m = *pModel;
                    ImGui::Text("File Path: %s", m.m_filePath.c_str());
                    
                    int modelType = static_cast<int>(m.m_modelType);
                    ImGui::RadioButton("Static", &modelType, 0); ImGui::SameLine();
                    ImGui::RadioButton("Dynamic", &modelType, 1);
                    m.m_modelType = static_cast<ModelType>(modelType);
                }
            }

            if (auto* pSprite = ecs.TryGetComponent<SpriteData>(entity)) {
                if (ImGui::CollapsingHeader("SpriteData", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& s = *pSprite;
                    ImGui::Text("Texture Path: %s", s.m_filePath.c_str());

                    ImGui::DragFloat2("Size", &s.m_size.x, 1.0f);
                    ImGui::DragFloat2("Pivot", &s.m_pivot.x, 0.01f, 0.0f, 1.0f);
                    ImGui::ColorEdit4("Color", &s.m_color.x);
                    ImGui::InputInt("Order In Layer", &s.m_orderInLayer);
                }
            }

            if (auto* pCam = ecs.TryGetComponent<CameraData>(entity)) {
                if (ImGui::CollapsingHeader("CameraData", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& c = *pCam;
                    const char* modes[] = { "EditorFree", "TPS", "FPS" };
                    int modeInt = static_cast<int>(c.m_cameraMode);
                    if (ImGui::Combo("Camera Mode", &modeInt, modes, IM_ARRAYSIZE(modes))) {
                        c.m_cameraMode = static_cast<CameraMode>(modeInt);
                    }
                    ImGui::DragFloat3("TPS Offset", &c.m_targetOffset.x, 0.1f);
                    ImGui::DragFloat3("FPS Offset", &c.m_fpsOffset.x, 0.1f);
                    ImGui::DragFloat("FOV", &c.m_fov, 0.5f, 1.0f, 179.0f);
                    ImGui::DragFloat("Near Z", &c.m_nearZ, 0.1f);
                    ImGui::DragFloat("Far Z", &c.m_farZ, 1.0f);
                }
            }

            if (auto* pCol = ecs.TryGetComponent<ColliderData>(entity)) {
                if (ImGui::CollapsingHeader("ColliderData", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& col = ecs.GetComponent<ColliderData>(entity);
                    ImGui::Checkbox("Is Static", &col.m_isStatic);

                    ImGui::TextDisabled("Shapes");
                    if (ImGui::Button("Add Box")) { col.m_shapes.push_back(std::make_shared<CollisionShapeBox>()); } ImGui::SameLine();
                    if (ImGui::Button("Add Sphere")) { col.m_shapes.push_back(std::make_shared<CollisionShapeSphere>()); } ImGui::SameLine();
                    if (ImGui::Button("Add Capsule")) { col.m_shapes.push_back(std::make_shared<CollisionShapeCapsule>()); }

                    for (size_t i = 0; i < col.m_shapes.size(); ++i) {
                        ImGui::PushID((int)i);
                        auto& shape = col.m_shapes[i];
                        if (ImGui::TreeNode(shape->m_name.c_str())) {
                            shape->Editor_ImGui();
                            ImGui::TreePop();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            col.m_shapes.erase(col.m_shapes.begin() + i);
                            --i;
                        }
                        ImGui::PopID();
                    }
                }
            }
            if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
                if (ImGui::CollapsingHeader("AnimationDataComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& animData = ecs.GetComponent<AnimationDataComponent>(entity);
                    
                    if (auto* pAnimModel = ecs.TryGetComponent<ModelRenderData>(entity)) {
                        auto& modelData = ecs.GetComponent<ModelRenderData>(entity);
                        if (modelData.m_spModelData) {
                            const auto& animations = modelData.m_spModelData->GetAnimations();
                            if (!animations.empty()) {
                                std::vector<const char*> animNames;
                                for (const auto& anim : animations) {
                                    animNames.push_back(anim.name.c_str());
                                }
                                
                                int currentIdx = animData.currentAnim.AnimationIndex;
                                if (currentIdx < 0) currentIdx = 0;
                                if (currentIdx >= (int)animNames.size()) currentIdx = (int)animNames.size() - 1;
                                
                                if (ImGui::Combo("Animation", &currentIdx, animNames.data(), (int)animNames.size())) {
                                    animData.currentAnim.AnimationIndex = currentIdx;
                                }
                            } else {
                                ImGui::Text("No animations in model.");
                            }
                        }
                    } else {
                        ImGui::InputInt("Animation Index", &animData.currentAnim.AnimationIndex);
                    }

                    ImGui::DragFloat("Progress Time", &animData.currentAnim.ProgressTime, 0.01f);
                    ImGui::DragFloat("Speed", &animData.currentAnim.Speed, 0.01f);
                    
                    if (ImGui::Button("Play")) {
                        animData.currentAnim.IsPlaying = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Pause")) {
                        animData.currentAnim.IsPlaying = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop")) {
                        animData.currentAnim.IsPlaying = false;
                        animData.currentAnim.ProgressTime = 0.0f;
                    }

                    ImGui::Checkbox("Is Loop", &animData.currentAnim.IsLoop);
                }
            }
            if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(entity)) {
                if (ImGui::CollapsingHeader("NativeScript", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& scriptData = ecs.GetComponent<NativeScriptData>(entity);
                    ImGui::Text("Script: %s", scriptData.ScriptName.c_str());
                    if (scriptData.Instance) {
                        scriptData.Instance->ImGuiUpdate();
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (ImGui::MenuItem("Create New Script...")) {
                    s_showCreateScriptPopup = true;
                    s_newScriptName[0] = '\0';
                }
                ImGui::Separator();

                ImGui::TextDisabled("--- Native Scripts ---");
                for (const auto& name : ClassAssembly::Instance().GetRegisteredClasses()) {
                    if (ImGui::MenuItem(name.c_str())) {
                        auto newComp = ClassAssembly::Instance().Create(name);
                        if (newComp) {
                            NativeScriptData data;
                            data.ScriptName = name;
                            data.Instance = std::dynamic_pointer_cast<NativeScript>(newComp);
                            data.Instance->SetGameObject(s_selectedObject.get());
                            GameManager::Instance().GetECS().AddComponent<NativeScriptData>(s_selectedObject->GetEntityID(), data);
                        }
                    }
                }
                ImGui::Separator();
                
                ImGui::TextDisabled("--- ECS Components ---");
                bool hasTrans = ecs.TryGetComponent<TransformData>(entity) != nullptr;
                if (ImGui::MenuItem("TransformData", nullptr, false, !hasTrans)) ecs.AddComponent<TransformData>(entity, TransformData{});
                
                bool hasModel = ecs.TryGetComponent<ModelRenderData>(entity) != nullptr;
                if (ImGui::MenuItem("ModelRenderData", nullptr, false, !hasModel)) ecs.AddComponent<ModelRenderData>(entity, ModelRenderData{});

                bool hasSprite = ecs.TryGetComponent<SpriteData>(entity) != nullptr;
                if (ImGui::MenuItem("SpriteData", nullptr, false, !hasSprite)) ecs.AddComponent<SpriteData>(entity, SpriteData{});
                
                bool hasCam = ecs.TryGetComponent<CameraData>(entity) != nullptr;
                if (ImGui::MenuItem("CameraData", nullptr, false, !hasCam)) ecs.AddComponent<CameraData>(entity, CameraData{});
                
                bool hasCol = ecs.TryGetComponent<ColliderData>(entity) != nullptr;
                if (ImGui::MenuItem("ColliderData", nullptr, false, !hasCol)) ecs.AddComponent<ColliderData>(entity, ColliderData{});

                bool hasAnim = ecs.TryGetComponent<AnimationDataComponent>(entity) != nullptr;
                if (ImGui::MenuItem("AnimationDataComponent", nullptr, false, !hasAnim)) ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});
                
                ImGui::EndPopup();
            }

            if (s_showCreateScriptPopup) {
                ImGui::OpenPopup("Create New Script Component");
            }
            if (ImGui::BeginPopupModal("Create New Script Component", &s_showCreateScriptPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("Script Name", s_newScriptName, IM_ARRAYSIZE(s_newScriptName));
                ImGui::InputText("Save Path", s_newScriptPath, IM_ARRAYSIZE(s_newScriptPath));
                
                if (ImGui::Button("Create", ImVec2(120, 0))) {
                    std::string name = s_newScriptName;
                    std::string path = s_newScriptPath;
                    if (!name.empty()) {
                        GenerateScriptFiles(name, path);
                    }
                    s_showCreateScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    s_showCreateScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } 
    } else {
        ImGui::Text("Nothing Selected");
    }
    ImGui::End();
}
