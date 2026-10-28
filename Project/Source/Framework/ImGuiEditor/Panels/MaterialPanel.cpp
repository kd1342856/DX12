#include "MaterialPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../Widgets/FieldWidgets.h"
#include "../EditHistory.h"
#include "../../../Framework/ECS/Components/Data/ModelRenderData.h"
#include "../../../Graphics/Geometry/Model/Model.h"
#include "../../../Graphics/Geometry/Mesh/Mesh.h"
#include "../../../Graphics/GPUResource/Material/MaterialOverride.h"
#include "../../../Framework/Manager/Asset/MeshManager.h"
#include "../../../Framework/Manager/Asset/ModelMeta.h"
#include "../../../Framework/Manager/Resource/ResourceManager.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/ECS.h"
#include "../../Object/GameObject.h"
#include <map>
#include <unordered_set>

namespace
{
    struct FieldDefinition
    {
        MaterialField field;
        FieldMeta meta;
    };

    // エディターで変えられる値(描き方(PSO)に関わらない値だけ)。並びはMaterialFieldと同じ
    const std::vector<FieldDefinition>& GetFieldDefinitions()
    {
        static const std::vector<FieldDefinition> s_definitions =
        {
            { MaterialField::BaseColor, Field("baseColor", u8"色",
                u8"表面の色(リニア)。テクスチャがあれば、その色に掛ける。\n実在の物の反射率はおよそ0.03(炭)～0.9(新雪)。").Kind(FieldKind::Color).Section(u8"表面") },
            { MaterialField::Opacity, Field("opacity", u8"不透明度",
                u8"色のアルファ。半透明とアルファテストのマテリアルで、どれだけ透けるかに効く(不透明のマテリアルでは変わらない)。").Range(0.0f, 1.0f) },
            { MaterialField::Metallic, Field("metallicFactor", u8"金属らしさ",
                u8"0は非金属(木・布・石)、1は金属。間の値は、錆や汚れが混ざった所だけに使う。テクスチャがあれば、その値に掛ける。").Range(0.0f, 1.0f) },
            { MaterialField::Roughness, Field("roughnessFactor", u8"粗さ",
                u8"0はつるつる(鏡のように映り込む)、1はざらざら。テクスチャがあれば、その値に掛ける。").Range(0.0f, 1.0f)
                .Ref(u8"磨いた金属・ガラス", 0.05f).Ref(u8"ニスを塗った木", 0.35f).Ref(u8"素焼き・漆喰", 0.7f).Ref(u8"畳・布", 0.85f) },
            { MaterialField::NormalScale, Field("normalScale", u8"凹凸の強さ",
                u8"法線マップの凹凸を何倍にするか。1でテクスチャのまま、0で平ら。").Range(0.0f, 2.0f).Section(u8"凹凸と隅の暗さ") },
            { MaterialField::OcclusionStrength, Field("occlusionStrength", u8"隅の暗さ(AO)の強さ",
                u8"AOのテクスチャで、隅や隙間を暗くする強さ。1でテクスチャのまま、0で使わない。").Range(0.0f, 1.0f) },
            { MaterialField::EmissiveColor, Field("emissiveColor", u8"発光の色",
                u8"自分で光る色。黒なら光らない。テクスチャがあれば、その色に掛ける。").Kind(FieldKind::Color).Section(u8"発光") },
            { MaterialField::EmissiveStrength, Field("emissiveStrength", u8"発光の明るさ",
                u8"発光の色を白(1, 1, 1)としたときの輝度。glTFには単位が無いので、読み込むときは100 nit(一般的な画面の白)にしている。").Unit("nit").Range(0.0f, 2000.0f)
                .Ref(u8"テレビの画面", 120.0f).Ref(u8"スマートフォンの画面", 400.0f).Ref(u8"乳白ガラスの照明", 600.0f).Ref(u8"ロウソクの炎", 10000.0f) },
            { MaterialField::EmissiveFrontScale, Field("emissiveFrontScale", u8"表の面の発光",
                u8"表の面(頂点の法線がカメラを向く面)の発光を何倍にするか。かさの内側だけを光らせるときなどに使う。").Range(0.0f, 1.0f).Advanced() },
            { MaterialField::EmissiveBackScale, Field("emissiveBackScale", u8"裏の面の発光",
                u8"裏の面(頂点の法線がカメラと反対を向く面)の発光を何倍にするか。").Range(0.0f, 1.0f).Advanced() },
        };
        return s_definitions;
    }

