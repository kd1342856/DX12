#pragma once
#include <vector>
#include <string>
#include "../../../../Framework/Manager/Asset/MeshManager.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"
#include "../../../../Framework/Manager/Animation/OpenableAnim.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"

// 隠れ場所(押入れ)。家のモデルの "...Oshiire_Carcass"(押入れの箱)のノードから中の空間を求め、
// 同じ押入れの襖("...Oshiire_..Fusuma.._Open"。天袋は除く)が全部閉まっている間、中にいるPlayerを「隠れている」とみなす。
// GhostAIがハント中に使う(視認中に隠れられたら襖を開けて見つける / 見られずに隠れたらその辺りを巡回するだけ)。
namespace HidingSpot
{
    struct Spot
    {
        Entity entity = INVALID_ENTITY;   // 家のモデル
        const void* modelData = nullptr;  // キャッシュが古くなっていないかの確認用
        std::vector<int> fusumaAnims;     // この押入れの襖のアニメーション番号
        Math::Vector3 boundsMin;          // 中の空間(ワールドAABB)
        Math::Vector3 boundsMax;
        Math::Vector3 frontPos;           // 襖の前(ゴーストが立って襖を開ける位置)
    };

    // ノードのメッシュのワールドAABB(ロード時の姿勢)
    inline bool NodeWorldAABB(const ModelData::Node& node, const Math::Matrix& entityWorld, Math::Vector3& outMin, Math::Vector3& outMax)
    {
        bool any = false;
        Math::Matrix world = node.originalGlobalTransform * entityWorld;
        for (const auto& handle : node.meshes)
        {
            Mesh* mesh = MeshManager::Instance().Get(handle);
            if (!mesh) continue;
            DirectX::BoundingBox box;
            mesh->GetLocalAABB().Transform(box, world);
            Math::Vector3 c(box.Center), e(box.Extents);
            Math::Vector3 mn = c - e, mx = c + e;
            if (!any) { outMin = mn; outMax = mx; any = true; }
            else { outMin = Math::Vector3::Min(outMin, mn); outMax = Math::Vector3::Max(outMax, mx); }
        }
        return any;
    }

    inline std::vector<Spot>& Spots()
    {
        static std::vector<Spot> s_spots;
        return s_spots;
    }

    inline void Rebuild()
    {
        auto& ecs = GameManager::Instance().GetECS();
        auto& spots = Spots();
        spots.clear();

        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
            auto* pTrans = ecs.TryGetComponent<TransformData>(entity);
            if (!pModel || !pTrans || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;
            const auto& nodes = pModel->m_spModelData->GetNodes();
            const auto& anims = pModel->m_spModelData->GetAnimations();

            for (const auto& node : nodes)
            {
                if (!OpenableAnim::EndsWith(node.name, "Oshiire_Carcass")) continue;
                // "Oshiire_" / "W2F_Oshiire_" など、同じ押入れの部品に共通の接頭辞
                const std::string prefix = node.name.substr(0, node.name.size() - std::string("Carcass").size());

                Spot spot;
                spot.entity = entity;
                spot.modelData = pModel->m_spModelData.get();
                if (!NodeWorldAABB(node, pTrans->m_worldMatrix, spot.boundsMin, spot.boundsMax)) continue;

                for (int i = 0; i < (int)anims.size(); ++i)
                {
                    const std::string& name = anims[i].name;
                    if (OpenableAnim::StartsWith(name, prefix.c_str()) && name.find("Fusuma") != std::string::npos)
                        spot.fusumaAnims.push_back(i);
                }
                if (spot.fusumaAnims.empty()) continue;

                // 襖側(=枠"..Frame"がある側)の外に少し出た位置を、ゴーストが立つ位置にする
                Math::Vector3 center = (spot.boundsMin + spot.boundsMax) * 0.5f;
                Math::Vector3 front = center;
                for (const auto& frameNode : nodes)
                {
                    if (frameNode.name != prefix + "Frame") continue;
                    Math::Vector3 fMin, fMax;
                    if (NodeWorldAABB(frameNode, pTrans->m_worldMatrix, fMin, fMax)) front = (fMin + fMax) * 0.5f;
                    break;
                }
                Math::Vector3 out = front - center;
                out.y = 0.0f;
                if (out.LengthSquared() > 0.0001f) out.Normalize();
                spot.frontPos = Math::Vector3(front.x, spot.boundsMin.y, front.z) + out * 0.6f;
                spots.push_back(std::move(spot));
            }
        }
    }

