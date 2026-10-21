#pragma once
#include <vector>
#include <string>
#include <fstream>
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
        std::vector<Math::Vector3> fusumaCenters; // 各襖(fusumaAnimsと同じ順)の閉まっている時の中心
        Math::Vector3 boundsMin;          // 中の空間(ワールドAABB)
        Math::Vector3 boundsMax;
        Math::Vector3 frontPos;           // 襖の前(ゴーストが立って襖を開ける位置。正面の中央)
        Math::Vector3 outDir;             // 押入れの中から襖の外へ向かう水平方向
        float frontHalfWidth = 0.0f;      // 正面に沿った立ち位置の範囲(frontPosからの距離)
    };

    // glTFファイルを直接読んで、名前がsuffixで終わるノードのメッシュAABB(モデル空間)を集める。
    // 家のモデルはStaticで読み込まれ、AssimpのaiProcess_OptimizeGraphでアニメーションしないノード
    // (押入れの箱"..Oshiire_Carcass"や枠"..Frame")が統合されて名前ごと消えるため、ModelDataのノードからは探せない。
    // 座標はAssimpのaiProcess_ConvertToLeftHandedに合わせてZを反転した左手系にする。
    struct NamedAABB
    {
        std::string name;
        Math::Vector3 min, max;
    };
    inline std::vector<NamedAABB> LoadNodeAABBsFromGltf(const std::string& path, const std::vector<std::string>& suffixes)
    {
        std::vector<NamedAABB> result;
        std::ifstream file(path);
        if (!file) return result;
        nlohmann::json gltf = nlohmann::json::parse(file, nullptr, false);
        if (gltf.is_discarded() || !gltf.contains("nodes")) return result;

        const auto& nodes = gltf["nodes"];
        std::vector<int> parent(nodes.size(), -1);
        for (int i = 0; i < (int)nodes.size(); ++i)
        {
            if (!nodes[i].contains("children")) continue;
            for (int c : nodes[i]["children"]) if (c >= 0 && c < (int)nodes.size()) parent[c] = i;
        }

        auto localMatrix = [&](const nlohmann::json& n) {
            if (n.contains("matrix"))
            {
                // glTFは列優先(列ベクトル用)。そのまま行優先で読むと行ベクトル用(DirectX)の行列になる
                Math::Matrix m;
                float* p = &m._11;
                for (int k = 0; k < 16; ++k) p[k] = n["matrix"][k].get<float>();
                return m;
            }
            Math::Vector3 t(0, 0, 0), sc(1, 1, 1);
            Math::Quaternion q(0, 0, 0, 1);
            if (n.contains("translation")) t = Math::Vector3(n["translation"][0], n["translation"][1], n["translation"][2]);
            if (n.contains("scale"))       sc = Math::Vector3(n["scale"][0], n["scale"][1], n["scale"][2]);
            if (n.contains("rotation"))    q = Math::Quaternion(n["rotation"][0], n["rotation"][1], n["rotation"][2], n["rotation"][3]);
            return Math::Matrix::CreateScale(sc) * Math::Matrix::CreateFromQuaternion(q) * Math::Matrix::CreateTranslation(t);
        };

        for (int i = 0; i < (int)nodes.size(); ++i)
        {
            const std::string name = nodes[i].value("name", "");
            bool match = false;
            for (const auto& sfx : suffixes) if (OpenableAnim::EndsWith(name, sfx.c_str())) { match = true; break; }
            if (!match || !nodes[i].contains("mesh")) continue;

            Math::Matrix world = localMatrix(nodes[i]);
            for (int p = parent[i]; p >= 0; p = parent[p]) world = world * localMatrix(nodes[p]);

            bool any = false;
            Math::Vector3 mn, mx;
            const auto& mesh = gltf["meshes"][nodes[i]["mesh"].get<int>()];
            for (const auto& prim : mesh["primitives"])
            {
                if (!prim.contains("attributes") || !prim["attributes"].contains("POSITION")) continue;
                const auto& acc = gltf["accessors"][prim["attributes"]["POSITION"].get<int>()];
                if (!acc.contains("min") || !acc.contains("max")) continue;
                DirectX::BoundingBox local;
                Math::Vector3 lo(acc["min"][0], acc["min"][1], acc["min"][2]);
                Math::Vector3 hi(acc["max"][0], acc["max"][1], acc["max"][2]);
                DirectX::BoundingBox::CreateFromPoints(local, lo, hi);
                DirectX::BoundingBox box;
                local.Transform(box, world);
                Math::Vector3 c(box.Center), e(box.Extents);
                if (!any) { mn = c - e; mx = c + e; any = true; }
                else { mn = Math::Vector3::Min(mn, c - e); mx = Math::Vector3::Max(mx, c + e); }
            }
            if (!any) continue;
            // 右手系 -> 左手系(Z反転)
            float zMin = -mx.z, zMax = -mn.z;
            mn.z = zMin; mx.z = zMax;
            result.push_back({ name, mn, mx });
        }
        return result;
    }

    // モデル空間のAABBをエンティティのワールド行列で変換する
    inline void TransformAABB(const Math::Vector3& mn, const Math::Vector3& mx, const Math::Matrix& world, Math::Vector3& outMin, Math::Vector3& outMax)
    {
        DirectX::BoundingBox local, box;
        DirectX::BoundingBox::CreateFromPoints(local, mn, mx);
        local.Transform(box, world);
        Math::Vector3 c(box.Center), e(box.Extents);
        outMin = c - e;
        outMax = c + e;
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
            const auto& anims = pModel->m_spModelData->GetAnimations();

            // 襖のアニメーションを持つモデルだけglTFを読む(全モデルのファイルを読まないように)
            bool hasFusuma = false;
            for (const auto& a : anims) if (a.name.find("Oshiire") != std::string::npos && a.name.find("Fusuma") != std::string::npos) { hasFusuma = true; break; }
            if (!hasFusuma) continue;

            const auto boxes = LoadNodeAABBsFromGltf(pModel->m_filePath, { "Oshiire_Carcass", "Oshiire_Frame", "_Fusuma_L", "_Fusuma_R" });
            for (const auto& carcass : boxes)
            {
                if (!OpenableAnim::EndsWith(carcass.name, "Oshiire_Carcass")) continue;
                // "Oshiire_" / "W2F_Oshiire_" など、同じ押入れの部品に共通の接頭辞
                const std::string prefix = carcass.name.substr(0, carcass.name.size() - std::string("Carcass").size());

                Spot spot;
                spot.entity = entity;
                spot.modelData = pModel->m_spModelData.get();
                TransformAABB(carcass.min, carcass.max, pTrans->m_worldMatrix, spot.boundsMin, spot.boundsMax);

                for (int i = 0; i < (int)anims.size(); ++i)
                {
                    const std::string& name = anims[i].name;
                    if (OpenableAnim::StartsWith(name, prefix.c_str()) && name.find("Fusuma") != std::string::npos)
                        spot.fusumaAnims.push_back(i);
                }
                if (spot.fusumaAnims.empty()) continue;

                // 襖側(=枠"..Frame"がある側)が正面。ゴーストは正面の少し外に立って襖を開ける
                Math::Vector3 center = (spot.boundsMin + spot.boundsMax) * 0.5f;
                Math::Vector3 front = center;
                for (const auto& frame : boxes)
                {
                    if (frame.name != prefix + "Frame") continue;
                    Math::Vector3 fMin, fMax;
                    TransformAABB(frame.min, frame.max, pTrans->m_worldMatrix, fMin, fMax);
                    front = (fMin + fMax) * 0.5f;
                    break;
                }
                Math::Vector3 out = front - center;
                out.y = 0.0f;
                if (out.LengthSquared() > 0.0001f) out.Normalize();
                else out = Math::Vector3(1, 0, 0);
                spot.outDir = out;
                spot.frontPos = Math::Vector3(front.x, spot.boundsMin.y, front.z) + out * 0.6f;
                // 正面に沿った方向の半分の長さ(押入れの端から少し内側まで)
                Math::Vector3 tangent(-out.z, 0.0f, out.x);
                Math::Vector3 ext = (spot.boundsMax - spot.boundsMin) * 0.5f;
                spot.frontHalfWidth = (std::max)(0.0f, fabsf(tangent.x) * ext.x + fabsf(tangent.z) * ext.z - 0.3f);

                // 各襖の閉まっている時の中心(アニメーション名から"_Open"を除いたものが襖のノード名)
                for (int idx : spot.fusumaAnims)
                {
                    std::string nodeName = anims[idx].name;
                    if (OpenableAnim::EndsWith(nodeName, "_Open")) nodeName.resize(nodeName.size() - 5);
                    Math::Vector3 c = spot.frontPos;
                    for (const auto& b : boxes)
                    {
                        if (b.name != nodeName) continue;
                        Math::Vector3 fMin, fMax;
                        TransformAABB(b.min, b.max, pTrans->m_worldMatrix, fMin, fMax);
                        c = (fMin + fMax) * 0.5f;
                        break;
                    }
                    spot.fusumaCenters.push_back(c);
                }
                spots.push_back(std::move(spot));
            }
        }
    }

    // 押入れの正面(襖の外)で、posに一番近い立ち位置。ゴーストが襖を開けて覗き込む位置
    inline Math::Vector3 GetFrontPosNear(const Spot& spot, const Math::Vector3& pos)
    {
        Math::Vector3 tangent(-spot.outDir.z, 0.0f, spot.outDir.x);
        Math::Vector3 d = pos - spot.frontPos;
        float t = d.x * tangent.x + d.z * tangent.z;
        t = (std::max)(-spot.frontHalfWidth, (std::min)(spot.frontHalfWidth, t));
        return spot.frontPos + tangent * t;
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

    // 押入れの襖を開ける(ゴーストが隠れたPlayerを見つけた時)。
    // nearPosを渡すと、閉まっている時にnearPosに一番近い襖を1枚だけ開ける。引き違いの襖は左を開けると右の位置へ、
    // 右を開けると左の位置へ滑るので、左右を同時に開けると入れ替わるだけで入口がまた塞がってしまう。
    inline void OpenFusuma(const Spot& spot, float soundVolume, const Math::Vector3* nearPos = nullptr)
    {
        auto& ecs = GameManager::Instance().GetECS();
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(spot.entity);
        if (!pModel || !pModel->m_spModelData) return;
        if (!ecs.TryGetComponent<AnimationDataComponent>(spot.entity))
            ecs.AddComponent<AnimationDataComponent>(spot.entity, AnimationDataComponent{});
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(spot.entity);
        if (!pAnim) return;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        int onlyIdx = -1;
        if (nearPos && spot.fusumaCenters.size() == spot.fusumaAnims.size())
        {
            float best = FLT_MAX;
            for (size_t k = 0; k < spot.fusumaAnims.size(); ++k)
            {
                Math::Vector3 d = spot.fusumaCenters[k] - *nearPos;
                d.y = 0.0f;
                if (d.LengthSquared() < best) { best = d.LengthSquared(); onlyIdx = spot.fusumaAnims[k]; }
            }
        }
        bool played = false;
        for (int idx : spot.fusumaAnims)
        {
            if (onlyIdx >= 0 && idx != onlyIdx) continue;
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