    // 同じglTFのマテリアルを写したメッシュのまとまり
    struct MaterialGroup
    {
        std::vector<Mesh*> meshes;
    };

    std::string MakeTargetKey(const std::string& modelPath, int sourceIndex)
    {
        return "Material#" + modelPath + "#" + std::to_string(sourceIndex);
    }

    // 上書きを.metaの側に入れ、そのモデルの同じglTFのマテリアルを写した全メッシュに掛け直す
    bool ApplyOverride(const std::string& modelPath, int sourceIndex, const MaterialOverride& materialOverride)
    {
        ModelMetaManager::Instance().SetOverride(modelPath, sourceIndex, materialOverride);
        const std::shared_ptr<ModelData> spModel = ResourceManager::Instance().GetModel(modelPath);
        if (!spModel || !spModel->IsLoaded())
        {
            return false;
        }
        for (auto& node : spModel->GetNodesRef())
        {
            for (auto& meshHandle : node.meshes)
            {
                Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
                if (pMesh && pMesh->GetMaterialRef().SourceIndex == sourceIndex)
                {
                    materialOverride.ApplyTo(pMesh->GetMaterialRef());
                }
            }
        }
        return true;
    }

    // Undo/Redoで値を書き戻す関数を、対象(モデルとマテリアルの番号)ごとに一度だけ登録する。
    // 値がnullなら上書きを外す(glTFの値に戻す)
    void RegisterUndoTarget(const std::string& targetKey, const std::string& modelPath, int sourceIndex)
    {
        static std::unordered_set<std::string> s_registeredTargets;
        if (!s_registeredTargets.insert(targetKey).second)
        {
            return;
        }
        EditHistory::Instance().RegisterTarget(targetKey, [modelPath, sourceIndex](const std::string& fieldKey, const nlohmann::json& value)
        {
            MaterialField field;
            if (!MaterialFields::FindByKey(fieldKey, field))
            {
                return false;
            }
            MaterialOverride materialOverride = ModelMetaManager::Instance().GetOverride(modelPath, sourceIndex);
            if (value.is_null())
            {
                materialOverride.Clear(field);
            }
            else
            {
                MaterialConstants constants = materialOverride.values;
                if (!MaterialFields::FromJson(value, constants, field))
                {
                    return false;
                }
                materialOverride.Set(field, constants);
            }
            return ApplyOverride(modelPath, sourceIndex, materialOverride);
        });
    }

    // Undoに積む値。上書きしていなければnull
    nlohmann::json CaptureField(const MaterialOverride& materialOverride, MaterialField field)
    {
        return materialOverride.Has(field) ? MaterialFields::ToJson(materialOverride.values, field) : nlohmann::json();
    }

    // 値1つの行を描く。valueは今の値で、変えたら書き換える
    FieldWidgets::EditResult DrawFieldRow(const FieldDefinition& definition, MaterialConstants& value, const MaterialConstants& imported, bool isOverridden)
    {
        // 「変えた印」と「初期値に戻す」は、値の比較ではなく上書き中かどうかで決める
        FieldWidgets::SetNextModified(isOverridden);
        if (MaterialFields::IsColor(definition.field))
        {
            DirectX::XMFLOAT3 color = MaterialFields::GetColor(value, definition.field);
            const FieldWidgets::EditResult result = FieldWidgets::Color(definition.meta, color, MaterialFields::GetColor(imported, definition.field));
            if (result.changed)
            {
                MaterialFields::SetColor(value, definition.field, color);
            }
            return result;
        }
        float number = MaterialFields::GetFloat(value, definition.field);
        const FieldWidgets::EditResult result = FieldWidgets::Number(definition.meta, number, MaterialFields::GetFloat(imported, definition.field));
        if (result.changed)
        {
            MaterialFields::SetFloat(value, definition.field, number);
        }
        return result;
    }