    // キャッシュが今のシーンのものか(シーンが変わったら作り直す)
    inline bool IsCacheValid()
    {
        auto& spots = Spots();
        if (spots.empty()) return false;
        auto& ecs = GameManager::Instance().GetECS();
        for (const auto& s : spots)
        {
            auto* pModel = ecs.TryGetComponent<ModelRenderData>(s.entity);
            if (!pModel || pModel->m_spModelData.get() != s.modelData) return false;
        }
        return true;
    }

    inline const std::vector<Spot>& GetSpots()
    {
        // 押入れが無いシーンで毎回全エンティティを走査しないよう、空の時の作り直しは間引く
        static int s_retryCounter = 0;
        if (!IsCacheValid())
        {
            if (Spots().empty() && s_retryCounter-- > 0) return Spots();
            Rebuild();
            s_retryCounter = 60;
        }
        return Spots();
    }

    inline bool IsFusumaClosed(const Spot& spot)
    {
        auto& ecs = GameManager::Instance().GetECS();
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(spot.entity);
        if (!pAnim) return true; // 一度も開けていない
        for (int idx : spot.fusumaAnims)
        {
            auto it = pAnim->multiAnims.find(idx);
            if (it == pAnim->multiAnims.end()) continue;
            if (it->second.IsPlaying || it->second.ProgressTime > 0.0f) return false;
        }
        return true;
    }

    inline bool IsInside(const Spot& spot, const Math::Vector3& pos)
    {
        const float kMargin = 0.05f;
        return pos.x >= spot.boundsMin.x + kMargin && pos.x <= spot.boundsMax.x - kMargin &&
               pos.z >= spot.boundsMin.z + kMargin && pos.z <= spot.boundsMax.z - kMargin &&
               pos.y >= spot.boundsMin.y - 0.3f && pos.y <= spot.boundsMax.y;
    }

    // posが押入れの中で、その押入れの襖が全部閉まっていれば、その押入れの番号を返す(隠れていなければ-1)
    inline int FindHiding(const Math::Vector3& pos)
    {
        const auto& spots = GetSpots();
        for (int i = 0; i < (int)spots.size(); ++i)
        {
            if (IsInside(spots[i], pos) && IsFusumaClosed(spots[i])) return i;
        }
        return -1;
    }

    // 押入れの襖を全部開ける(ゴーストが隠れたPlayerを見つけた時)
    inline void OpenFusuma(const Spot& spot, float soundVolume)
    {
        auto& ecs = GameManager::Instance().GetECS();
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(spot.entity);
        if (!pModel || !pModel->m_spModelData) return;
        if (!ecs.TryGetComponent<AnimationDataComponent>(spot.entity))
            ecs.AddComponent<AnimationDataComponent>(spot.entity, AnimationDataComponent{});
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(spot.entity);
        if (!pAnim) return;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        bool played = false;
        for (int idx : spot.fusumaAnims)
        {
            auto& data = pAnim->multiAnims[idx];
            if (data.IsPlaying || data.ProgressTime > 0.0f) continue;
            AnimationManager::Instance().PlayMultiAnimation(spot.entity, idx, false, 1.0f);
            if (!played)
            {
                AudioManager::Instance().PlaySEAt(OpenableAnim::OpenSound(anims[idx].name), spot.frontPos, soundVolume, 15.0f);
                played = true;
            }
        }
        // 開いている間も襖の当たり判定が動くように(Player::TryInteractDoorと同じ)
        auto* pCollider = ecs.TryGetComponent<ColliderData>(spot.entity);
        if (pCollider && pCollider->m_isStatic) pCollider->m_isStatic = false;
    }
}
