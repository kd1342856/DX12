#include "../../../Pch.h"
#include "PropLayout.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/Manager/Scene/Scene.h"
#include "../../../Framework/Manager/Collision/CollisionShape.h"
#include "../../../Framework/Object/GameObject.h"
#include "../../../Framework/DirectX/Utility/Logger.h"
#include "../../../Framework/Manager/Animation/OpenableAnim.h"
#include <filesystem>
#include <functional>
#include <unordered_set>

namespace
{
    bool ReadFloat3(const nlohmann::json& a, float out[3])
    {
        if (!a.is_array() || a.size() < 3) return false;
        for (int i = 0; i < 3; ++i) out[i] = a[i].get<float>();
        return true;
    }

    void LogError(const std::string& msg)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "[PropLayout] " + msg);
    }
}

std::vector<PropLayout::SpawnedProp> PropLayout::Spawn(Scene& scene, const std::string& layoutPath)
{
    std::vector<SpawnedProp> spawned;

    std::ifstream in(layoutPath);
    if (!in.is_open())
    {
        LogError("layout not found: " + layoutPath);
        return spawned;
    }

    nlohmann::json layout = nlohmann::json::parse(in, nullptr, false);
    if (layout.is_discarded() || !layout.contains("assets") || !layout["assets"].is_object())
    {
        LogError("invalid layout: " + layoutPath);
        return spawned;
    }

    // 各assetのfileはlayout.jsonのあるフォルダ(Props)からの相対パス
    const std::string propsDir = std::filesystem::path(layoutPath).parent_path().generic_string();

    // シーン側で同じモデルを使っている物(照明として置いた吊り下げ照明など)は、シーンの物を正として小物には出さない
    std::unordered_set<std::string> sceneModelPaths;
    {
        auto& ecs = GameManager::Instance().GetECS();
        std::function<void(GameObject*)> collect = [&](GameObject* obj) {
            if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID())) {
                if (!pModel->m_filePath.empty()) sceneModelPaths.insert(std::filesystem::path(pModel->m_filePath).lexically_normal().generic_string());
            }
            for (auto& child : obj->GetChildren()) collect(child.get());
        };
        for (auto& obj : scene.GetGameObjects()) if (obj) collect(obj.get());
    }

    for (const auto& [assetName, asset] : layout["assets"].items())
    {
        const std::string file = asset.value("file", "");
        if (file.empty() || !asset.contains("instances")) continue;
        const std::string modelPath = propsDir + "/" + file;
        if (sceneModelPaths.count(std::filesystem::path(modelPath).lexically_normal().generic_string()))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Info, "[PropLayout] Skipped " + assetName + " (placed in the scene)");
            continue;
        }
        if (!std::filesystem::exists(modelPath))
        {
            LogError("model not found: " + modelPath + " (" + assetName + ")");
            continue;
        }

        // 当たり判定は box_fallback(アセットローカル、glTF空間)。zを反転してエンジン空間にする
        nlohmann::json colliderShape;
        bool hasCollider = false;
        if (asset.contains("physics") && asset["physics"].contains("collider"))
        {
            const auto& col = asset["physics"]["collider"];
            if (col.contains("box_fallback"))
            {
                const auto& box = col["box_fallback"];
                float center[3], half[3];
                if (ReadFloat3(box["center"], center) && ReadFloat3(box["half_extents"], half))
                {
                    colliderShape = {
                        { "ShapeId", static_cast<int>(CollisionShape::Box) },
                        { "Offset", { center[0], center[1], -center[2] } },
                        { "Tags", static_cast<uint32_t>(CollisionTags::All) },
                        { "Width", half[0] * 2.0f },
                        { "Height", half[1] * 2.0f },
                        { "Depth", half[2] * 2.0f },
                    };
                    hasCollider = true;
                }
            }
        }

        const bool hasParts = asset.contains("parts") && asset["parts"].is_array() && !asset["parts"].empty();

        for (const auto& inst : asset["instances"])
        {
            float t[3] = { 0, 0, 0 }, s[3] = { 1, 1, 1 };
            ReadFloat3(inst.value("translation", nlohmann::json()), t);
            ReadFloat3(inst.value("scale", nlohmann::json()), s);

            // glTFの回転(x,y,z,w)をz反転した空間へ: 鏡映で回転軸は(x,y,-z)、回転の向きは逆になるので(-x,-y,z,w)
            Math::Quaternion q = Math::Quaternion::Identity;
            const auto& r = inst.value("rotation", nlohmann::json());
            if (r.is_array() && r.size() >= 4)
            {
                q = Math::Quaternion(-r[0].get<float>(), -r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
                q.Normalize();
            }
            // TransformDataはCreateFromYawPitchRoll(y,x,z)で組むので(pitch,yaw,roll)=(x,y,z)のオイラー角にする
            const Math::Vector3 euler = Math::Matrix::CreateFromQuaternion(q).ToEuler();

            nlohmann::json objJ;
            objJ["Name"] = inst.value("name", assetName);
            objJ["IsActive"] = true;
            objJ["Components"] = nlohmann::json::array({
                {
                    { "Type", "TransformData" },
                    { "Position", { t[0], t[1], -t[2] } },
                    { "Rotation", { euler.x, euler.y, euler.z } },
                    { "Scale", { s[0], s[1], s[2] } },
                },
                {
                    { "Type", "ModelRenderData" },
                    { "FilePath", modelPath },
                    { "ModelType", 0 },
                },
            });
            if (hasParts)
            {
                // 扉/引き出しのアニメーション("*_Open")。OpenableAnim::IsOpenableでプレイヤーが開け閉めできる
                objJ["Components"].push_back({
                    { "Type", "AnimationDataComponent" },
                    { "AnimationIndex", -1 },
                    { "IsPlaying", false },
                    { "IsLoop", false },
                    { "ProgressTime", 0.0f },
                    { "Speed", 1.0f },
                });
            }
            objJ["Children"] = nlohmann::json::array();
            if (hasCollider)
            {
                objJ["Children"].push_back({
                    { "Name", "Collider" },
                    { "IsActive", true },
                    { "Children", nlohmann::json::array() },
                    { "Components", nlohmann::json::array({
                        {
                            { "Type", "TransformData" },
                            { "Position", { 0.0f, 0.0f, 0.0f } },
                            { "Rotation", { 0.0f, 0.0f, 0.0f } },
                            { "Scale", { 1.0f, 1.0f, 1.0f } },
                        },
                        {
                            { "Type", "ColliderData" },
                            { "IsStatic", true },
                            { "Shapes", nlohmann::json::array({ colliderShape }) },
                        },
                    }) },
                });
            }

            const size_t prevCount = scene.GetGameObjects().size();
            scene.DeserializeGameObject(objJ, nullptr);
            if (scene.GetGameObjects().size() <= prevCount)
            {
                LogError("failed to create " + objJ["Name"].get<std::string>());
                continue;
            }

            SpawnedProp prop;
            prop.obj = scene.GetGameObjects().back();
            // layout.jsonが配置の正なので、シーン保存には含めない
            prop.obj->SetDynamic(true);
            for (const auto& child : prop.obj->GetChildren()) child->SetDynamic(true);
            if (inst.contains("initial_open") && inst["initial_open"].is_array())
            {
                for (const auto& n : inst["initial_open"]) prop.initialOpenNodes.push_back(n.get<std::string>());
            }
            spawned.push_back(std::move(prop));
        }
    }

    Logger::Instance().AddLog(Logger::LogLevel::Info, "[PropLayout] Spawned " + std::to_string(spawned.size()) + " props from " + layoutPath);
    return spawned;
}

