#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../EditHistory.h"

namespace
{
    // シーンのライトを、Undo/Redoで値を書き戻せる対象として登録する。対象の名前は「種類#エンティティの番号」
    // (Undoの履歴はシーンを切り替えると消えるので、エンティティの番号で探してよい)
    template<typename TComponent>
    std::string RegisterLightTarget(const char* pKind, Entity entity)
    {
        const std::string target = std::string(pKind) + "#" + std::to_string(entity);
        EditHistory::Instance().RegisterTarget(target, [entity](const std::string& fieldKey, const nlohmann::json& value)
        {
            auto* pComponent = GameManager::Instance().GetECS().TryGetComponent<TComponent>(entity);
            return pComponent && SettingsFieldUI::ApplyFieldJson(*pComponent, fieldKey, value);
        });
        return target;
    }

    nlohmann::json ToJson(const Math::Vector3& value)
    {
        return nlohmann::json::array({ value.x, value.y, value.z });
    }

    bool FromJson(const nlohmann::json& json, Math::Vector3& outValue)
    {
        if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() || !json[2].is_number())
        {
            return false;
        }
        outValue = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
        return true;
    }

    // 回転はラジアンで持つが、Undoの知らせで読めるよう度で記録する
    Math::Vector3 ToDegrees(const Math::Vector3& radians)
    {
        return { DirectX::XMConvertToDegrees(radians.x), DirectX::XMConvertToDegrees(radians.y), DirectX::XMConvertToDegrees(radians.z) };
    }

    Math::Vector3 ToRadians(const Math::Vector3& degrees)
    {
        return { DirectX::XMConvertToRadians(degrees.x), DirectX::XMConvertToRadians(degrees.y), DirectX::XMConvertToRadians(degrees.z) };
    }

    // トランスフォームを、Undo/Redoで値を書き戻せる対象として登録する(「Transform#エンティティの番号」)
    std::string RegisterTransformTarget(Entity entity)
    {
        const std::string target = "Transform#" + std::to_string(entity);
        EditHistory::Instance().RegisterTarget(target, [entity](const std::string& fieldKey, const nlohmann::json& value)
        {
            auto* pTransform = GameManager::Instance().GetECS().TryGetComponent<TransformData>(entity);
            Math::Vector3 vector;
            if (!pTransform || !FromJson(value, vector))
            {
                return false;
            }
            if (fieldKey == "position")
            {
                pTransform->m_position = vector;
            }
            else if (fieldKey == "rotation")
            {
                pTransform->m_rotation = ToRadians(vector);
            }
            else if (fieldKey == "scale")
            {
                pTransform->m_scale = vector;
            }
            else
            {
                return false;
            }
            return true;
        });
        return target;
    }

    // 名前を、Undo/Redoで値を書き戻せる対象として登録する(「Name#エンティティの番号」)。物が消えていたら戻さない
    std::string RegisterNameTarget(const std::shared_ptr<GameObject>& spObject)
    {
        const std::string target = "Name#" + std::to_string(spObject->GetEntityID());
        std::weak_ptr<GameObject> wpObject = spObject;
        EditHistory::Instance().RegisterTarget(target, [wpObject](const std::string& fieldKey, const nlohmann::json& value)
        {
            const std::shared_ptr<GameObject> spTarget = wpObject.lock();
            if (!spTarget || fieldKey != "name" || !value.is_string())
            {
                return false;
            }
            spTarget->SetName(value.get<std::string>());
            return true;
        });
        return target;
    }

    // 直前に描いた部品の操作を、Undoの1回として記録する。操作の開始で変更前(before: 部品を描く前の値)を控え、
    // 操作を終えて値が変わっていたら積む(ドラッグの途中はフレームごとに積まない)
    void RecordItemEdit(const std::string& target, const char* pKey, const char* pLabel, const nlohmann::json& before, const nlohmann::json& after)
    {
        static std::unordered_map<std::string, nlohmann::json> s_pendingBefore;
        const std::string pendingKey = target + "/" + pKey;
        if (ImGui::IsItemActivated())
        {
            s_pendingBefore[pendingKey] = before;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            const auto it = s_pendingBefore.find(pendingKey);
            const nlohmann::json committedBefore = (it != s_pendingBefore.end()) ? it->second : before;
            if (it != s_pendingBefore.end())
            {
                s_pendingBefore.erase(it);
            }
            EditHistory::Instance().PushChange({ target, pKey, pLabel, committedBefore, after });
        }
    }
}

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
        // Name Edit(入力を終えたときにUndoの1回として記録する)
        char nameBuffer[256];
        strcpy_s(nameBuffer, s_selectedObject->GetName().c_str());
        const std::string nameBefore = s_selectedObject->GetName();
        if (ImGui::InputText(u8"名前", nameBuffer, 256)) {
            s_selectedObject->SetName(nameBuffer);
        }
        RecordItemEdit(RegisterNameTarget(s_selectedObject), "name", u8"名前", nameBefore, s_selectedObject->GetName());

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
                    // どの値も、操作を終えたときにUndoの1回として記録する(シェーダーエディターと同じ履歴)
                    const std::string transformTarget = RegisterTransformTarget(entity);
                    const nlohmann::json positionBefore = ToJson(t.m_position);
                    ImGui::DragFloat3(u8"位置", &t.m_position.x, 0.1f);
                    RecordItemEdit(transformTarget, "position", u8"位置", positionBefore, ToJson(t.m_position));

                    Math::Vector3 rotDeg = ToDegrees(t.m_rotation);
                    const nlohmann::json rotationBefore = ToJson(rotDeg);
                    if (ImGui::DragFloat3(u8"回転 (度)", &rotDeg.x, 0.5f)) {
                        t.m_rotation = ToRadians(rotDeg);
                    }
                    RecordItemEdit(transformTarget, "rotation", u8"回転", rotationBefore, ToJson(rotDeg));

                    const nlohmann::json scaleBefore = ToJson(t.m_scale);
                    ImGui::DragFloat3(u8"拡大率", &t.m_scale.x, 0.1f);
                    RecordItemEdit(transformTarget, "scale", u8"拡大率", scaleBefore, ToJson(t.m_scale));
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
                    // 値の表示名・説明・単位・目安はPointLightData::VisitFieldsにある。Undo/Redoもここで記録する
                    const std::string target = RegisterLightTarget<PointLightData>("PointLight", entity);
                    SettingsFieldUI::Options options;
                    options.isSearchable = false;
                    SettingsFieldUI::Draw(*pLight, target.c_str(), options);
                }
            }
            if (auto* pSpot = ecs.TryGetComponent<SpotLightData>(entity)) {
                if (ImGui::CollapsingHeader(u8"スポットライト", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& l = *pSpot;
                    const std::string target = RegisterLightTarget<SpotLightData>("SpotLight", entity);
                    SettingsFieldUI::Options options;
                    options.isSearchable = false;
                    SettingsFieldUI::Draw(l, target.c_str(), options);
                    // マテリアルの名前(文字列)は部品の一覧に無いので、入力欄で描く
                    ImGui::PushID("SpotLightData");
                    char glowMatBuf[64] = {};
                    strncpy_s(glowMatBuf, l.m_glowMaterial.c_str(), _TRUNCATE);
                    if (ImGui::InputText(u8"発光させるマテリアル", glowMatBuf, sizeof(glowMatBuf))) l.m_glowMaterial = glowMatBuf;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"空なら全メッシュ。指定時はこの名前のマテリアルだけ光らせる");
                    char reflMatBuf[64] = {};
                    strncpy_s(reflMatBuf, l.m_glowReflectMaterial.c_str(), _TRUNCATE);
                    if (ImGui::InputText(u8"反射で光るマテリアル", reflMatBuf, sizeof(reflMatBuf))) l.m_glowReflectMaterial = reflMatBuf;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"光源の光を受けて明るく見える面(かさの内側など)。このマテリアルの裏面だけを光らせる");
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
