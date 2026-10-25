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
        if (ImGui::InputText(u8"名前", nameBuffer, 256)) {
            s_selectedObject->SetName(nameBuffer);
        }

        ImGui::SameLine();
        if (ImGui::Button(u8"削除")) {
            s_selectedObject->Destroy();
            s_selectedObject = nullptr;
        }

        if (s_selectedObject) {
            ImGui::SameLine();
            if (ImGui::Button(u8"プレハブとして保存")) {
                ImGui::OpenPopup("SavePrefabPopup");
            }

            if (ImGui::BeginPopup("SavePrefabPopup")) {
                static char prefabName[256] = "";
                if (prefabName[0] == '\0') {
                    strcpy_s(prefabName, s_selectedObject->GetName().c_str());
                }
                ImGui::InputText(u8"ファイル名", prefabName, 256);
                ImGui::Text(".json will be appended");
                if (ImGui::Button(u8"保存")) {
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
                if (ImGui::CollapsingHeader(u8"トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& t = *pTrans;
                    ImGui::DragFloat3(u8"位置", &t.m_position.x, 0.1f);
                    
                    Math::Vector3 rotDeg = t.m_rotation;
                    rotDeg.x = DirectX::XMConvertToDegrees(rotDeg.x);
                    rotDeg.y = DirectX::XMConvertToDegrees(rotDeg.y);
                    rotDeg.z = DirectX::XMConvertToDegrees(rotDeg.z);
                    if (ImGui::DragFloat3(u8"回転 (度)", &rotDeg.x, 0.5f)) {
                        t.m_rotation.x = DirectX::XMConvertToRadians(rotDeg.x);
                        t.m_rotation.y = DirectX::XMConvertToRadians(rotDeg.y);
                        t.m_rotation.z = DirectX::XMConvertToRadians(rotDeg.z);
                    }
                    ImGui::DragFloat3(u8"拡大率", &t.m_scale.x, 0.1f);
                }
            }

            if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity)) {
                if (ImGui::CollapsingHeader(u8"モデル", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& m = *pModel;
                    ImGui::Text(u8"ファイル: %s", m.m_filePath.c_str());
                    
                    int modelType = static_cast<int>(m.m_modelType);
                    ImGui::RadioButton(u8"静的", &modelType, 0); ImGui::SameLine();
                    ImGui::RadioButton(u8"動的", &modelType, 1);
                    m.m_modelType = static_cast<ModelType>(modelType);
                    ImGui::Checkbox(u8"個別にアニメーション", &m.m_perInstanceAnimation);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"同じモデルを複数置いても、このオブジェクトだけ別々にアニメーションさせる(ノードアニメーションのみ)");
                }
            }

            if (auto* pSprite = ecs.TryGetComponent<SpriteData>(entity)) {
                if (ImGui::CollapsingHeader(u8"スプライト", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& s = *pSprite;
                    ImGui::Text(u8"テクスチャ: %s", s.m_filePath.c_str());

                    ImGui::DragFloat2(u8"サイズ", &s.m_size.x, 1.0f);
                    ImGui::DragFloat2(u8"基準点", &s.m_pivot.x, 0.01f, 0.0f, 1.0f);
                    ImGui::ColorEdit4(u8"色", &s.m_color.x);
                    ImGui::InputInt(u8"描画順", &s.m_orderInLayer);
                }
            }

            if (auto* pCam = ecs.TryGetComponent<CameraData>(entity)) {
                if (ImGui::CollapsingHeader(u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& c = *pCam;
                    const char* modes[] = { u8"エディタ自由視点", u8"三人称 (TPS)", u8"一人称 (FPS)" };
                    int modeInt = static_cast<int>(c.m_cameraMode);
                    if (ImGui::Combo(u8"カメラモード", &modeInt, modes, IM_ARRAYSIZE(modes))) {
                        c.m_cameraMode = static_cast<CameraMode>(modeInt);
                    }
                    ImGui::DragFloat3(u8"三人称のオフセット", &c.m_targetOffset.x, 0.1f);
                    ImGui::DragFloat3(u8"一人称のオフセット", &c.m_fpsOffset.x, 0.1f);
                    ImGui::DragFloat(u8"視野角", &c.m_fov, 0.5f, 1.0f, 179.0f);
                    ImGui::DragFloat(u8"描画距離 (近)", &c.m_nearZ, 0.1f);
                    ImGui::DragFloat(u8"描画距離 (遠)", &c.m_farZ, 1.0f);
                    ImGui::DragFloat(u8"移動速度 (m/s)", &c.m_moveSpeed, 0.1f, 0.1f, 100.0f);

                    ImGui::Separator();
                    ImGui::Checkbox(u8"プレビュー用カメラにする", &c.m_isDebugPreview);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            u8"ONにすると、このカメラのTransform(位置・回転)から見た絵を毎フレーム\n"
                            u8"専用のプレビューRTに描画する。「Window > Preview Camera」で結果を確認できる。\n"
                            u8"普通のGameObjectなので、Transformで自由に動かして任意の角度から\n"
                            u8"シーンやガラス面の反射を確認できる(エディタのフリーカメラや反射カメラとは無関係)。\n"
                            u8"同時に複数のカメラでONにした場合、見つかった最初の1つだけが使われる。");
                    }
                }
            }

            if (auto* pCol = ecs.TryGetComponent<ColliderData>(entity)) {
                if (ImGui::CollapsingHeader(u8"当たり判定", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& col = ecs.GetComponent<ColliderData>(entity);
                    ImGui::Checkbox(u8"動かない (静的)", &col.m_isStatic);

                    ImGui::TextDisabled(u8"形状");
                    if (ImGui::Button(u8"箱を追加")) { col.m_shapes.push_back(std::make_shared<CollisionShapeBox>()); } ImGui::SameLine();
                    if (ImGui::Button(u8"球を追加")) { col.m_shapes.push_back(std::make_shared<CollisionShapeSphere>()); } ImGui::SameLine();
                    if (ImGui::Button(u8"カプセルを追加")) { col.m_shapes.push_back(std::make_shared<CollisionShapeCapsule>()); }

                    for (size_t i = 0; i < col.m_shapes.size(); ++i) {
                        ImGui::PushID((int)i);
                        auto& shape = col.m_shapes[i];
                        if (ImGui::TreeNode(shape->m_name.c_str())) {
                            shape->Editor_ImGui();
                            ImGui::TreePop();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(u8"削除")) {
                            col.m_shapes.erase(col.m_shapes.begin() + i);
                            --i;
                        }
                        ImGui::PopID();
                    }
                }
            }
            if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
                if (ImGui::CollapsingHeader(u8"アニメーション", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                                
                                if (ImGui::Combo(u8"再生するアニメーション", &currentIdx, animNames.data(), (int)animNames.size())) {
                                    animData.currentAnim.AnimationIndex = currentIdx;
                                }
                            } else {
                                ImGui::Text(u8"モデルにアニメーションがありません");
                            }
                        }
                    } else {
                        ImGui::InputInt(u8"アニメーション番号", &animData.currentAnim.AnimationIndex);
                    }

                    ImGui::DragFloat(u8"再生位置", &animData.currentAnim.ProgressTime, 0.01f);
                    ImGui::DragFloat(u8"再生速度", &animData.currentAnim.Speed, 0.01f);
                    
                    if (ImGui::Button(u8"再生")) {
                        animData.currentAnim.IsPlaying = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"一時停止")) {
                        animData.currentAnim.IsPlaying = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"停止")) {
                        animData.currentAnim.IsPlaying = false;
                        animData.currentAnim.ProgressTime = 0.0f;
                    }

                    ImGui::Checkbox(u8"ループ再生", &animData.currentAnim.IsLoop);
                }
            }
            if (auto* pLight = ecs.TryGetComponent<PointLightData>(entity)) {
                if (ImGui::CollapsingHeader(u8"点光源", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& l = *pLight;
                    ImGui::Checkbox(u8"有効", &l.m_enabled);
                    ImGui::ColorEdit3(u8"色味", &l.m_color.x);
                    ImGui::DragFloat(u8"色温度 (K)", &l.m_temperature, 10.0f, 0.0f, 12000.0f, "%.0f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"0なら色温度を使わず色味だけで決める。ろうそく約1800K、白熱電球約2700K");
                    ImGui::DragFloat(u8"光束 (lm)", &l.m_intensity, 1.0f, 0.0f, 100000.0f, "%.1f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"ろうそく約12 lm、60W相当の電球約800 lm");
                    ImGui::DragFloat(u8"届く距離", &l.m_range, 0.1f, 0.1f, 100.0f);
                    ImGui::Checkbox(u8"影を落とす", &l.m_castShadow);
                    ImGui::DragFloat(u8"光源の半径 (m)", &l.m_sourceRadius, 0.001f, 0.001f, 0.5f, "%.3f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"大きいほど影の輪郭がぼける。ろうそくの炎約0.01m、電球約0.03m");
                    ImGui::Separator();
                    ImGui::Text(u8"明滅");
                    ImGui::Checkbox(u8"明滅させる", &l.m_flickerEnabled);
                    ImGui::DragFloat(u8"明滅の速さ", &l.m_flickerSpeed, 0.1f, 0.0f, 60.0f);
                    ImGui::DragFloat(u8"明滅の強さ", &l.m_flickerIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat(u8"明滅のずらし値", &l.m_flickerSeed, 0.1f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"同じ部屋に複数のフリッカーライトを置く時、位相をずらしてバラバラに明滅させたい場合に変える。");
                }
            }
            if (auto* pSpot = ecs.TryGetComponent<SpotLightData>(entity)) {
                if (ImGui::CollapsingHeader(u8"スポットライト", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& l = *pSpot;
                    ImGui::PushID("SpotLightData");
                    ImGui::Checkbox(u8"有効", &l.m_enabled);
                    ImGui::ColorEdit3(u8"色味", &l.m_color.x);
                    ImGui::DragFloat(u8"色温度 (K)", &l.m_temperature, 10.0f, 0.0f, 12000.0f, "%.0f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"0なら色温度を使わず色味だけで決める。白熱電球約2700K、LEDライト約5000K");
                    ImGui::DragFloat(u8"光度 (cd)", &l.m_intensity, 1.0f, 0.0f, 100000.0f, "%.1f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"中心方向の明るさ。小型のLEDライト約1000 cd、60W相当の電球を下向きに約130 cd");
                    ImGui::DragFloat(u8"届く距離", &l.m_range, 0.1f, 0.1f, 100.0f);
                    ImGui::Checkbox(u8"影を落とす", &l.m_castShadow);
                    ImGui::DragFloat(u8"光源の半径 (m)", &l.m_sourceRadius, 0.001f, 0.001f, 0.5f, "%.3f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"大きいほど影の輪郭がぼける。ろうそくの炎約0.01m、電球約0.03m");
                    ImGui::DragFloat(u8"内側の角度", &l.m_innerAngle, 0.5f, 0.0f, 89.0f);
                    ImGui::DragFloat(u8"外側の角度", &l.m_outerAngle, 0.5f, 0.1f, 89.0f);
                    ImGui::DragFloat3(u8"光源の位置", &l.m_localOffset.x, 0.01f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"光源位置(このオブジェクトのローカル空間。Scaleも掛かる)");
                    ImGui::DragFloat3(u8"照らす向き", &l.m_localDirection.x, 0.01f, -1.0f, 1.0f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"照らす向き(このオブジェクトのローカル空間)");
                    ImGui::DragFloat(u8"モデルの発光 (nit)", &l.m_modelGlow, 10.0f, 0.0f, 1000000.0f, "%.0f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"点灯中にモデル自体を光らせる輝度(cd/m2)。電球のかさや蛍光灯の表面は数千nit程度");
                    char glowMatBuf[64] = {};
                    strncpy_s(glowMatBuf, l.m_glowMaterial.c_str(), _TRUNCATE);
                    if (ImGui::InputText(u8"発光させるマテリアル", glowMatBuf, sizeof(glowMatBuf))) l.m_glowMaterial = glowMatBuf;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"空なら全メッシュ。指定時はこの名前のマテリアルだけ光らせる");
                    char reflMatBuf[64] = {};
                    strncpy_s(reflMatBuf, l.m_glowReflectMaterial.c_str(), _TRUNCATE);
                    if (ImGui::InputText(u8"反射で光るマテリアル", reflMatBuf, sizeof(reflMatBuf))) l.m_glowReflectMaterial = reflMatBuf;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"光源の光を受けて明るく見える面(かさの内側など)。このマテリアルの裏面だけを光らせる");
                    ImGui::DragFloat(u8"反射で光る強さ", &l.m_glowReflectScale, 0.01f, 0.0f, 2.0f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"反射で光る面の明るさ(Model Glowに対する倍率)");
                    ImGui::PopID();
                }
            }
            if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(entity)) {
                if (ImGui::CollapsingHeader(u8"スクリプト", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& scriptData = ecs.GetComponent<NativeScriptData>(entity);
                    ImGui::Text(u8"スクリプト名: %s", scriptData.ScriptName.c_str());
                    if (scriptData.Instance) {
                        scriptData.Instance->ImGuiUpdate();
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::Button(u8"コンポーネントを追加", ImVec2(-1, 0))) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (ImGui::MenuItem(u8"新しいスクリプトを作成...")) {
                    s_showCreateScriptPopup = true;
                    s_newScriptName[0] = '\0';
                }
                ImGui::Separator();

                ImGui::TextDisabled(u8"--- スクリプト ---");
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
                
                ImGui::TextDisabled(u8"--- コンポーネント ---");
                bool hasTrans = ecs.TryGetComponent<TransformData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"トランスフォーム", nullptr, false, !hasTrans)) ecs.AddComponent<TransformData>(entity, TransformData{});
                
                bool hasModel = ecs.TryGetComponent<ModelRenderData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"モデル", nullptr, false, !hasModel)) ecs.AddComponent<ModelRenderData>(entity, ModelRenderData{});

                bool hasSprite = ecs.TryGetComponent<SpriteData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"スプライト", nullptr, false, !hasSprite)) ecs.AddComponent<SpriteData>(entity, SpriteData{});
                
                bool hasCam = ecs.TryGetComponent<CameraData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"カメラ", nullptr, false, !hasCam)) ecs.AddComponent<CameraData>(entity, CameraData{});
                
                bool hasCol = ecs.TryGetComponent<ColliderData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"当たり判定", nullptr, false, !hasCol)) ecs.AddComponent<ColliderData>(entity, ColliderData{});

                bool hasAnim = ecs.TryGetComponent<AnimationDataComponent>(entity) != nullptr;
                if (ImGui::MenuItem(u8"アニメーション", nullptr, false, !hasAnim)) ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});

                bool hasPointLight = ecs.TryGetComponent<PointLightData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"点光源", nullptr, false, !hasPointLight)) ecs.AddComponent<PointLightData>(entity, PointLightData{});

                bool hasSpotLight = ecs.TryGetComponent<SpotLightData>(entity) != nullptr;
                if (ImGui::MenuItem(u8"スポットライト", nullptr, false, !hasSpotLight)) ecs.AddComponent<SpotLightData>(entity, SpotLightData{});

                ImGui::EndPopup();
            }

            if (s_showCreateScriptPopup) {
                ImGui::OpenPopup(u8"新しいスクリプトを作成");
            }
            if (ImGui::BeginPopupModal(u8"新しいスクリプトを作成", &s_showCreateScriptPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("Script Name", s_newScriptName, IM_ARRAYSIZE(s_newScriptName));
                ImGui::InputText(u8"保存先", s_newScriptPath, IM_ARRAYSIZE(s_newScriptPath));
                
                if (ImGui::Button(u8"作成", ImVec2(120, 0))) {
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
                if (ImGui::Button(u8"キャンセル", ImVec2(120, 0))) {
                    s_showCreateScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } 
    } else {
        ImGui::Text(u8"何も選択されていません");
    }
    ImGui::End();
}