void PropLayout::ApplyInitialOpen(const std::vector<SpawnedProp>& props)
{
    auto& ecs = GameManager::Instance().GetECS();
    for (const auto& prop : props)
    {
        if (prop.initialOpenNodes.empty() || !prop.obj) continue;
        const Entity e = prop.obj->GetEntityID();
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(e);
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(e);
        if (!pModel || !pAnim || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded())
        {
            LogError("cannot apply initial_open (model not loaded): " + prop.obj->GetName());
            continue;
        }

        const auto& anims = pModel->m_spModelData->GetAnimations();
        for (const auto& node : prop.initialOpenNodes)
        {
            int animIdx = -1;
            for (int i = 0; i < (int)anims.size(); ++i)
            {
                if (anims[i].name == node + "_Open" ||
                    (!anims[i].channels.empty() && anims[i].channels[0].nodeName == node))
                {
                    animIdx = i;
                    break;
                }
            }
            if (animIdx < 0)
            {
                LogError("initial_open: no animation for node " + node + " in " + prop.obj->GetName());
                continue;
            }

            // 開き終わる直前から再生する。AnimationSystemは開き終わり(OpenableAnim::FullOpenTime)で止めるので、
            // 次のフレームで開いた姿勢のまま停止し、プレイヤーが操作すると閉まる(Player::TryInteractDoorの判定と同じ状態)
            auto& data = pAnim->multiAnims[animIdx];
            data.AnimationIndex = animIdx;
            data.IsPlaying = true;
            data.IsLoop = false;
            data.Speed = 1.0f;
            data.ProgressTime = std::max(0.0f, OpenableAnim::FullOpenTime(anims[animIdx].name, anims[animIdx].duration) - 0.002f);
        }
    }
}