    // 1つのマテリアルの値を描く
    void DrawMaterialFields(const std::string& modelPath, int sourceIndex, const MaterialGroup& group)
    {
        const Material& representative = group.meshes.front()->GetMaterialRef();
        const std::string targetKey = MakeTargetKey(modelPath, sourceIndex);
        RegisterUndoTarget(targetKey, modelPath, sourceIndex);
        // 操作を始めたときの値。操作を終えたときに、変更前としてUndoに積む
        static std::unordered_map<std::string, nlohmann::json> s_pendingBefore;

        auto drawRow = [&](const FieldDefinition& definition)
        {
            MaterialOverride materialOverride = ModelMetaManager::Instance().GetOverride(modelPath, sourceIndex);
            const nlohmann::json before = CaptureField(materialOverride, definition.field);
            MaterialConstants value = representative.Constants;
            const FieldWidgets::EditResult result = DrawFieldRow(definition, value, representative.ImportedConstants, materialOverride.Has(definition.field));

            const std::string pendingKey = targetKey + "/" + definition.meta.key;
            if (result.activated && !result.committed)
            {
                s_pendingBefore[pendingKey] = before;
            }
            if (result.changed)
            {
                if (result.reset)
                {
                    materialOverride.Clear(definition.field);
                }
                else
                {
                    materialOverride.Set(definition.field, value);
                }
                ApplyOverride(modelPath, sourceIndex, materialOverride);
            }
            if (result.committed)
            {
                const auto it = s_pendingBefore.find(pendingKey);
                const nlohmann::json committedBefore = (it != s_pendingBefore.end()) ? it->second : before;
                if (it != s_pendingBefore.end())
                {
                    s_pendingBefore.erase(it);
                }
                EditHistory::Instance().PushChange({ targetKey, definition.meta.key, definition.meta.label, committedBefore, CaptureField(materialOverride, definition.field) });
            }
        };

        ImGui::PushID(sourceIndex);
        // 見出しごとに表を分ける(基本の値)
        bool isTableOpen = false;
        for (const FieldDefinition& definition : GetFieldDefinitions())
        {
            if ((definition.meta.flags & FieldFlags::kAdvanced) != 0)
            {
                continue;
            }
            if (definition.meta.section && definition.meta.section[0] != '\0')
            {
                if (isTableOpen)
                {
                    FieldWidgets::EndFieldTable();
                    isTableOpen = false;
                }
                ImGui::SeparatorText(definition.meta.section);
                // 見出しごとの表は、IDが重ならないよう先頭の値のキーで名前を付ける
                const std::string tableId = std::string("##basic_") + definition.meta.key;
                isTableOpen = FieldWidgets::BeginFieldTable(tableId.c_str());
            }
            if (isTableOpen)
            {
                drawRow(definition);
            }
        }
        if (isTableOpen)
        {
            FieldWidgets::EndFieldTable();
        }
        if (ImGui::TreeNodeEx(u8"詳細", ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            if (FieldWidgets::BeginFieldTable("##advanced"))
            {
                for (const FieldDefinition& definition : GetFieldDefinitions())
                {
                    if ((definition.meta.flags & FieldFlags::kAdvanced) != 0)
                    {
                        drawRow(definition);
                    }
                }
                FieldWidgets::EndFieldTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    std::string DescribeTextures(const Material& material)
    {
        std::string text;
        auto add = [&](bool isValid, const char* pName)
        {
            if (isValid)
            {
                text += (text.empty() ? "" : u8"・") + std::string(pName);
            }
        };
        add(material.BaseColor.valid, u8"色");
        add(material.Normal.valid, u8"法線");
        add(material.MetallicRoughness.valid, u8"金属と粗さ");
        add(material.Occlusion.valid, u8"AO");
        add(material.Emissive.valid, u8"発光");
        return text.empty() ? std::string(u8"なし") : text;
    }
}

void MaterialPanel::Draw(EditorContext& ctx)
{
    // 検索中は、検索できる値の無いパネルを隠す
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"マテリアル###Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    if (!ctx.SelectedObject)
    {
        ImGui::TextDisabled(u8"ヒエラルキーで物を選ぶと、そのモデルのマテリアルを編集できます");
        return;
    }

    auto& ecs = GameManager::Instance().GetECS();
    auto* pModelData = ecs.TryGetComponent<ModelRenderData>(ctx.SelectedObject->GetEntityID());
    if (!pModelData || !pModelData->m_spModelData || pModelData->m_filePath.empty())
    {
        ImGui::TextDisabled(u8"選んだ物にはモデルがありません");
        return;
    }
    if (!pModelData->m_spModelData->IsLoaded())
    {
        ImGui::TextDisabled(u8"モデルを読み込んでいます…");
        return;
    }
    const std::string& modelPath = pModelData->m_filePath;

    // 同じglTFのマテリアルを写したメッシュをまとめる(glTFの中の順)
    std::map<int, MaterialGroup> groups;
    for (auto& node : pModelData->m_spModelData->GetNodesRef())
    {
        for (auto& meshHandle : node.meshes)
        {
            Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
            if (!pMesh)
            {
                continue;
            }
            if (!pMesh->IsReady())
            {
                // GPUへ送る前のメッシュは、読み込んだときのマテリアルを後から写すので、まだ編集しない
                ImGui::TextDisabled(u8"モデルを読み込んでいます…");
                return;
            }
            groups[pMesh->GetMaterialRef().SourceIndex].meshes.push_back(pMesh);
        }
    }

    auto& metaManager = ModelMetaManager::Instance();
    ImGui::TextWrapped(u8"モデル: %s", modelPath.c_str());
    ImGui::TextDisabled(u8"値はモデルの.metaに保存し、同じモデルを置いた物すべてに効きます");
    const bool isDirty = metaManager.IsDirty(modelPath);
    const bool isReadOnly = metaManager.IsReadOnly(modelPath);
    ImGui::BeginDisabled(!isDirty || isReadOnly);
    if (ImGui::Button(u8"モデルの.metaに保存"))
    {
        metaManager.Save(modelPath);
    }
    ImGui::EndDisabled();
    if (isDirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(FieldWidgets::GetModifiedColor()), u8"保存していない変更があります");
    }
    if (isReadOnly)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), u8"この.metaは読めない(か新しい版の)ため、上書きしません。ログを見てください");
    }
    if (const int unmatchedCount = metaManager.CountUnmatched(modelPath); unmatchedCount > 0)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), u8".metaに、どのマテリアルにも当たらない上書きが %d 個あります(消さずに残しています。ログを見てください)", unmatchedCount);
    }

    for (const auto& [sourceIndex, group] : groups)
    {
        const Material& representative = group.meshes.front()->GetMaterialRef();
        if (sourceIndex < 0)
        {
            ImGui::TextDisabled(u8"元のマテリアルが分からないメッシュが %d 個あります(編集できません)", static_cast<int>(group.meshes.size()));
            continue;
        }
        const std::string name = representative.Name.empty() ? std::string(u8"(名前なし)") : representative.Name;
        const bool isOverridden = !metaManager.GetOverride(modelPath, sourceIndex).IsEmpty();
        const std::string label = name + u8"  (番号 " + std::to_string(sourceIndex) + u8"、メッシュ " + std::to_string(group.meshes.size()) + u8" 個)"
            + (isOverridden ? u8"  上書きあり" : "") + "###material" + std::to_string(sourceIndex);
        if (ImGui::TreeNodeEx(label.c_str(), groups.size() == 1 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
        {
            ImGui::TextDisabled(u8"テクスチャ: %s", DescribeTextures(representative).c_str());
            DrawMaterialFields(modelPath, sourceIndex, group);
            ImGui::TreePop();
        }
    }
}
