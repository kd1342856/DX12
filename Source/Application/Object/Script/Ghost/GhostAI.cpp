#include "../../../../Pch.h"
#include "GhostAI.h"
#include "../System/StimulusSource.h"
#include "../System/DoorLock.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../System/GameSequence.h"
#include "../Player/Player.h"
#include "../Item/PickupItem.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../../Framework/Manager/NavMesh/NavMeshManager.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../../Framework/Manager/Particle/ParticleManager.h"
#include <functional>
#include <unordered_set>

REGISTER_COMPONENT(GhostAI);

// Log Windowはログ本文をSJISとして変換して表示するため、UTF-8の日本語は文字化けする - ローマ字で出す
static const char* GetGhostTypeName(GhostAI::GhostType type)
{
    switch (type)
    {
    case GhostAI::GhostType::Kageonna:
        return "Kageonna (Light)";
    case GhostAI::GhostType::Yanari:
        return "Yanari (Sound)";
    default:
        return "Jibakurei (Normal)";
    }
}

void GhostAI::Awake()
{
}

void GhostAI::Start()
{
    m_changeDirTimer = 0.0f;

    // AnimationDataComponentは自分自身ではなく子の"Model"オブジェクトに付いているため、
    // 子階層を探してキャッシュしておく(毎フレーム探索しないよう)
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> findAnimEntity = [&](GameObject* obj)
    {
        if (m_animEntity != INVALID_ENTITY)
        {
            return;
        }
        if (ecs.TryGetComponent<AnimationDataComponent>(obj->GetEntityID()))
        {
            m_animEntity = obj->GetEntityID();
            return;
        }
        for (auto& child : obj->GetChildren())
        {
            findAnimEntity(child.get());
        }
    };
    if (m_pGameObject)
    {
        findAnimEntity(m_pGameObject);
    }

    m_playerEntity = FindPlayerEntity();
    EnsureHouseBounds();
    EnsureStairsRoom();

    if (m_randomizeType)
    {
        // 家鳴(Yanari)はまだ未実装のため、現状は地縛霊と影女から抽選する
        m_ghostType = (Random::Instance().Range(0, 1) == 0) ? GhostType::Jibakurei : GhostType::Kageonna;
    }
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Ghost type: %s", GetGhostTypeName(m_ghostType));
    Logger::Instance().SetPinnedLine("Ghost Type", GetGhostTypeName(m_ghostType));

    m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
    m_huntTriggerTimer = Random::Instance().Range(m_huntTriggerIntervalMin, m_huntTriggerIntervalMax);
    m_tellTimer = Random::Instance().Range(m_tellIntervalMin, m_tellIntervalMax);
    SetState(GhostAI::State::Wander);
}

Entity GhostAI::FindPlayerEntity()
{
    auto scene = Editor::GetScene();
    if (!scene)
    {
        return INVALID_ENTITY;
    }

    Entity found = INVALID_ENTITY;
    std::function<void(const std::shared_ptr<GameObject>&)> findPlayer = [&](const std::shared_ptr<GameObject>& obj)
    {
        if (found != INVALID_ENTITY)
        {
            return;
        }
        if (obj->GetName() == "Player")
        {
            found = obj->GetEntityID();
            return;
        }
        for (const auto& child : obj->GetChildren())
        {
            findPlayer(child);
        }
    };
    for (auto const& obj : scene->GetGameObjects())
    {
        findPlayer(obj);
    }
    return found;
}

void GhostAI::EnsureHouseBounds()
{
    if (m_houseBoundsValid)
    {
        return;
    }

    auto gs = GameSequence::GetInstance();
    if (!gs)
    {
        return;
    }

    const auto& rooms = gs->GetRooms();
    if (rooms.empty())
    {
        return;
    }

    Math::Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
    Math::Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (RoomArea* room : rooms)
    {
        if (!room)
        {
            continue;
        }
        mn = Math::Vector3::Min(mn, room->m_min);
        mx = Math::Vector3::Max(mx, room->m_max);
    }

    // 部屋同士の境界(廊下など)も含まれるように少し余裕を持たせておく
    mn.x -= m_houseBoundsMargin;
    mn.z -= m_houseBoundsMargin;
    mx.x += m_houseBoundsMargin;
    mx.z += m_houseBoundsMargin;

    m_houseBoundsMin = mn;
    m_houseBoundsMax = mx;
    m_houseBoundsValid = true;
}

// Looked up lazily (retried every call until it succeeds) rather than only once in Start(), because
// GameSequence::Start() populates its room list in its own Start() and there's no guaranteed
// ordering between different NativeScript components' Start() calls - if GhostAI::Start() happened
// to run first, GameSequence::GetRooms() would still be empty and m_stairsRoom would silently stay
// null forever, quietly disabling every stairs-specific fix (waypoint routing, pass-through) with
// no visible error.
void GhostAI::EnsureStairsRoom()
{
    if (m_stairsRoom)
    {
        return;
    }
    auto gs = GameSequence::GetInstance();
    if (!gs)
    {
        return;
    }
    for (RoomArea* room : gs->GetRooms())
    {
        if (room && room->m_isStairs)
        {
            m_stairsRoom = room;
            Logger::Instance().AddLog(Logger::LogLevel::Info,
                                      "[GhostAI] EnsureStairsRoom: found stairs at (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
                                      room->m_min.x, room->m_min.y, room->m_min.z, room->m_max.x, room->m_max.y, room->m_max.z);
            return;
        }
    }
}

Math::Vector3 GhostAI::ClampToHouseBounds(const Math::Vector3& pos) const
{
    if (!m_houseBoundsValid)
    {
        return pos;
    }

    Math::Vector3 result = pos;
    result.x = std::clamp(result.x, m_houseBoundsMin.x, m_houseBoundsMax.x);
    result.z = std::clamp(result.z, m_houseBoundsMin.z, m_houseBoundsMax.z);
    return result;
}

void GhostAI::SetModelVisible(bool visible)
{
    if (m_animEntity == INVALID_ENTITY)
    {
        return;
    }
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_animEntity))
    {
        pModel->m_isVisible = visible;
    }
}

// When the real target is on a different floor, NavMesh's own long-range pathfinding has to funnel
// through the stairs' bridge polygon - a connector built by force-linking (see BuildManualNavMesh's
// forceLink comments) rather than sharing a real welded edge, since a staircase's footprint sits
// entirely inside whichever room it starts/ends in instead of touching it along a boundary. The
// straight-path waypoint Detour derives from that synthetic portal isn't spatially meaningful, so
// it can send the ghost toward a nonsensical point instead of the stairs' actual location - looking
// like it just paces back and forth on its own floor, directly above/below the real target, instead
// of ever heading for the stairs. Sidestep that entirely: when crossing floors, explicitly target
// the stairs' own (spatially correct) position first. Movement never uses the Y component (see
// AdvanceAlongPath/GetMoveDirection), and the ghost's actual height comes from gravity + the ground
// raycast following the real stair-step geometry, so once its actual current Y catches up to the
// target's Y (within m_floorHeightThreshold), it has physically arrived on the destination floor and
// GetEffectiveMoveTarget starts returning the real target again.
Math::Vector3 GhostAI::GetEffectiveMoveTarget(const Math::Vector3& currentPos, const Math::Vector3& finalTarget)
{
    if (!m_stairsRoom)
    {
        return finalTarget;
    }

    // Which landing this crossing exits onto (whichever end sits closer to the final destination's
    // floor). During Hunt, finalTarget is the player's LIVE position, which can itself cross floors
    // while the ghost is already mid-crossing (the player runs back upstairs while the ghost is
    // still descending toward them). Re-deriving this from finalTarget every frame let that flip
    // the exit landing mid-flight - the exit check below would then compare the ghost's current
    // (still near the OLD landing) height against the NEW landing, read that as "already arrived",
    // release the latch, and the entry check would immediately re-trigger a crossing back the other
    // way from essentially the same spot, over and over. Freeze it once for the whole crossing
    // instead, the moment the entry check below actually commits to one (see m_isCrossingFloors).
    float bottomDist = fabsf(finalTarget.y - m_stairsRoom->m_min.y);
    float topDist = fabsf(finalTarget.y - m_stairsRoom->m_max.y);
    float exitLandingY = m_isCrossingFloors        ? m_frozenExitLandingY
                         : (bottomDist <= topDist) ? m_stairsRoom->m_min.y
                                                   : m_stairsRoom->m_max.y;

    // Hysteresis: once committed to routing via the stairs, keep doing so until the Y gap closes
    // to well under the threshold, and vice versa - otherwise a gap sitting right at
    // m_floorHeightThreshold (gravity/ground-raycast jitter while on the stairs, or ordinary
    // movement that happens to pass near that height difference) flips the target every recompute,
    // which looks like starting forward and immediately reversing, repeatedly.
    //
    // The exit check compares against the EXIT LANDING's height, not finalTarget.y directly: a room
    // like Hallway2F spans a wide Y range of its own (it isn't flat), so a destination point deep
    // inside it can sit well above the landing right at the top of the stairs even though the ghost
    // has already fully completed the climb and is standing on the correct floor. Comparing to
    // finalTarget.y there kept this stuck "crossing" indefinitely: it would step just past the
    // stairs' own bounds, MoveToward would route back toward the stairs' synthetic bridge portal
    // (see BuildManualNavMesh's comments) since the real destination is still far off across the
    // room, re-entering the stairs' padded area and flipping back into crossing mode - forever.
    // m_recentlyExitedStairs bridges a mismatch between the two branches below: release compares
    // against the stairs' own real landing height (exitLandingY), but re-entry compares against the
    // wander target's Y, which is a room's volumetric GetCenter() - typically ~2m above its actual
    // floor. Right after releasing near the true landing, current height can still look more than
    // m_floorHeightThreshold away from that approximate target center, which would satisfy the entry
    // condition again on the very next frame and re-latch indefinitely while standing at the bottom of
    // the stairs. Block re-entry until position has actually left the stairs' padded area once.
    if (m_recentlyExitedStairs && !IsInStairsArea(currentPos, m_stairsNavExitPadding))
    {
        m_recentlyExitedStairs = false;
    }
    if (m_isCrossingFloors)
    {
        float exitGap = fabsf(currentPos.y - exitLandingY);
        if (exitGap <= m_floorHeightThreshold * 0.5f)
        {
            m_isCrossingFloors = false;
            m_recentlyExitedStairs = true;
        }
    }
    else if (!m_recentlyExitedStairs)
    {
        // Whether crossing is needed at all is decided by which side of the stairs' own midpoint
        // height each point falls on, not by the raw Y difference between them: finalTarget.y is a
        // room's volumetric GetCenter(), which for any room taller than 2x m_floorHeightThreshold
        // (JPRoomF2 spans 4m) sits more than the threshold above that room's own floor - so ordinary
        // same-floor walking toward it kept spuriously reading as "different floor, needs stairs"
        // even nowhere near the stairs. Comparing against the stairs' midpoint instead classifies
        // any point as belonging to the lower or upper floor regardless of within-room height
        // variance, so it only fires for an actual floor change.
        float floorMidY = (m_stairsRoom->m_min.y + m_stairsRoom->m_max.y) * 0.5f;
        bool currentIsUpper = currentPos.y > floorMidY;
        bool targetIsUpper = finalTarget.y > floorMidY;
        if (currentIsUpper != targetIsUpper)
        {
            m_isCrossingFloors = true;
            m_frozenExitLandingY = exitLandingY;
        }
    }
    if (!m_isCrossingFloors)
    {
        return finalTarget;
    }

    // GetCenter() sits at the Y midpoint between the stairs' bottom and top landing, which is
    // roughly equidistant from both - findNearestPoly can then snap to either one essentially at
    // random (and inconsistently frame to frame). Break the tie explicitly by aiming for whichever
    // landing is closer to the FINAL destination's floor (the exit side), not the ghost's own
    // current floor (the entry side): picking the entry landing seemed reasonable at first, but
    // it's self-defeating - once the ghost actually arrives there, its current Y *is* that
    // landing's Y, so this would keep re-selecting the same already-reached point forever with
    // nothing to walk toward next, which looked like stopping dead in the middle of the stairs.
    // Aiming for the exit landing instead gives it a reason to walk the entire staircase in one go.
    // 渡る時は、まず今いる階の階段の入口までNavMeshで歩かせる(昇り降り自体は入口に着いてから
    // StartStairsCrossing/GetStairsCrossingMoveが階段に沿って行う)。以前は階段の中心(出口の高さ)を
    // 目指させていたため、階段の周りのどこからでも昇り降りが始まり、階段の横から天井をすり抜けていた。
    return GetStairsEntryPoint(exitLandingY);
}

Math::Vector3 GhostAI::GetStairsEntryPoint(float exitLandingY) const
{
    if (!m_stairsRoom)
    {
        return Math::Vector3::Zero;
    }
    float centerX = (m_stairsRoom->m_min.x + m_stairsRoom->m_max.x) * 0.5f;
    // 下端(低い方の床)はmin.z側、上端(高い方の床)はmax.z側
    bool goingUp = fabsf(exitLandingY - m_stairsRoom->m_max.y) < fabsf(exitLandingY - m_stairsRoom->m_min.y);
    if (goingUp)
    {
        return Math::Vector3(centerX, m_stairsRoom->m_min.y, m_stairsRoom->m_min.z - m_stairsEntryOffset);
    }
    return Math::Vector3(centerX, m_stairsRoom->m_max.y, m_stairsRoom->m_max.z + m_stairsEntryOffset);
}

bool GhostAI::IsInStairsArea(const Math::Vector3& pos, float padOverride) const
{
    if (!m_stairsRoom)
    {
        return false;
    }
    const float pad = (padOverride >= 0.0f) ? padOverride : m_stairsPassThroughPadding;
    return pos.x >= m_stairsRoom->m_min.x - pad && pos.x <= m_stairsRoom->m_max.x + pad &&
           pos.z >= m_stairsRoom->m_min.z - pad && pos.z <= m_stairsRoom->m_max.z + pad;
}

// Called once, right when the stairs-crossing latch engages, to fix the start/end points and
// duration of the crossing. See GetStairsCrossingMove's comment for why the crossing no longer
// derives its direction from cTrans.m_position every frame.
void GhostAI::StartStairsCrossing(const Math::Vector3& currentPos, const Math::Vector3& finalTarget, float speed)
{
    if (!m_stairsRoom)
    {
        return;
    }

    const float kOvershoot = m_stairsNavExitPadding + 0.1f; // must clear IsInStairsArea's nav-exit pad
    // 上りか下りかは、GetEffectiveMoveTargetが渡ると決めた時に記録した出口の高さ(m_frozenExitLandingY)で
    // 決める。以前はここで「今の」finalTargetから決め直していたため、ハント中に目的地(プレイヤー位置・
    // 見失った地点など)が渡ると決めた後で反対の階に変わると、今いる階の出口へ向かってしまい、
    // 着いても記録した出口の高さには届かないので昇り降り中の状態が解除されず、出口でずっと止まっていた。
    (void)finalTarget;
    bool towardBottom = fabsf(m_frozenExitLandingY - m_stairsRoom->m_min.y) <= fabsf(m_frozenExitLandingY - m_stairsRoom->m_max.y);
    float centerX = (m_stairsRoom->m_min.x + m_stairsRoom->m_max.x) * 0.5f;
    const float minY = m_stairsRoom->m_min.y, maxY = m_stairsRoom->m_max.y;
    const float minZ = m_stairsRoom->m_min.z, maxZ = m_stairsRoom->m_max.z;

    // 入口付近の今の位置 -> 今いる階側の階段の端 -> 反対側の端 -> 出口(ナビ用の余白の外)。
    // 最初の区間は今いる階の高さのまま水平に、階段区間だけで高さが変わる。
    m_stairsCrossPoints.clear();
    m_stairsCrossPoints.push_back(currentPos);
    if (towardBottom)
    {
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, maxY, maxZ));
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, minY, minZ));
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, minY, minZ - kOvershoot));
    }
    else
    {
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, minY, minZ));
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, maxY, maxZ));
        m_stairsCrossPoints.push_back(Math::Vector3(centerX, maxY, maxZ + kOvershoot));
    }
    m_stairsCrossPoints[1].y = currentPos.y; // 最初の区間は高さを変えない(床の上を歩いて階段の端へ)

    float length = 0.0f;
    for (size_t i = 1; i < m_stairsCrossPoints.size(); ++i)
    {
        length += Math::Vector3::Distance(m_stairsCrossPoints[i - 1], m_stairsCrossPoints[i]);
    }
    m_stairsCrossSpeed = (speed > 0.0001f) ? speed : 1.0f;
    m_stairsCrossElapsed = 0.0f;
    m_stairsCrossDuration = length / m_stairsCrossSpeed;

    const Math::Vector3& end = m_stairsCrossPoints.back();
    Logger::Instance().AddLog(Logger::LogLevel::Info,
                              "[GhostAI] StartStairsCrossing: start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f) duration=%.2f",
                              currentPos.x, currentPos.y, currentPos.z, end.x, end.y, end.z, m_stairsCrossDuration);
}

// While inside the stairwell, drive movement directly instead of going through NavMesh at all: the
// bridge polygon there is a synthetic force-linked portal (see BuildManualNavMesh's comments), not
// real welded geometry, and GetEffectiveMoveTarget's landing selection can end up re-targeting a
// point the ghost has already reached with nothing left to walk toward, stalling it partway up or
// down the stairs.
//
// This used to recompute a direction from cTrans.m_position every call (steer toward a fixed target
// point), but that kept reversing direction mid-crossing for reasons that traced to neither collision
// (pass-through was confirmed active throughout via diagnostic logging) nor MoveToward stealing
// control back (still happened even after latching stairs-crossing mode so MoveToward couldn't run at
// all) - something about re-deriving direction from position itself was unreliable here. Interpolating
// between two points fixed once at the start of the crossing, purely as a function of elapsed time,
// removes position from the calculation entirely: there is nothing left that can make it go backward.
Math::Vector3 GhostAI::GetStairsCrossingMove(float deltaTime)
{
    m_stairsCrossElapsed += deltaTime;
    if (m_stairsCrossPoints.empty())
    {
        return Math::Vector3::Zero;
    }

    // 経過時間 x 速度 だけ折れ線に沿って進んだ位置(位置ではなく時間だけで決まるので逆戻りしない)
    float distLeft = m_stairsCrossElapsed * m_stairsCrossSpeed;
    for (size_t i = 1; i < m_stairsCrossPoints.size(); ++i)
    {
        const Math::Vector3& a = m_stairsCrossPoints[i - 1];
        const Math::Vector3& b = m_stairsCrossPoints[i];
        float segLen = Math::Vector3::Distance(a, b);
        if (distLeft <= segLen && segLen > 0.0001f)
        {
            return Math::Vector3::Lerp(a, b, distLeft / segLen);
        }
        distLeft -= segLen;
    }
    return m_stairsCrossPoints.back();
}

void GhostAI::SetDoorPassThrough(bool enabled)
{
    // The capsule ColliderData lives on a child "Collider" GameObject, not on the Ghost root
    // entity itself (mirrors how m_animEntity is found for AnimationDataComponent in Start()).
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> apply = [&](GameObject* obj)
    {
        if (auto* pCollider = ecs.TryGetComponent<ColliderData>(obj->GetEntityID()))
        {
            for (auto& shape : pCollider->m_shapes)
            {
                if (shape)
                {
                    shape->m_isTrigger = enabled;
                }
            }
        }
        for (auto& child : obj->GetChildren())
        {
            apply(child.get());
        }
    };
    if (m_pGameObject)
    {
        apply(m_pGameObject);
    }
}

void GhostAI::SetState(GhostAI::State state)
{
    // Toggle the fog target density when entering/leaving Hunt (ShaderManager smoothly
    // interpolates the actual density toward it, see ShaderManager::UpdateFogCB).
    if (m_currentState != State::Hunt && state == State::Hunt)
    {
        ShaderManager::Instance().SetHuntActive(true);
        LockEntranceDoor();
    }
    else if (m_currentState == State::Hunt && state != State::Hunt)
    {
        ShaderManager::Instance().SetHuntActive(false);
        // ハント終了: 玄関の施錠を解き、家中の電気を消す
        DoorLock::UnlockAll();
        TurnOffAllLights();
    }

    // 誘導を抜けたら発生源を手放し、同じ発生源へすぐ戻らないようクールダウンを始める
    if (m_currentState == State::Lured && state != State::Lured)
    {
        m_lureCooldownTimer = m_lureCooldown;
    }
    if (state != State::Lured)
    {
        m_lureSource = nullptr;
        m_lureArrived = false;
    }

    m_currentState = state;

    // 基本的に姿は非表示。実体化するのはハント中(とそこからのスタン・死亡)だけ。
    // Luredで光源に張り付いている間も姿は見せない(実体化はハントとPlayerを脅かす演出に限る方針)。
    switch (state)
    {
    case GhostAI::State::Wander:
    case GhostAI::State::HouseWander:
    case GhostAI::State::Idle:
    case GhostAI::State::Lured:
        SetModelVisible(false);
        break;
    case GhostAI::State::Hunt:
    case GhostAI::State::Stun:
    case GhostAI::State::Dead:
        SetModelVisible(true);
        break;
    }

    if (m_animEntity == INVALID_ENTITY)
    {
        return; // アニメーション対象が見つからない場合は何もしない
    }

    switch (state)
    {
    case GhostAI::State::Idle:
    case GhostAI::State::Stun:
        AnimationManager::Instance().PlayAnimation(m_animEntity, m_animIdle, true);
        break;
    case GhostAI::State::Wander:
    case GhostAI::State::HouseWander:
    case GhostAI::State::Lured:
        AnimationManager::Instance().PlayAnimation(m_animEntity, m_animWander, true);
        break;
    case GhostAI::State::Hunt:
        AnimationManager::Instance().PlayAnimation(m_animEntity, m_animHunt, true, 1.5f);
        break;
    case GhostAI::State::Dead:
        AnimationManager::Instance().PlayAnimation(m_animEntity, m_animDead, false);
        break;
    }
}

void GhostAI::FacePosition(TransformData& cTrans, const Math::Vector3& dir, float deltaTime)
{
    if (dir.LengthSquared() <= 0.001f)
    {
        return;
    }

    float targetAngle = atan2f(dir.x, dir.z);

    // 最短角度で補間 (Lerpだと360度をまたいで不自然になるので注意)
    float diff = targetAngle - cTrans.m_rotation.y;
    while (diff < -DirectX::XM_PI)
    {
        diff += DirectX::XM_2PI;
    }
    while (diff > DirectX::XM_PI)
    {
        diff -= DirectX::XM_2PI;
    }

    cTrans.m_rotation.y += diff * 10.0f * deltaTime;
}

void GhostAI::UpdateWander(float deltaTime, TransformData& cTrans)
{
    // Room待機: ゴーストルーム(m_targetRoom)の中をうろうろする
    m_changeDirTimer -= deltaTime;
    if (m_changeDirTimer <= 0.0f)
    {
        float randX = Random::Instance().Range(-1.0f, 1.0f);
        float randZ = Random::Instance().Range(-1.0f, 1.0f);
        m_moveDir = Math::Vector3(randX, 0.0f, randZ);
        if (m_moveDir.LengthSquared() > 0.0f)
        {
            m_moveDir.Normalize();
        }
        m_changeDirTimer = Random::Instance().Range(1.0f, 3.0f);
    }

    Math::Vector3 nextPos = cTrans.m_position + m_moveDir * (m_moveSpeedKmh * kKmhToMs) * deltaTime;
    if (m_targetRoom == nullptr || m_targetRoom->IsInside(nextPos))
    {
        cTrans.m_position = nextPos;
    }
    else
    {
        // 部屋の外に出そうになったら、部屋の中心へ向きを変える
        Math::Vector3 roomCenter = (m_targetRoom->m_min + m_targetRoom->m_max) * 0.5f;
        m_moveDir = roomCenter - cTrans.m_position;
        m_moveDir.y = 0.0f;
        if (m_moveDir.LengthSquared() > 0.0f)
        {
            m_moveDir.Normalize();
        }
        else
        {
            // 中心にいる場合はランダムな方向へ
            m_moveDir = Math::Vector3(Random::Instance().Range(-1.0f, 1.0f), 0.0f, Random::Instance().Range(-1.0f, 1.0f));
            m_moveDir.Normalize();
        }
        m_changeDirTimer = 1.0f; // しばらくは中心に向かって歩き続ける
    }

    FacePosition(cTrans, m_moveDir, deltaTime);
}

// m_targetRoom以外の部屋から、現在位置からNavMesh上で実際に到達可能なものだけを集める。
// (家が複雑な形などの理由でNavMeshが繋がっていない部屋を選んでしまうと、
//  そこへ向かったまま永遠に立ち往生してしまうため)
std::vector<RoomArea*> GhostAI::CollectReachableCandidateRooms(const Math::Vector3& currentPos) const
{
    std::vector<RoomArea*> candidates;
    auto gs = GameSequence::GetInstance();
    if (!gs)
    {
        return candidates;
    }

    bool navReady = NavMeshManager::Instance().IsBuilt();
    for (RoomArea* room : gs->GetRooms())
    {
        if (!room || room == m_targetRoom)
        {
            continue;
        }
        // Stairs is a connector between floors, not a real destination room - its own center sits at
        // the stairs' mid-height, which GetEffectiveMoveTarget/the navmesh can resolve ambiguously to
        // either the top or bottom landing polygon, causing the ghost to climb up and down repeatedly
        // once it arrives. Never pick it as a wander target.
        if (room->m_isStairs)
        {
            continue;
        }
        if (navReady && !NavMeshManager::Instance().IsReachable(currentPos, room->GetCenter()))
        {
            continue;
        }
        candidates.push_back(room);
    }
    return candidates;
}

void GhostAI::StartHouseWander(const Math::Vector3& currentPos)
{
    std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(currentPos);

    if (candidates.empty())
    {
        // 行き先(到達可能な他の部屋)がない場合はRoom待機を続ける
        auto gs = GameSequence::GetInstance();
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
                                  "[GhostAI] StartHouseWander: no reachable candidate room (GameSequence rooms=%d, NavMeshBuilt=%d). Staying in Room idle.",
                                  gs ? (int)gs->GetRooms().size() : -1, (int)NavMeshManager::Instance().IsBuilt());
        m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
        return;
    }

    RoomArea* dest = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
    m_houseWanderTargetPos = dest->GetCenter();
    m_houseWanderDurationTimer = Random::Instance().Range(m_houseWanderDurationMin, m_houseWanderDurationMax);
    m_houseWanderReturning = false;
    m_houseWanderStuckTimer = 0.0f;
    m_houseWanderLastPos = currentPos;
    Logger::Instance().AddLog(Logger::LogLevel::Info,
                              "[GhostAI] StartHouseWander -> target=(%.1f, %.1f, %.1f) NavMeshBuilt=%d",
                              m_houseWanderTargetPos.x, m_houseWanderTargetPos.y, m_houseWanderTargetPos.z,
                              (int)NavMeshManager::Instance().IsBuilt());
    SetState(GhostAI::State::HouseWander);
}

void GhostAI::MoveTowardTarget(float deltaTime, TransformData& cTrans, const Math::Vector3& finalTarget, float speed)
{
    Math::Vector3 moveTarget = GetEffectiveMoveTarget(cTrans.m_position, finalTarget);
    m_debugFinalTarget = finalTarget;
    m_debugMoveTarget = moveTarget;

    Math::Vector3 nextPos;
    // Also require m_isCrossingFloors: GetStairsCrossingMove's fixed-overshoot target assumes the
    // room on the far side sits just outside the stairs' own AABB in the min/max-z direction, which
    // isn't true here (LivingRoom's real bounds actually overlap most of the stairs' footprint from
    // above/below rather than sitting beyond it) - once the ghost's height has already reached the
    // destination floor (m_isCrossingFloors false), continuing to walk that fixed direction just
    // pushes it further past the real landing into the house's exterior void. Gate on height as well
    // so arrival cuts the straight-line walk short and hands off to normal NavMesh movement (which
    // already has a working connector into the adjacent room) the moment it's no longer needed.
    // 昇り降りは、今いる階の階段の入口に着いてから始める(階段の周りのどこからでも始めると、
    // 階段の横から壁や天井をすり抜けてしまう)
    bool atStairsEntry = false;
    if (m_isCrossingFloors && m_stairsRoom)
    {
        Math::Vector3 entry = GetStairsEntryPoint(m_frozenExitLandingY);
        Math::Vector3 toEntry = entry - cTrans.m_position;
        float dy = fabsf(toEntry.y);
        toEntry.y = 0.0f;
        atStairsEntry = (toEntry.Length() <= m_stairsEntryReach) && (dy <= m_floorHeightThreshold);
    }
    if (!m_stairsCrossingLatched && atStairsEntry)
    {
        m_stairsCrossingLatched = true;
        StartStairsCrossing(cTrans.m_position, finalTarget, speed);
    }
    bool crossingStairs = m_stairsCrossingLatched;
    if (crossingStairs)
    {
        nextPos = GetStairsCrossingMove(deltaTime);
        if (!m_isCrossingFloors)
        {
            m_stairsCrossingLatched = false; // height has arrived - release the latch
        }
        // 保険: 補間が終わって(少し待っても)まだ渡り切った判定にならない時は、昇り降り中の状態を
        // 強制的に解除する。解除しないとGetStairsCrossingMoveが終点を返し続け、その場から一生動けない。
        const float kCrossReleaseGrace = 0.5f;
        if (m_stairsCrossingLatched && m_stairsCrossElapsed >= m_stairsCrossDuration + kCrossReleaseGrace)
        {
            Logger::Instance().AddLog(Logger::LogLevel::Warning,
                                      "[GhostAI] Stairs crossing finished without reaching the exit landing - force-releasing.");
            m_stairsCrossingLatched = false;
            m_isCrossingFloors = false;
            m_recentlyExitedStairs = true;
            // 直後の高さ固定(holdAtExitLanding)が、実際には着いていない出口の高さへ吸い寄せないよう、
            // 今いる高さを出口として扱う
            m_frozenExitLandingY = nextPos.y;
        }
        // Keep the cached NavMesh path cleared for the whole crossing so the moment it steps back
        // outside the stairwell, MoveToward computes a fresh route instead of resuming whatever
        // stale/irrelevant path happened to be cached from before it entered.
        NavMeshManager::Instance().ClearPath((int)GetGameObject()->GetEntityID());
    }
    else if (NavMeshManager::Instance().IsBuilt())
    {
        nextPos = NavMeshManager::Instance().MoveToward(
            (int)GetGameObject()->GetEntityID(), cTrans.m_position, moveTarget, speed, deltaTime,
            m_pathUpdateInterval, m_pathNodeReachThreshold);
    }
    else
    {
        // NavMesh未構築時は直線移動にフォールバック
        Math::Vector3 toTarget = finalTarget - cTrans.m_position;
        toTarget.y = 0.0f;
        if (toTarget.LengthSquared() > 0.0001f)
        {
            toTarget.Normalize();
        }
        nextPos = cTrans.m_position + toTarget * speed * deltaTime;
    }
    nextPos = ClampToHouseBounds(nextPos);

    Math::Vector3 moveDelta = nextPos - cTrans.m_position;
    moveDelta.y = 0.0f;
    cTrans.m_position.x = nextPos.x;
    cTrans.m_position.z = nextPos.z;
    if (crossingStairs)
    {
        // GetStairsCrossingMove already computed height directly from Z-progress along the stairs
        // (see its own comment) instead of relying on gravity/raycast, which was bouncing between
        // adjacent step treads - apply it here and skip the gravity+raycast block below for this
        // frame so it doesn't immediately override this with a raycast-derived height again.
        cTrans.m_position.y = nextPos.y;
        m_skipGravityThisFrame = true;
    }

    // 向き更新の閾値は1フレームの想定移動量に対する割合で判定する。固定値(0.01m/フレーム)だと
    // 高FPS時(ハント速度1.8m/sなら180fps超)に毎フレームの移動量が閾値を下回り、m_moveDirが
    // 更新されず向きが固定されたまま後ろ歩きで追ってくる不具合になっていた。
    const float minStep = speed * deltaTime * 0.1f;
    if (moveDelta.LengthSquared() > minStep * minStep && moveDelta.LengthSquared() > 1e-10f)
    {
        m_moveDir = moveDelta;
        m_moveDir.Normalize();
    }
    FacePosition(cTrans, m_moveDir, deltaTime);
}

void GhostAI::UpdateHouseWander(float deltaTime, TransformData& cTrans)
{
    // 徘徊: ゴーストルームの外側、家の範囲内に限定してNavMesh経由で歩き回る
    EnsureHouseBounds();
    TryOpenNearbyDoor(cTrans.m_position, deltaTime);

    MoveTowardTarget(deltaTime, cTrans, m_houseWanderTargetPos, m_moveSpeedKmh * kKmhToMs);
    const Math::Vector3& moveTarget = m_debugMoveTarget;

    if (!m_houseWanderReturning)
    {
        m_houseWanderDurationTimer -= deltaTime;
    }

    // 立ち往生検知: 経路が繋がっていない、あるいは何らかの理由でしばらく進めていない場合は
    // その目的地を諦める(IsReachableをすり抜けた想定外のケースの保険)
    // 毎フレームではなく一定時間ごとに区間の移動量をチェックする(毎フレーム比較だと
    // 微小な往復(壁の角などで2経路の間を行ったり来たりしている場合)でも
    // 毎フレーム比較で"10cm以上動いた"と判定されてしまい、閾値依存の検知なので、
    // 停滞を検知できない)
    m_houseWanderStuckCheckTimer += deltaTime;
    if (m_houseWanderStuckCheckTimer >= m_stuckCheckWindow)
    {
        float windowProgress = Math::Vector3::Distance(cTrans.m_position, m_houseWanderLastPos);
        if (windowProgress > m_stuckCheckMinProgress)
        {
            m_houseWanderStuckTimer = 0.0f;
        }
        else
        {
            m_houseWanderStuckTimer += m_houseWanderStuckCheckTimer;
        }
        m_houseWanderLastPos = cTrans.m_position;
        m_houseWanderStuckCheckTimer = 0.0f;
    }

    // Quick first-line recovery: if it hasn't made progress in a bit (but hasn't hit the full
    // give-up timeout yet), grant temporary pass-through so it can physically push through whatever
    // it's caught on (a wall/corner the approximate NavMesh geometry didn't perfectly account for)
    // rather than just standing there fighting collision until the full timeout gives up entirely.
    if (m_houseWanderStuckTimer >= m_quickStuckPassThroughDelay && m_doorPassThroughTimer <= 0.0f)
    {
        m_doorPassThroughTimer = m_doorPassThroughDuration;
    }

    if (m_houseWanderStuckTimer >= m_houseWanderStuckTimeout)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
                                  "[GhostAI] UpdateHouseWander: stuck for %.1fs at (%.2f,%.2f,%.2f) moveTarget=(%.2f,%.2f,%.2f) heading to (%.1f, %.1f, %.1f), giving up this destination.",
                                  m_houseWanderStuckTimer, cTrans.m_position.x, cTrans.m_position.y, cTrans.m_position.z,
                                  moveTarget.x, moveTarget.y, moveTarget.z,
                                  m_houseWanderTargetPos.x, m_houseWanderTargetPos.y, m_houseWanderTargetPos.z);
        m_houseWanderStuckTimer = 0.0f;

        if (m_houseWanderReturning)
        {
            // 帰り道でも立ち往生した場合は、その場でゴーストルームを目標にして仕切り直す
            m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
            SetState(GhostAI::State::Wander);
            return;
        }

        std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(cTrans.m_position);
        if (!candidates.empty())
        {
            RoomArea* dest = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
            m_houseWanderTargetPos = dest->GetCenter();
            m_houseWanderLastPos = cTrans.m_position;
        }
        else if (m_targetRoom)
        {
            m_houseWanderTargetPos = m_targetRoom->GetCenter();
            m_houseWanderReturning = true;
            m_houseWanderLastPos = cTrans.m_position;
        }
        return;
    }

    float distToTarget = Math::Vector3::Distance(cTrans.m_position, m_houseWanderTargetPos);
    if (distToTarget > m_roomArriveThreshold)
    {
        return; // まだ到着していない
    }

    if (m_houseWanderReturning)
    {
        // ゴーストルームへの帰還完了 -> Room待機へ
        m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
        SetState(GhostAI::State::Wander);
        return;
    }

    if (m_houseWanderDurationTimer <= 0.0f)
    {
        // 徘徊時間切れ -> ゴーストルームへ戻る
        if (m_targetRoom)
        {
            m_houseWanderTargetPos = m_targetRoom->GetCenter();
        }
        m_houseWanderReturning = true;
        m_houseWanderLastPos = cTrans.m_position;
        return;
    }

    // まだ徘徊を続ける: 到達可能な別の部屋を目指す
    std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(cTrans.m_position);
    if (!candidates.empty())
    {
        RoomArea* dest = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
        m_houseWanderTargetPos = dest->GetCenter();
    }
    else if (m_targetRoom)
    {
        // 他に行ける部屋がなければ帰還する
        m_houseWanderTargetPos = m_targetRoom->GetCenter();
        m_houseWanderReturning = true;
    }
    m_houseWanderLastPos = cTrans.m_position;
}

bool GhostAI::CanSeePlayer(const TransformData& cTrans, Math::Vector3& outPlayerPos)
{
    if (m_playerEntity == INVALID_ENTITY)
    {
        m_playerEntity = FindPlayerEntity();
    }
    if (m_playerEntity == INVALID_ENTITY)
    {
        return false;
    }

    auto& ecs = GameManager::Instance().GetECS();
    auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity);
    if (!pPlayerTrans)
    {
        return false;
    }

    outPlayerPos = pPlayerTrans->m_position;

    // 視点位置と、狙う目標位置(足元だと不自然なので、少し上を狙う)
    Math::Vector3 eyePos = cTrans.m_position + Math::Vector3(0.0f, m_eyeHeight, 0.0f);
    Math::Vector3 targetPos = outPlayerPos + Math::Vector3(0.0f, 1.0f, 0.0f);

    Math::Vector3 toPlayer = targetPos - eyePos;
    float distance = toPlayer.Length();
    if (distance > m_visionRange)
    {
        return false;
    }
    // 距離が近すぎるとその後のレイキャストが不安定(最大距離がほぼ0になる)になるため、
    // その場合は無条件で視認したことにする
    if (distance <= 0.3f)
    {
        return true;
    }

    Math::Vector3 dir = toPlayer;
    dir /= distance;

    // 視野角チェック(水平方向のみ。Ghostの向き=rotation.yを基準にする)
    Math::Vector3 forward(sinf(cTrans.m_rotation.y), 0.0f, cosf(cTrans.m_rotation.y));
    Math::Vector3 flatDir(dir.x, 0.0f, dir.z);
    // 近距離(m_proximitySenseDistance以内)は背後でも気付く。遮蔽チェックは下で行うので壁越しは不可
    float flatDist = flatDir.Length() * distance;
    if (flatDist > m_proximitySenseDistance && flatDir.LengthSquared() > 0.0001f)
    {
        flatDir.Normalize();
        float cosHalfFov = cosf(DirectX::XMConvertToRadians(m_visionAngle * 0.5f));
        if (forward.Dot(flatDir) < cosHalfFov)
        {
            return false;
        }
    }

    // 遮蔽チェック: Playerまでの間にあるStageメッシュ(壁など)に当たったら見えていない
    RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(eyePos, dir, distance - 0.2f, "Stage");
    if (hit.hit)
    {
        return false;
    }

    return true;
}

// HouseWander/Huntで移動中に、近くにある閉まっているドアを自動的に開ける。
// 毎フレーム全エンティティを走査するのは重いので、m_doorCheckIntervalで間引く。
// Playerの手動ドア操作(Player::TryInteractDoor)と同じ仕組みを使うが、Ghostは開けるだけで閉めない。
void GhostAI::TryOpenNearbyDoor(const Math::Vector3& ghostPos, float deltaTime)
{
    m_doorCheckTimer -= deltaTime;
    if (m_doorCheckTimer > 0.0f)
    {
        return;
    }
    m_doorCheckTimer = m_doorCheckInterval;

    auto& ecs = GameManager::Instance().GetECS();
    auto& animMgr = AnimationManager::Instance();

    Entity bestEntity = INVALID_ENTITY;
    int bestAnimIdx = -1;
    float bestDist = FLT_MAX;

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);
        if (!pModel || !pTransform)
        {
            continue;
        }
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded())
        {
            continue;
        }

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty())
        {
            continue;
        }

        Math::Vector3 entityPos = pTransform->m_position;
        if ((ghostPos - entityPos).Length() > 50.0f)
        {
            continue; // 距離で大まかに足切り
        }

        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
        if (!pAnim)
        {
            ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});
            pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
            if (!pAnim)
            {
                continue;
            }
        }

        for (int i = 0; i < (int)anims.size(); ++i)
        {
            const std::string& animName = anims[i].name;
            if (animName.find("Door") == std::string::npos &&
                animName.find("door") == std::string::npos)
            {
                continue;
            }

            Math::Vector3 doorPos = entityPos;
            if (!anims[i].channels.empty())
            {
                const std::string& doorNodeName = anims[i].channels[0].nodeName;
                const auto& nodes = pModel->m_spModelData->GetNodes();
                for (const auto& node : nodes)
                {
                    if (node.name == doorNodeName)
                    {
                        Math::Vector3 nodeScale, nodeTrans;
                        Math::Quaternion nodeRot;
                        (node.globalTransform * pTransform->m_worldMatrix).Decompose(nodeScale, nodeRot, nodeTrans);
                        doorPos = nodeTrans;
                        break;
                    }
                }
            }

            // 既に開いている(または開いている最中の)ドアは対象外 - 閉じているものだけ開ける
            const auto& state = pAnim->multiAnims[i];
            if (state.IsPlaying || state.ProgressTime > 0.0f)
            {
                continue;
            }
            if (DoorLock::IsLocked(entity, i))
            {
                continue; // 施錠中(ハント中の玄関)は開けない
            }

            float dist = (ghostPos - doorPos).Length();
            if (dist < m_doorOpenRange && dist < bestDist)
            {
                bestDist = dist;
                bestEntity = entity;
                bestAnimIdx = i;
            }
        }
    }

    if (bestEntity == INVALID_ENTITY || bestAnimIdx < 0)
    {
        return;
    }

    animMgr.PlayMultiAnimation(bestEntity, bestAnimIdx, false, 1.0f);
    // 誰もいないのにドアが軋む音がする -> 近くにいればプレイヤーにも聞こえる
    AudioManager::Instance().PlaySEAt("Asset/Sound/SE/door_open.wav", ghostPos, m_doorSoundVolume, 15.0f);

    // While the door swings open, its collider is briefly non-static and can physically shove the
    // Ghost's own capsule collider back away from the doorway (both are dynamic colliders, so the
    // push gets split between them) - make the Ghost's own collider a trigger for a few seconds so
    // it doesn't get pushed around while walking through.
    m_doorPassThroughTimer = m_doorPassThroughDuration;
    SetDoorPassThrough(true);

    //ドアアニメーション中はコライダーを動的扱いにして毎フレームAABBが更新されるようにする
    // (Player::TryInteractDoorと同じ仕組み)
    auto* pCollider = ecs.TryGetComponent<ColliderData>(bestEntity);
    if (pCollider && pCollider->m_isStatic)
    {
        pCollider->m_isStatic = false;
    }
}

void GhostAI::UpdateHunt(float deltaTime, TransformData& cTrans)
{
    // ハント: 視認できている間はPlayerの現在位置を追跡する。見失ったら最終目撃地点へ向かい、
    // そこでも見つからなければ到達可能な部屋を巡回して捜索する(ハント時間が切れるまで)。
    if (m_playerEntity == INVALID_ENTITY)
    {
        m_playerEntity = FindPlayerEntity();
    }

    Math::Vector3 seenPos;
    m_isSeeingPlayer = CanSeePlayer(cTrans, seenPos);
    // 点灯したフラッシュライトを手に持っている間は、壁越し・視野外でも居場所が分かる
    m_isSensingLight = IsPlayerHoldingLitFlashLight();
    // 視認加速: 全身が見えている間は溜まり、見失うとm_losDecayTime秒かけて戻る
    m_isSeeingFullBody = m_isSeeingPlayer && CanSeePlayerFullBody(cTrans);
    if (m_isSeeingFullBody)
    {
        m_losTimer = (std::min)(m_losTimer + deltaTime, m_losTimeToMax);
    }
    else
    {
        float decayPerSec = (m_losDecayTime > 0.0001f) ? (m_losTimeToMax / m_losDecayTime) : FLT_MAX;
        m_losTimer = (std::max)(0.0f, m_losTimer - decayPerSec * deltaTime);
    }
    // 照明による減速: 体(胴の高さ)が点いている照明に照らされているか
    m_isInLight = IsPositionLit(cTrans.m_position + Math::Vector3(0.0f, 1.0f, 0.0f));

    // 追跡: 一度視認したら、壁に隠れても現在位置を追い続ける。視認加速が溜まらない(全身が見えていない)
    // 状態がm_trackGiveUpTime秒続いたら追跡をやめ、最後に分かっていた位置からの捜索に移る。
    if (m_isSeeingPlayer)
    {
        m_isTrackingPlayer = true;
    }
    if (m_isTrackingPlayer)
    {
        if (m_isSeeingFullBody)
        {
            m_trackGiveUpTimer = 0.0f;
        }
        else
        {
            m_trackGiveUpTimer += deltaTime;
            if (m_trackGiveUpTimer >= m_trackGiveUpTime)
            {
                m_isTrackingPlayer = false;
                m_trackGiveUpTimer = 0.0f;
            }
        }
    }
    bool knowsPlayerPos = m_isTrackingPlayer || m_isSensingLight;

    if (knowsPlayerPos && m_playerEntity != INVALID_ENTITY)
    {
        auto& ecs = GameManager::Instance().GetECS();
        if (auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity))
        {
            m_lastKnownPlayerPos = pPlayerTrans->m_position;
            m_hasLastKnownPlayerPos = true;
            m_huntSearchRoom = nullptr;
        }
    }

    Math::Vector3 target = cTrans.m_position;
    if (m_hasLastKnownPlayerPos)
    {
        target = m_lastKnownPlayerPos;
        if (!knowsPlayerPos)
        {
            // 最終目撃地点に着いても見つからなければ捜索に切り替える(別の階は着いたとみなさない)
            Math::Vector3 toLast = m_lastKnownPlayerPos - cTrans.m_position;
            float dy = fabsf(toLast.y);
            toLast.y = 0.0f;
            if (toLast.Length() <= m_searchGiveUpDistance && dy <= m_floorHeightThreshold)
            {
                m_hasLastKnownPlayerPos = false;
                PickHuntSearchRoom(cTrans.m_position);
            }
        }
    }
    if (!m_hasLastKnownPlayerPos)
    {
        if (!m_huntSearchRoom || m_huntSearchRoom->IsInside(cTrans.m_position))
        {
            PickHuntSearchRoom(cTrans.m_position);
        }
        if (m_huntSearchRoom)
        {
            target = m_huntSearchRoom->GetCenter();
        }
    }

    EnsureHouseBounds();
    TryOpenNearbyDoor(cTrans.m_position, deltaTime);

    // NavMeshが構築済みならそちらの経路で移動する(壁を突き抜けず確実にドアや通路を経由できる)
    MoveTowardTarget(deltaTime, cTrans, target, GetHuntSpeed());

    // Hunt previously had no stuck-detection at all (unlike UpdateHouseWander), so once physically
    // caught on something the ghost would just stand there fighting collision for the rest of the
    // Hunt duration with no recovery. Mirror the same quick pass-through grant used there.
    // Progress is measured over a window (see m_stuckCheckWindow's comment) rather than
    // frame-to-frame, so rapid back-and-forth oscillation - which looks like progress on any
    // single frame - is correctly detected as stuck too.
    m_huntStuckCheckTimer += deltaTime;
    if (m_huntStuckCheckTimer >= m_stuckCheckWindow)
    {
        float windowProgress = Math::Vector3::Distance(cTrans.m_position, m_huntStuckLastPos);
        if (windowProgress > m_stuckCheckMinProgress)
        {
            m_huntStuckTimer = 0.0f;
        }
        else
        {
            m_huntStuckTimer += m_huntStuckCheckTimer;
        }
        m_huntStuckLastPos = cTrans.m_position;
        m_huntStuckCheckTimer = 0.0f;
    }
    if (m_huntStuckTimer >= m_quickStuckPassThroughDelay && m_doorPassThroughTimer <= 0.0f)
    {
        m_doorPassThroughTimer = m_doorPassThroughDuration;
    }
}

float GhostAI::GetHuntSpeed() const
{
    float t = (m_losTimeToMax > 0.0001f) ? std::clamp(m_losTimer / m_losTimeToMax, 0.0f, 1.0f) : 1.0f;
    float speedKmh = m_huntSpeedKmh + (m_huntMaxSpeedKmh - m_huntSpeedKmh) * t;
    if (m_isInLight)
    {
        speedKmh *= m_litSpeedMultiplier;
    }
    return speedKmh * kKmhToMs;
}

bool GhostAI::CanSeePlayerFullBody(const TransformData& cTrans)
{
    if (m_playerEntity == INVALID_ENTITY)
    {
        return false;
    }
    auto* pPlayerTrans = GameManager::Instance().GetECS().TryGetComponent<TransformData>(m_playerEntity);
    if (!pPlayerTrans)
    {
        return false;
    }

    // 視野角・距離はCanSeePlayer(胴の高さ)で確認済みの前提。ここでは足元・胴・頭の3点すべてに
    // 遮蔽物が無いかだけを見る(家具の陰で下半身が隠れている等は「全身は見えていない」扱い)。
    Math::Vector3 eyePos = cTrans.m_position + Math::Vector3(0.0f, m_eyeHeight, 0.0f);
    const float kBodyHeights[] = {0.2f, 1.0f, 1.6f};
    for (float h : kBodyHeights)
    {
        Math::Vector3 toPoint = pPlayerTrans->m_position + Math::Vector3(0.0f, h, 0.0f) - eyePos;
        float dist = toPoint.Length();
        if (dist <= 0.3f)
        {
            continue;
        }
        toPoint /= dist;
        RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(eyePos, toPoint, dist - 0.2f, "Stage");
        if (hit.hit)
        {
            return false;
        }
    }
    return true;
}

bool GhostAI::IsPositionLit(const Math::Vector3& pos) const
{
    auto& ecs = GameManager::Instance().GetECS();

    auto inSpot = [&](Entity e, const SpotLightData& light)
    {
        auto* pTrans = ecs.TryGetComponent<TransformData>(e);
        if (!pTrans)
        {
            return false;
        }
        Math::Vector3 lightPos = Math::Vector3::Transform(light.m_localOffset, pTrans->m_worldMatrix);
        Math::Vector3 dir = Math::Vector3::TransformNormal(light.m_localDirection, pTrans->m_worldMatrix);
        Math::Vector3 toPos = pos - lightPos;
        float dist = toPos.Length();
        if (dist > light.m_range)
        {
            return false;
        }
        if (dist < 0.001f || dir.LengthSquared() < 1e-8f)
        {
            return true;
        }
        dir.Normalize();
        return (toPos / dist).Dot(dir) >= cosf(DirectX::XMConvertToRadians(light.m_outerAngle));
    };
    auto inPoint = [&](Entity e, const PointLightData& light)
    {
        auto* pTrans = ecs.TryGetComponent<TransformData>(e);
        return pTrans && Math::Vector3::Distance(pTrans->m_worldMatrix.Translation(), pos) <= light.m_range;
    };

    // 照明(StimulusSource)配下のライト: 点滅(Flicker)で一瞬消えていても、照明が点いていれば点灯扱い
    std::unordered_set<Entity> managed;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (!src || src->m_type != StimulusType::Light || !src->GetGameObject())
        {
            continue;
        }
        bool lit = false;
        std::function<void(GameObject*)> visit = [&](GameObject* obj)
        {
            Entity e = obj->GetEntityID();
            managed.insert(e);
            if (src->IsActive() && !lit)
            {
                if (auto* pSpot = ecs.TryGetComponent<SpotLightData>(e))
                {
                    lit = inSpot(e, *pSpot);
                }
                if (!lit)
                {
                    if (auto* pPoint = ecs.TryGetComponent<PointLightData>(e))
                    {
                        lit = inPoint(e, *pPoint);
                    }
                }
            }
            for (auto& child : obj->GetChildren())
            {
                visit(child.get());
            }
        };
        visit(src->GetGameObject());
        if (lit)
        {
            return true;
        }
    }

    // それ以外のスポットライト(持っている/落ちているフラッシュライト等)は実際の点灯状態で判定する。
    // 照明に属さないポイントライト(ロウソク等の演出)は「電気」ではないので対象外。
    auto& spots = ecs.GetComponentArray<SpotLightData>();
    for (size_t i = 0; i < spots.GetSize(); ++i)
    {
        Entity e = spots.GetEntityFromIndex(i);
        const SpotLightData& light = *(spots.begin() + i);
        if (!light.m_enabled || managed.count(e))
        {
            continue;
        }
        if (inSpot(e, light))
        {
            return true;
        }
    }
    return false;
}

bool GhostAI::IsPlayerHoldingLitFlashLight()
{
    if (m_playerEntity == INVALID_ENTITY)
    {
        return false;
    }
    // PlayerはECSのComponent型として登録されていないので、NativeScriptData経由でdynamic_castする
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pScriptData = ecs.TryGetComponent<NativeScriptData>(m_playerEntity))
    {
        if (auto* pPlayer = dynamic_cast<Player*>(pScriptData->Instance.get()))
        {
            return pPlayer->IsHoldingLitFlashLight();
        }
    }
    return false;
}

void GhostAI::LockEntranceDoor()
{
    if (m_entranceDoorName.empty())
    {
        return;
    }
    auto& ecs = GameManager::Instance().GetECS();

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        if (!pModel || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded())
        {
            continue;
        }
        if (!ecs.TryGetComponent<TransformData>(entity))
        {
            continue;
        }

        const auto& anims = pModel->m_spModelData->GetAnimations();
        for (int i = 0; i < (int)anims.size(); ++i)
        {
            if (anims[i].name.find(m_entranceDoorName) == std::string::npos)
            {
                continue;
            }

            auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
            if (!pAnim)
            {
                ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});
                pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
                if (!pAnim)
                {
                    continue;
                }
            }

            // 開いている/開いている最中なら逆再生で閉める(Player::TryInteractDoorの閉める処理と同じ)
            auto& state = pAnim->multiAnims[i];
            bool isClosingOrClosed = (state.IsPlaying && state.Speed < 0.0f) || (!state.IsPlaying && state.ProgressTime <= 0.0f);
            if (!isClosingOrClosed)
            {
                float currentTime = state.ProgressTime;
                AnimationManager::Instance().PlayMultiAnimation(entity, i, false, -1.0f);
                pAnim->multiAnims[i].ProgressTime = currentTime;
                if (auto* pCollider = ecs.TryGetComponent<ColliderData>(entity))
                {
                    pCollider->m_isStatic = false;
                }
            }
            DoorLock::Lock(entity, i);
            Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Hunt: entrance door '%s' closed and locked.", m_entranceDoorName.c_str());
        }
    }
}

void GhostAI::UpdateLightFlicker(float deltaTime, const Math::Vector3& ghostPos)
{
    // 毎フレーム全スクリプトを走査しないよう間引く(点滅時間を間隔より長くして途切れないようにする)
    const float kCheckInterval = 0.2f;
    m_lightFlickerCheckTimer -= deltaTime;
    if (m_lightFlickerCheckTimer > 0.0f)
    {
        return;
    }
    m_lightFlickerCheckTimer = kCheckInterval;

    const bool hunting = (m_currentState == State::Hunt);
    auto& ecs = GameManager::Instance().GetECS();
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (!src || src->m_type != StimulusType::Light || !src->IsActive())
        {
            continue;
        }

        Math::Vector3 lightPos = src->GetWorldPosition();
        // Y+方向(足元より上)の照明だけを対象にする。2階にいる時、1階の天井照明は足元より
        // わずかに下にあるので除外され、上の階の照明はm_lightFlickerVerticalRangeで除外される
        float dy = lightPos.y - ghostPos.y;
        if (dy <= 0.0f || dy > m_lightFlickerVerticalRange)
        {
            continue;
        }
        Math::Vector3 flat = lightPos - ghostPos;
        flat.y = 0.0f;
        if (flat.Length() > m_lightFlickerRange)
        {
            continue;
        }

        if (hunting)
        {
            // ハント中は近くにいる間ずっと点滅させる
            src->Flicker(kCheckInterval * 2.5f);
        }
        else if (!src->IsFlickering() &&
                 Random::Instance().Range(0.0f, 1.0f) < m_wanderFlickerChance * kCheckInterval)
        {
            // ハント以外は時々、短く点滅させる
            src->Flicker(Random::Instance().Range(0.3f, 1.0f));
        }
    }

    // フラッシュライト(手持ち・床に落ちている物)にも干渉する。消しはせず、見た目だけ点滅させる。
    // 同じ階(高さの差がm_floorHeightThreshold以内)で、m_flashLightFlickerRange以内にある時だけ。
    auto nearFlashLight = [&](const Math::Vector3& pos)
    {
        if (fabsf(pos.y - ghostPos.y) > m_floorHeightThreshold)
        {
            return false;
        }
        return Math::Vector3::Distance(pos, ghostPos) <= m_flashLightFlickerRange;
    };
    // ハント中は近くにいる間ずっと、それ以外は時々「しばらく」(1〜3秒)点滅させる
    auto flickerDuration = [&](bool alreadyFlickering) -> float
    {
        if (hunting)
        {
            return kCheckInterval * 2.5f;
        }
        if (alreadyFlickering)
        {
            return 0.0f;
        }
        if (Random::Instance().Range(0.0f, 1.0f) >= m_wanderFlickerChance * kCheckInterval)
        {
            return 0.0f;
        }
        return Random::Instance().Range(1.0f, 3.0f);
    };

    if (m_playerEntity == INVALID_ENTITY)
    {
        m_playerEntity = FindPlayerEntity();
    }
    if (auto* pScriptData = ecs.TryGetComponent<NativeScriptData>(m_playerEntity))
    {
        auto* pPlayer = dynamic_cast<Player*>(pScriptData->Instance.get());
        auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity);
        if (pPlayer && pPlayerTrans && pPlayer->IsFlashLightOn() && pPlayer->HasItem(ItemType::FlashLight) &&
            nearFlashLight(pPlayerTrans->m_position))
        {
            float d = flickerDuration(pPlayer->IsFlashLightFlickering());
            if (d > 0.0f)
            {
                pPlayer->FlickerFlashLight(d);
            }
        }
    }
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* pickup = dynamic_cast<PickupItem*>(scriptData.Instance.get());
        if (!pickup || pickup->m_itemType != ItemType::FlashLight || !pickup->IsLightOn() || pickup->m_collected)
        {
            continue;
        }
        if (!pickup->GetGameObject())
        {
            continue;
        }
        auto* pTrans = ecs.TryGetComponent<TransformData>(pickup->GetGameObject()->GetEntityID());
        if (!pTrans || !nearFlashLight(pTrans->m_position))
        {
            continue;
        }
        float d = flickerDuration(pickup->IsLightFlickering());
        if (d > 0.0f)
        {
            pickup->FlickerLight(d);
        }
    }
}

void GhostAI::TurnOffAllLights()
{
    auto& ecs = GameManager::Instance().GetECS();
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (src && src->m_type == StimulusType::Light)
        {
            src->SetActive(false);
        }
    }
}

// ハント中に見失った後の捜索先として、到達可能な部屋をランダムに選ぶ(今いる部屋は除く)
void GhostAI::PickHuntSearchRoom(const Math::Vector3& currentPos)
{
    std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(currentPos);
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                    [&](RoomArea* room)
    { return room == m_huntSearchRoom || room->IsInside(currentPos); }),
                     candidates.end());
    m_huntSearchRoom = candidates.empty() ? nullptr
                                          : candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
}

// 種類ごとに感知する刺激。感知しない種類(地縛霊、未実装の家鳴)はfalseを返す。
static bool GetSensedStimulusType(GhostAI::GhostType type, StimulusType& outType)
{
    switch (type)
    {
    case GhostAI::GhostType::Kageonna:
        outType = StimulusType::Light;
        return true;
    default:
        return false;
    }
}

void GhostAI::ReturnToGhostRoom(const Math::Vector3& currentPos)
{
    // テレポートせず、ゴーストルームまでNavMesh経由で徒歩で帰る
    if (m_targetRoom && NavMeshManager::Instance().IsBuilt())
    {
        m_houseWanderTargetPos = m_targetRoom->GetCenter();
        m_houseWanderReturning = true;
        m_houseWanderDurationTimer = 0.0f;
        m_houseWanderStuckTimer = 0.0f;
        m_houseWanderLastPos = currentPos;
        SetState(GhostAI::State::HouseWander);
    }
    else
    {
        // 目標の部屋がない、またはNavMesh未構築ならその場でRoom待機扱いにする
        m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
        SetState(GhostAI::State::Wander);
    }
}

void GhostAI::CheckStimulus(float deltaTime, const Math::Vector3& currentPos)
{
    StimulusType sensed;
    if (!GetSensedStimulusType(m_ghostType, sensed))
    {
        return;
    }

    if (m_lureCooldownTimer > 0.0f)
    {
        m_lureCooldownTimer -= deltaTime;
        return;
    }

    // 毎フレーム全スクリプトを走査しないよう間引く
    m_lureCheckTimer -= deltaTime;
    if (m_lureCheckTimer > 0.0f)
    {
        return;
    }
    m_lureCheckTimer = m_lureCheckInterval;

    StimulusSource* src = StimulusSource::FindNearestActive(sensed, currentPos);
    if (!src)
    {
        return;
    }

    // NavMeshで繋がっていない場所の発生源へ向かうと立ち往生するので無視する
    Math::Vector3 srcPos = src->GetWorldPosition();
    if (NavMeshManager::Instance().IsBuilt() && !NavMeshManager::Instance().IsReachable(currentPos, srcPos))
    {
        return;
    }

    SetState(GhostAI::State::Lured);
    m_lureSource = src;
    m_lureArrived = false;
    m_houseWanderStuckTimer = 0.0f;
    m_houseWanderStuckCheckTimer = 0.0f;
    m_houseWanderLastPos = currentPos;
    Logger::Instance().AddLog(Logger::LogLevel::Info,
                              "[GhostAI] Lured by stimulus at (%.1f, %.1f, %.1f)", srcPos.x, srcPos.y, srcPos.z);
}

void GhostAI::UpdateLured(float deltaTime, TransformData& cTrans)
{
    // 発生源が消された(プレイヤーが消灯した等)ら誘導終了
    if (!m_lureSource || !m_lureSource->IsActive())
    {
        ReturnToGhostRoom(cTrans.m_position);
        return;
    }

    Math::Vector3 srcPos = m_lureSource->GetWorldPosition();

    if (m_lureArrived)
    {
        // 到着後: 発生源の方を向いてその場に留まる(姿は見せない)
        Math::Vector3 toSrc = srcPos - cTrans.m_position;
        toSrc.y = 0.0f;
        FacePosition(cTrans, toSrc, deltaTime);

        m_lureDwellTimer -= deltaTime;
        if (m_lureDwellTimer <= 0.0f)
        {
            ReturnToGhostRoom(cTrans.m_position);
        }
        return;
    }

    EnsureHouseBounds();
    TryOpenNearbyDoor(cTrans.m_position, deltaTime);
    MoveTowardTarget(deltaTime, cTrans, srcPos, m_moveSpeedKmh * kKmhToMs);

    Math::Vector3 flatToSrc = srcPos - cTrans.m_position;
    flatToSrc.y = 0.0f;
    if (flatToSrc.Length() <= m_lureArriveThreshold)
    {
        m_lureArrived = true;
        m_lureDwellTimer = m_lureDwellDuration;
        NavMeshManager::Instance().ClearPath((int)GetGameObject()->GetEntityID());
        return;
    }

    // 立ち往生検知(UpdateHouseWanderと同じ区間ごとの判定)。進めなければ誘導を諦めて帰る。
    m_houseWanderStuckCheckTimer += deltaTime;
    if (m_houseWanderStuckCheckTimer >= m_stuckCheckWindow)
    {
        float windowProgress = Math::Vector3::Distance(cTrans.m_position, m_houseWanderLastPos);
        if (windowProgress > m_stuckCheckMinProgress)
        {
            m_houseWanderStuckTimer = 0.0f;
        }
        else
        {
            m_houseWanderStuckTimer += m_houseWanderStuckCheckTimer;
        }
        m_houseWanderLastPos = cTrans.m_position;
        m_houseWanderStuckCheckTimer = 0.0f;
    }
    if (m_houseWanderStuckTimer >= m_quickStuckPassThroughDelay && m_doorPassThroughTimer <= 0.0f)
    {
        m_doorPassThroughTimer = m_doorPassThroughDuration;
    }
    if (m_houseWanderStuckTimer >= m_houseWanderStuckTimeout)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, "[GhostAI] UpdateLured: stuck, giving up the lure.");
        ReturnToGhostRoom(cTrans.m_position);
    }
}

void GhostAI::UpdateTell(float deltaTime, const Math::Vector3& currentPos)
{
    m_tellTimer -= deltaTime;
    if (m_tellTimer > 0.0f)
    {
        return;
    }
    m_tellTimer = Random::Instance().Range(m_tellIntervalMin, m_tellIntervalMax);

    // 照明のいたずら(全種類)。影女は点いている照明を消すだけで、点けることはしない。
    // それ以外のゴーストは、近くの照明をランダムに選んで点けたり消したりする。
    // プレイヤーが元に戻せるよう、Fキーか壁スイッチで操作できる照明だけを対象にする。
    const bool onlyTurnOff = (m_ghostType == GhostType::Kageonna);
    auto& ecs = GameManager::Instance().GetECS();
    std::vector<StimulusSource*> candidates;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (!src || src->m_type != StimulusType::Light)
        {
            continue;
        }
        if (!(src->m_interactable || src->m_controlledBySwitch))
        {
            continue;
        }
        if (onlyTurnOff && !src->IsActive())
        {
            continue;
        }
        if (Math::Vector3::Distance(currentPos, src->GetWorldPosition()) > m_tellRange)
        {
            continue;
        }
        candidates.push_back(src);
    }
    if (candidates.empty())
    {
        return;
    }

    StimulusSource* target = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
    bool turnOn = !target->IsActive();
    target->SetActive(turnOn);
    target->PlayToggleSound(); // 壁スイッチ式ならスイッチ音、紐スイッチ式なら点灯/消灯音
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Tell: turned %s a light by itself.", turnOn ? "on" : "off");
}

bool GhostAI::IsAnyLightLitNear(const Math::Vector3& ghostPos) const
{
    auto& ecs = GameManager::Instance().GetECS();
    const float radiusSq = m_kageonnaLightRadius * m_kageonnaLightRadius;

    // 数えるのは「部屋の電気」= Room Lightの照明(StimulusSource: 天井照明等)だけ。ランプ等の小さな
    // 明かり(m_isRoomLight=false)とフラッシュライト(手持ち・床)は数えない(部屋の電気を点けないと倒せない)。演出用に置いただけの消せない
    // ポイントライト(ロウソク等)も対象外。点滅中も点灯扱い(IsActive)で判定する。
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (!src || !src->IsActive() || src->m_type != StimulusType::Light || !src->m_isRoomLight)
        {
            continue;
        }
        if (Math::Vector3::DistanceSquared(src->GetWorldPosition(), ghostPos) <= radiusSq)
        {
            return true;
        }
    }
    return false;
}

bool GhostAI::TryIncenseStun(float stunTime)
{
    if (m_isExorcised)
    {
        return false;
    }

    // 退治(香でのスタン)はハント中だけ。それ以外の時に当てても何も起きない。
    if (m_currentState != State::Hunt)
    {
        return false;
    }

    bool canStun = true;
    switch (m_ghostType)
    {
    case GhostType::Kageonna:
    {
        // 影女は周りで部屋の電気(天井照明等。ランプ・フラッシュライトは除く)が点いている時だけ効く(影は光の中にしか映らない)
        auto& ecs = GameManager::Instance().GetECS();
        auto* pTrans = ecs.TryGetComponent<TransformData>(GetGameObject()->GetEntityID());
        canStun = pTrans && IsAnyLightLitNear(pTrans->m_position);
        break;
    }
    default:
        break;
    }

    if (canStun)
    {
        ApplyStun(stunTime);
        return true;
    }

    // 失敗: Hunt中ならHuntが続いたまま時間が延長される
    if (m_currentState == State::Hunt)
    {
        m_huntTimer += m_failHuntExtension;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Incense failed: Hunt extended by %.1fs.", m_failHuntExtension);
    }
    return false;
}

void GhostAI::Update(float deltaTime)
{
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    if (m_isExorcised)
    {
        UpdateExorciseEffect(deltaTime, cTrans);
        return;
    }

    EnsureStairsRoom();

    if (m_currentState == GhostAI::State::Stun)
    {
        m_stunTimer -= deltaTime;
        if (m_stunTimer <= 0.0f)
        {
            SetState(GhostAI::State::Wander);
        }
        return;
    }

    // ハントは位置や視認に関係なく、定期的に無条件で開始する(Room待機・徘徊・誘導中)
    if (m_currentState == GhostAI::State::Wander || m_currentState == GhostAI::State::HouseWander ||
        m_currentState == GhostAI::State::Lured)
    {
        // Debug: force-start a Hunt immediately, bypassing the random trigger timer, so testing
        // doesn't require waiting out m_huntTriggerIntervalMin/Max.
        bool debugForceHunt = Input::Instance().IsKeyTrigger('H');
        m_huntTriggerTimer -= deltaTime;
        if (m_huntTriggerTimer <= 0.0f || debugForceHunt)
        {
            SetState(GhostAI::State::Hunt);
            m_huntTimer = Random::Instance().Range(m_huntDurationMin, m_huntDurationMax);
            m_huntTriggerTimer = Random::Instance().Range(m_huntTriggerIntervalMin, m_huntTriggerIntervalMax);
            m_huntStuckTimer = 0.0f;
            m_huntStuckLastPos = cTrans.m_position;
            // 開始時点のPlayer位置だけは把握している(そこへ向かう間に視認できなければ捜索になる)
            m_huntSearchRoom = nullptr;
            m_hasLastKnownPlayerPos = false;
            m_isTrackingPlayer = false;
            m_trackGiveUpTimer = 0.0f;
            m_losTimer = 0.0f;
            if (m_playerEntity == INVALID_ENTITY)
            {
                m_playerEntity = FindPlayerEntity();
            }
            if (auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity))
            {
                m_lastKnownPlayerPos = pPlayerTrans->m_position;
                m_hasLastKnownPlayerPos = true;
            }
            if (debugForceHunt)
            {
                Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Debug: Hunt force-started via H key.");
            }
        }
    }

    if (m_currentState == GhostAI::State::Wander || m_currentState == GhostAI::State::HouseWander)
    {
        CheckStimulus(deltaTime, cTrans.m_position);
    }
    if (m_currentState == GhostAI::State::Wander || m_currentState == GhostAI::State::HouseWander ||
        m_currentState == GhostAI::State::Lured)
    {
        UpdateTell(deltaTime, cTrans.m_position);
    }

    UpdateLightFlicker(deltaTime, cTrans.m_position);

    switch (m_currentState)
    {
    case GhostAI::State::Lured:
        UpdateLured(deltaTime, cTrans);
        break;

    case GhostAI::State::Wander:
        m_houseWanderTimer -= deltaTime;
        if (m_houseWanderTimer <= 0.0f)
        {
            StartHouseWander(cTrans.m_position);
        }
        else
        {
            UpdateWander(deltaTime, cTrans);
        }
        break;

    case GhostAI::State::HouseWander:
        UpdateHouseWander(deltaTime, cTrans);
        break;

    case GhostAI::State::Hunt:
        m_huntTimer -= deltaTime;
        if (m_huntTimer <= 0.0f)
        {
            // ハントが時間切れで終わった = プレイヤーが生き延びた(リザルトの表示用に数える)
            if (auto gs = GameSequence::GetInstance())
            {
                gs->NotifyHuntSurvived();
            }
            // ハント終了: テレポートせず、ゴーストルームまでNavMesh経由で徒歩で帰る
            m_hasLastKnownPlayerPos = false;
            m_isSeeingPlayer = false;
            m_isSensingLight = false;
            m_isTrackingPlayer = false;
            m_trackGiveUpTimer = 0.0f;
            m_huntSearchRoom = nullptr;
            m_losTimer = 0.0f;
            m_isSeeingFullBody = false;
            m_isInLight = false;
            ReturnToGhostRoom(cTrans.m_position);
        }
        else
        {
            UpdateHunt(deltaTime, cTrans);
        }
        break;

    default:
        break;
    }

    if (m_doorPassThroughTimer > 0.0f)
    {
        m_doorPassThroughTimer -= deltaTime;
    }

    // 階段の段差(蹴上げ)に当たらないよう、壁抜けは実際に昇り降りしている間だけ許可する
    // (以前は階段の周り1.5mに入っただけで許可していたため、階段の横の壁をすり抜けていた)
    bool wantPassThrough = (m_doorPassThroughTimer > 0.0f) || m_stairsCrossingLatched;
    if (wantPassThrough != m_wasPassThroughActive)
    {
        SetDoorPassThrough(wantPassThrough);
        m_wasPassThroughActive = wantPassThrough;
    }

    //---重力(どのステートでも常に適用) ---
    // Skipped while GetStairsCrossingMove just set height directly from Z-progress along the stairs
    // (see its own comment) - letting gravity/the ground raycast run afterward would immediately
    // override that with a raycast against individual step treads again, which is what caused the
    // visible jitter/bouncing this was added to fix. Also zero out velocityY so it doesn't quietly
    // accumulate downward speed the whole time and cause a sudden drop the moment gravity resumes.
    // While still inside the stairs' padded area right after a crossing (m_recentlyExitedStairs),
    // hold height at the known exit landing instead of trusting the ground raycast: the real
    // staircase mesh has actual 3D height (unlike the flat RoomArea abstraction), so if the normal
    // NavMesh walk toward the next destination happens to route back across the stairs' own XZ
    // footprint (e.g. LivingRoom's polygon geometrically contains the stairs, so a straight line
    // through LivingRoom can pass directly over them), gravity/the ground raycast would climb the
    // ghost right back up the real risers as a side effect of just walking past - which then reads
    // as "back on the upper floor" and re-triggers a whole new crossing, producing an endless
    // climb-descend-climb loop even though the ghost was never meant to be crossing again.
    bool holdAtExitLanding = m_recentlyExitedStairs && IsInStairsArea(cTrans.m_position, m_stairsNavExitPadding);
    if (!m_skipGravityThisFrame && !holdAtExitLanding)
    {
        m_velocityY -= m_gravityStrength * deltaTime;
        cTrans.m_position.y += m_velocityY * deltaTime;

        // --- 地面チェック (レイキャスト) ---
        // origin は常に position + 0.5 (固定)
        // groundOffset = モデル原点から何m上にあるか(プラス値)
        Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0);
        Math::Vector3 rayDir(0, -1, 0);
        RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(origin, rayDir, 1000.0f, "Stage");

        // 閾値 = groundOffset(原点から足元までの距離) + 0.5(origin offset) + 0.1(余裕)
        float snapThreshold = m_groundOffset + 0.5f + 0.1f;

        if (hit.hit && hit.distance <= snapThreshold)
        {
            m_isGrounded = true;
            if (m_velocityY < 0.0f)
            {
                m_velocityY = 0.0f;
                // 接地位置に合わせる: 新しいY = origin.y - distance
                // 原点を足元として groundOffset 分上に置く
                cTrans.m_position.y = (origin.y - hit.distance) + m_groundOffset;
            }
        }
        else
        {
            m_isGrounded = false;
        }
    }
    else
    {
        m_velocityY = 0.0f;
        m_isGrounded = true;
        if (holdAtExitLanding)
        {
            cTrans.m_position.y = m_frozenExitLandingY + m_groundOffset;
        }
    }
    m_skipGravityThisFrame = false;
}

void GhostAI::PostUpdate()
{
}

void GhostAI::PreDraw()
{
}

void GhostAI::OnDestroy()
{
    // If the ghost is destroyed (e.g. scene change) while mid-Hunt, it never goes through
    // SetState's normal Hunt-exit path, which would otherwise leave the fog stuck dense.
    if (m_currentState == State::Hunt)
    {
        ShaderManager::Instance().SetHuntActive(false);
    }
    DoorLock::UnlockAll();
    // シーンを抜けた後に古いゴーストタイプが表示され続けないようにする
    Logger::Instance().RemovePinnedLine("Ghost Type");
}

void GhostAI::Draw()
{
    if (!m_showPathDebug)
    {
        return;
    }
    if (m_currentState != State::HouseWander && m_currentState != State::Hunt && m_currentState != State::Lured)
    {
        return;
    }

    auto& ecs = GameManager::Instance().GetECS();
    auto* pTrans = ecs.TryGetComponent<TransformData>(GetGameObject()->GetEntityID());
    if (!pTrans)
    {
        return;
    }
    Math::Vector3 pos = pTrans->m_position;

    auto& cm = CollisionManager::Instance();
    const ImU32 colorFinalTarget = IM_COL32(255, 60, 60, 255); // red: ultimate destination (room center / player position)
    const ImU32 colorMoveTarget = IM_COL32(255, 200, 0, 255);  // orange: GetEffectiveMoveTarget's actual NavMesh target (may be a stairs waypoint instead of the real destination)
    const ImU32 colorPath = IM_COL32(0, 220, 255, 255);        // cyan: the currently cached path waypoints being followed

    auto drawMarker = [&](const Math::Vector3& p, ImU32 color)
    {
        cm.AddDebugLineAlways(p, p + Math::Vector3(0, 1.0f, 0), color);
    };

    drawMarker(m_debugFinalTarget, colorFinalTarget);
    cm.AddDebugLineAlways(pos + Math::Vector3(0, 0.1f, 0), m_debugFinalTarget + Math::Vector3(0, 0.1f, 0), colorFinalTarget);

    if ((m_debugMoveTarget - m_debugFinalTarget).LengthSquared() > 0.01f)
    {
        drawMarker(m_debugMoveTarget, colorMoveTarget);
        cm.AddDebugLineAlways(pos + Math::Vector3(0, 0.3f, 0), m_debugMoveTarget + Math::Vector3(0, 0.3f, 0), colorMoveTarget);
    }

    if (const auto* path = NavMeshManager::Instance().GetCachedPath((int)GetGameObject()->GetEntityID()))
    {
        Math::Vector3 prev = pos;
        for (const auto& wp : *path)
        {
            cm.AddDebugLineAlways(prev + Math::Vector3(0, 0.5f, 0), wp + Math::Vector3(0, 0.5f, 0), colorPath);
            drawMarker(wp, colorPath);
            prev = wp;
        }
    }
}

void GhostAI::Serialize(nlohmann::json& out) const
{
    out["moveSpeedKmh"] = m_moveSpeedKmh;
    out["animIdle"] = m_animIdle;
    out["animWander"] = m_animWander;
    out["animHunt"] = m_animHunt;
    out["animDead"] = m_animDead;
    out["groundOffset"] = m_groundOffset;
    out["houseBoundsMargin"] = m_houseBoundsMargin;
    out["houseWanderIntervalMin"] = m_houseWanderIntervalMin;
    out["houseWanderIntervalMax"] = m_houseWanderIntervalMax;
    out["houseWanderDurationMin"] = m_houseWanderDurationMin;
    out["houseWanderDurationMax"] = m_houseWanderDurationMax;
    out["roomArriveThreshold"] = m_roomArriveThreshold;
    out["houseWanderStuckTimeout"] = m_houseWanderStuckTimeout;
    out["huntDurationMin"] = m_huntDurationMin;
    out["huntDurationMax"] = m_huntDurationMax;
    out["huntSpeedKmh"] = m_huntSpeedKmh;
    out["huntMaxSpeedKmh"] = m_huntMaxSpeedKmh;
    out["losTimeToMax"] = m_losTimeToMax;
    out["losDecayTime"] = m_losDecayTime;
    out["litSpeedMultiplier"] = m_litSpeedMultiplier;
    out["huntTriggerIntervalMin"] = m_huntTriggerIntervalMin;
    out["huntTriggerIntervalMax"] = m_huntTriggerIntervalMax;
    out["visionRange"] = m_visionRange;
    out["visionAngle"] = m_visionAngle;
    out["proximitySenseDistance"] = m_proximitySenseDistance;
    out["trackGiveUpTime"] = m_trackGiveUpTime;
    out["entranceDoorName"] = m_entranceDoorName;
    out["lightFlickerRange"] = m_lightFlickerRange;
    out["lightFlickerVerticalRange"] = m_lightFlickerVerticalRange;
    out["wanderFlickerChance"] = m_wanderFlickerChance;
    out["eyeHeight"] = m_eyeHeight;
    out["searchGiveUpDistance"] = m_searchGiveUpDistance;
    out["doorCheckInterval"] = m_doorCheckInterval;
    out["doorOpenRange"] = m_doorOpenRange;
    out["doorSoundVolume"] = m_doorSoundVolume;
    out["pathNodeReachThreshold"] = m_pathNodeReachThreshold;
    out["ghostType"] = static_cast<int>(m_ghostType);
    out["randomizeType"] = m_randomizeType;
    out["lureCheckInterval"] = m_lureCheckInterval;
    out["lureArriveThreshold"] = m_lureArriveThreshold;
    out["lureDwellDuration"] = m_lureDwellDuration;
    out["lureCooldown"] = m_lureCooldown;
    out["failHuntExtension"] = m_failHuntExtension;
    out["exorciseDuration"] = m_exorciseDuration;
    out["exorciseModelHeight"] = m_exorciseModelHeight;
    out["exorciseEdgeWidth"] = m_exorciseEdgeWidth;
    out["exorciseEdgeGlow"] = m_exorciseEdgeGlow;
    out["exorciseSoundVolume"] = m_exorciseSoundVolume;
    out["exorciseGlowStrength"] = m_exorciseGlowStrength;
    out["exorciseLightIntensity"] = m_exorciseLightIntensity;
    out["exorciseParticleRate"] = m_exorciseParticleRate;
    out["exorciseParticleRadius"] = m_exorciseParticleRadius;
    out["exorciseParticleBurst"] = m_exorciseParticleBurst;
    out["flashLightFlickerRange"] = m_flashLightFlickerRange;
    out["kageonnaLightRadius"] = m_kageonnaLightRadius;
    out["tellIntervalMin"] = m_tellIntervalMin;
    out["tellIntervalMax"] = m_tellIntervalMax;
    out["tellRange"] = m_tellRange;
}

void GhostAI::Deserialize(const nlohmann::json& in)
{
    // 旧形式(m/s・ハント倍率)で保存されたシーンはkm/hに換算して読む
    float legacyMoveSpeedMs = 1.0f;
    if (in.contains("moveSpeed"))
    {
        legacyMoveSpeedMs = in["moveSpeed"];
        m_moveSpeedKmh = legacyMoveSpeedMs * 3.6f;
    }
    if (in.contains("moveSpeedKmh"))
    {
        m_moveSpeedKmh = in["moveSpeedKmh"];
    }
    if (in.contains("animIdle"))
    {
        m_animIdle = in["animIdle"];
    }
    if (in.contains("animWander"))
    {
        m_animWander = in["animWander"];
    }
    if (in.contains("animHunt"))
    {
        m_animHunt = in["animHunt"];
    }
    if (in.contains("animDead"))
    {
        m_animDead = in["animDead"];
    }
    if (in.contains("groundOffset"))
    {
        m_groundOffset = in["groundOffset"];
    }
    if (in.contains("houseBoundsMargin"))
    {
        m_houseBoundsMargin = in["houseBoundsMargin"];
    }
    if (in.contains("houseWanderIntervalMin"))
    {
        m_houseWanderIntervalMin = in["houseWanderIntervalMin"];
    }
    if (in.contains("houseWanderIntervalMax"))
    {
        m_houseWanderIntervalMax = in["houseWanderIntervalMax"];
    }
    if (in.contains("houseWanderDurationMin"))
    {
        m_houseWanderDurationMin = in["houseWanderDurationMin"];
    }
    if (in.contains("houseWanderDurationMax"))
    {
        m_houseWanderDurationMax = in["houseWanderDurationMax"];
    }
    if (in.contains("roomArriveThreshold"))
    {
        m_roomArriveThreshold = in["roomArriveThreshold"];
    }
    if (in.contains("houseWanderStuckTimeout"))
    {
        m_houseWanderStuckTimeout = in["houseWanderStuckTimeout"];
    }
    if (in.contains("huntDurationMin"))
    {
        m_huntDurationMin = in["huntDurationMin"];
    }
    if (in.contains("huntDurationMax"))
    {
        m_huntDurationMax = in["huntDurationMax"];
    }
    if (in.contains("huntSpeedMultiplier"))
    {
        m_huntSpeedKmh = legacyMoveSpeedMs * in["huntSpeedMultiplier"].get<float>() * 3.6f;
    }
    if (in.contains("huntSpeedKmh"))
    {
        m_huntSpeedKmh = in["huntSpeedKmh"];
    }
    if (in.contains("huntMaxSpeedKmh"))
    {
        m_huntMaxSpeedKmh = in["huntMaxSpeedKmh"];
    }
    if (in.contains("losTimeToMax"))
    {
        m_losTimeToMax = in["losTimeToMax"];
    }
    if (in.contains("losDecayTime"))
    {
        m_losDecayTime = in["losDecayTime"];
    }
    if (in.contains("litSpeedMultiplier"))
    {
        m_litSpeedMultiplier = in["litSpeedMultiplier"];
    }
    if (in.contains("huntTriggerIntervalMin"))
    {
        m_huntTriggerIntervalMin = in["huntTriggerIntervalMin"];
    }
    if (in.contains("huntTriggerIntervalMax"))
    {
        m_huntTriggerIntervalMax = in["huntTriggerIntervalMax"];
    }
    if (in.contains("visionRange"))
    {
        m_visionRange = in["visionRange"];
    }
    if (in.contains("visionAngle"))
    {
        m_visionAngle = in["visionAngle"];
    }
    if (in.contains("proximitySenseDistance"))
    {
        m_proximitySenseDistance = in["proximitySenseDistance"];
    }
    if (in.contains("trackGiveUpTime"))
    {
        m_trackGiveUpTime = in["trackGiveUpTime"];
    }
    if (in.contains("entranceDoorName"))
    {
        m_entranceDoorName = in["entranceDoorName"].get<std::string>();
    }
    if (in.contains("lightFlickerRange"))
    {
        m_lightFlickerRange = in["lightFlickerRange"];
    }
    if (in.contains("lightFlickerVerticalRange"))
    {
        m_lightFlickerVerticalRange = in["lightFlickerVerticalRange"];
    }
    if (in.contains("wanderFlickerChance"))
    {
        m_wanderFlickerChance = in["wanderFlickerChance"];
    }
    if (in.contains("eyeHeight"))
    {
        m_eyeHeight = in["eyeHeight"];
    }
    if (in.contains("searchGiveUpDistance"))
    {
        m_searchGiveUpDistance = in["searchGiveUpDistance"];
    }
    if (in.contains("doorCheckInterval"))
    {
        m_doorCheckInterval = in["doorCheckInterval"];
    }
    if (in.contains("doorOpenRange"))
    {
        m_doorOpenRange = in["doorOpenRange"];
    }
    if (in.contains("doorSoundVolume"))
    {
        m_doorSoundVolume = in["doorSoundVolume"];
    }
    if (in.contains("pathNodeReachThreshold"))
    {
        m_pathNodeReachThreshold = in["pathNodeReachThreshold"];
    }
    if (in.contains("ghostType"))
    {
        m_ghostType = static_cast<GhostType>((int)in["ghostType"]);
    }
    if (in.contains("randomizeType"))
    {
        m_randomizeType = in["randomizeType"];
    }
    if (in.contains("lureCheckInterval"))
    {
        m_lureCheckInterval = in["lureCheckInterval"];
    }
    if (in.contains("lureArriveThreshold"))
    {
        m_lureArriveThreshold = in["lureArriveThreshold"];
    }
    if (in.contains("lureDwellDuration"))
    {
        m_lureDwellDuration = in["lureDwellDuration"];
    }
    if (in.contains("lureCooldown"))
    {
        m_lureCooldown = in["lureCooldown"];
    }
    if (in.contains("failHuntExtension"))
    {
        m_failHuntExtension = in["failHuntExtension"];
    }
    if (in.contains("exorciseDuration"))
    {
        m_exorciseDuration = in["exorciseDuration"];
    }
    if (in.contains("exorciseModelHeight"))
    {
        m_exorciseModelHeight = in["exorciseModelHeight"];
    }
    if (in.contains("exorciseEdgeWidth"))
    {
        m_exorciseEdgeWidth = in["exorciseEdgeWidth"];
    }
    if (in.contains("exorciseEdgeGlow"))
    {
        m_exorciseEdgeGlow = in["exorciseEdgeGlow"];
    }
    if (in.contains("exorciseSoundVolume"))
    {
        m_exorciseSoundVolume = in["exorciseSoundVolume"];
    }
    if (in.contains("exorciseGlowStrength"))
    {
        m_exorciseGlowStrength = in["exorciseGlowStrength"];
    }
    if (in.contains("exorciseLightIntensity"))
    {
        m_exorciseLightIntensity = in["exorciseLightIntensity"];
    }
    if (in.contains("exorciseParticleRate"))
    {
        m_exorciseParticleRate = in["exorciseParticleRate"];
    }
    if (in.contains("exorciseParticleRadius"))
    {
        m_exorciseParticleRadius = in["exorciseParticleRadius"];
    }
    if (in.contains("exorciseParticleBurst"))
    {
        m_exorciseParticleBurst = in["exorciseParticleBurst"];
    }
    if (in.contains("flashLightFlickerRange"))
    {
        m_flashLightFlickerRange = in["flashLightFlickerRange"];
    }
    // 旧名(鬼火だった頃)のキーも読む
    if (in.contains("onibiDarknessRadius"))
    {
        m_kageonnaLightRadius = in["onibiDarknessRadius"];
    }
    if (in.contains("onibiLightRadius"))
    {
        m_kageonnaLightRadius = in["onibiLightRadius"];
    }
    if (in.contains("kageonnaLightRadius"))
    {
        m_kageonnaLightRadius = in["kageonnaLightRadius"];
    }
    if (in.contains("tellIntervalMin"))
    {
        m_tellIntervalMin = in["tellIntervalMin"];
    }
    if (in.contains("tellIntervalMax"))
    {
        m_tellIntervalMax = in["tellIntervalMax"];
    }
    if (in.contains("tellRange"))
    {
        m_tellRange = in["tellRange"];
    }
}

void GhostAI::ImGuiUpdate()
{
    ImGui::DragFloat("Move Speed (km/h)", &m_moveSpeedKmh, 0.1f, 0.1f, 40.0f, "%.1f km/h");
    ImGui::DragFloat("Ground Offset", &m_groundOffset, 0.01f, -2.0f, 2.0f);
    ImGui::Separator();
    ImGui::Text("Ghost Type");
    {
        int current = static_cast<int>(m_ghostType);
        const char* names[] = {"Jibakurei (Normal)", "Kageonna (Light)", "Yanari (Sound, not implemented)"};
        if (ImGui::Combo("Type", &current, names, IM_ARRAYSIZE(names)))
        {
            m_ghostType = static_cast<GhostType>(current);
            Logger::Instance().SetPinnedLine("Ghost Type", GetGhostTypeName(m_ghostType));
        }
    }
    ImGui::Checkbox("Randomize Type On Start", &m_randomizeType);
    ImGui::DragFloat("Lure Arrive Threshold", &m_lureArriveThreshold, 0.1f, 0.1f, 5.0f);
    ImGui::DragFloat("Lure Dwell Duration", &m_lureDwellDuration, 0.5f, 0.0f, 120.0f);
    ImGui::DragFloat("Lure Cooldown", &m_lureCooldown, 0.5f, 0.0f, 120.0f);
    ImGui::DragFloat("Lure Check Interval", &m_lureCheckInterval, 0.05f, 0.05f, 5.0f);
    ImGui::DragFloat("Fail Hunt Extension", &m_failHuntExtension, 0.5f, 0.0f, 60.0f);
    ImGui::DragFloat("Exorcise Duration", &m_exorciseDuration, 0.1f, 0.1f, 10.0f, "%.1f s");
    ImGui::DragFloat("Exorcise Model Height", &m_exorciseModelHeight, 0.05f, 0.1f, 10.0f, "%.2f m");
    ImGui::DragFloat("Exorcise Edge Width", &m_exorciseEdgeWidth, 0.01f, 0.01f, 1.0f, "%.2f m");
    ImGui::DragFloat("Exorcise Edge Glow", &m_exorciseEdgeGlow, 0.1f, 0.0f, 50.0f);
    ImGui::SliderFloat("Exorcise Sound Volume", &m_exorciseSoundVolume, 0.0f, 1.0f);
    ImGui::DragFloat("Exorcise Glow Strength", &m_exorciseGlowStrength, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Exorcise Light Intensity", &m_exorciseLightIntensity, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Exorcise Particle Rate", &m_exorciseParticleRate, 1.0f, 0.0f, 500.0f, "%.0f /s");
    ImGui::DragFloat("Exorcise Particle Radius", &m_exorciseParticleRadius, 0.05f, 0.0f, 5.0f, "%.2f m");
    ImGui::DragInt("Exorcise Particle Burst", &m_exorciseParticleBurst, 1.0f, 0, 300);
    ImGui::DragFloat("FlashLight Flicker Range", &m_flashLightFlickerRange, 0.1f, 0.0f, 30.0f, "%.1f m");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"この距離以内の点いたフラッシュライト(手持ち・床)を点滅させる(消しはしない)");
    }
    ImGui::DragFloat("Kageonna Light Radius", &m_kageonnaLightRadius, 0.5f, 0.0f, 50.0f);
    ImGui::DragFloat("Tell Interval Min", &m_tellIntervalMin, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Tell Interval Max", &m_tellIntervalMax, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Tell Range", &m_tellRange, 0.1f, 0.5f, 50.0f);
    ImGui::Separator();
    ImGui::Text("House Bounds");
    ImGui::DragFloat("Bounds Margin", &m_houseBoundsMargin, 0.1f, 0.0f, 20.0f);
    if (ImGui::Button("Recompute Bounds"))
    {
        m_houseBoundsValid = false;
        EnsureHouseBounds();
    }
    if (m_houseBoundsValid)
    {
        ImGui::Text("Min: (%.1f, %.1f, %.1f)", m_houseBoundsMin.x, m_houseBoundsMin.y, m_houseBoundsMin.z);
        ImGui::Text("Max: (%.1f, %.1f, %.1f)", m_houseBoundsMax.x, m_houseBoundsMax.y, m_houseBoundsMax.z);
    }
    else
    {
        ImGui::TextDisabled("(not computed - no RoomArea found)");
    }
    ImGui::Separator();
    ImGui::Text("House Wander");
    ImGui::DragFloat("Wander Interval Min", &m_houseWanderIntervalMin, 0.5f, 0.0f, 300.0f);
    ImGui::DragFloat("Wander Interval Max", &m_houseWanderIntervalMax, 0.5f, 0.0f, 300.0f);
    ImGui::DragFloat("Wander Duration Min", &m_houseWanderDurationMin, 0.5f, 0.0f, 120.0f);
    ImGui::DragFloat("Wander Duration Max", &m_houseWanderDurationMax, 0.5f, 0.0f, 120.0f);
    ImGui::DragFloat("Room Arrive Threshold", &m_roomArriveThreshold, 0.1f, 0.1f, 5.0f);
    ImGui::DragFloat("Stuck Timeout", &m_houseWanderStuckTimeout, 0.5f, 1.0f, 30.0f);
    ImGui::Separator();
    ImGui::Text("Hunt");
    ImGui::DragFloat("Hunt Duration Min", &m_huntDurationMin, 0.5f, 0.0f, 60.0f);
    ImGui::DragFloat("Hunt Duration Max", &m_huntDurationMax, 0.5f, 0.0f, 60.0f);
    ImGui::DragFloat("Hunt Speed (km/h)", &m_huntSpeedKmh, 0.1f, 0.1f, 40.0f, "%.1f km/h");
    ImGui::DragFloat("Hunt Max Speed (LoS, km/h)", &m_huntMaxSpeedKmh, 0.1f, 0.1f, 40.0f, "%.1f km/h");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"Playerの全身を見続けた時に到達する最高速度(視認加速)");
    }
    ImGui::DragFloat("LoS Time To Max (s)", &m_losTimeToMax, 0.1f, 0.1f, 30.0f, "%.1f s");
    ImGui::DragFloat("LoS Decay Time (s)", &m_losDecayTime, 0.1f, 0.0f, 30.0f, "%.1f s");
    ImGui::DragFloat("Lit Speed Multiplier", &m_litSpeedMultiplier, 0.01f, 0.0f, 1.0f, "x%.2f");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"ハント中、照明に照らされている間の速度倍率");
    }
    if (m_currentState == State::Hunt)
    {
        ImGui::Text("[Debug] Hunt speed: %.1f km/h  LoS %.1f/%.1f s  FullBody=%s  InLight=%s",
                    GetHuntSpeed() / kKmhToMs, m_losTimer, m_losTimeToMax,
                    m_isSeeingFullBody ? "yes" : "no", m_isInLight ? "yes" : "no");
    }
    ImGui::DragFloat("Hunt Trigger Interval Min", &m_huntTriggerIntervalMin, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Hunt Trigger Interval Max", &m_huntTriggerIntervalMax, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Path Node Reach Threshold", &m_pathNodeReachThreshold, 0.02f, 0.05f, 1.0f);
    ImGui::Checkbox("Show Path Debug (red=destination, orange=NavMesh target, cyan=path)", &m_showPathDebug);
    ImGui::Separator();
    ImGui::Text("Door");
    ImGui::DragFloat("Door Open Range", &m_doorOpenRange, 0.1f, 0.5f, 10.0f);
    ImGui::SliderFloat("Door Sound Volume", &m_doorSoundVolume, 0.0f, 1.0f);
    ImGui::DragFloat("Door Check Interval", &m_doorCheckInterval, 0.05f, 0.05f, 2.0f);
    {
        char buf[128] = {};
        strncpy_s(buf, m_entranceDoorName.c_str(), _TRUNCATE);
        if (ImGui::InputText("Entrance Door (locked in Hunt)", buf, sizeof(buf)))
        {
            m_entranceDoorName = buf;
        }
    }
    ImGui::Text("Light Flicker");
    ImGui::DragFloat("Flicker Range", &m_lightFlickerRange, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("Flicker Vertical Range", &m_lightFlickerVerticalRange, 0.1f, 0.0f, 10.0f);
    ImGui::SliderFloat("Wander Flicker Chance (/sec)", &m_wanderFlickerChance, 0.0f, 1.0f);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"ハント以外で近くの照明が点滅し始める1秒あたりの確率。ハント中は常に点滅");
    }
    ImGui::Separator();
    ImGui::Text("Vision (Hunt tracking)");
    ImGui::DragFloat("Vision Range", &m_visionRange, 0.5f, 0.0f, 50.0f);
    ImGui::DragFloat("Vision Angle", &m_visionAngle, 1.0f, 0.0f, 360.0f);
    ImGui::DragFloat("Proximity Sense Dist", &m_proximitySenseDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("Track Give Up Time (s)", &m_trackGiveUpTime, 0.1f, 0.0f, 60.0f, "%.1f s");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"一度視認したら追い続ける。視認加速が入らない(全身が見えていない)状態がこの秒数続いたら追跡をやめる");
    }
    ImGui::DragFloat("Eye Height", &m_eyeHeight, 0.05f, 0.0f, 3.0f);
    ImGui::DragFloat("Search GiveUp Dist", &m_searchGiveUpDistance, 0.1f, 0.1f, 10.0f);
    ImGui::Text("State: %s", m_currentState == State::Idle ? "Idle" : m_currentState == State::Wander    ? "Wander (Room)"
                                                                  : m_currentState == State::HouseWander ? "HouseWander"
                                                                  : m_currentState == State::Hunt        ? "Hunt"
                                                                  : m_currentState == State::Stun        ? "Stun"
                                                                  : m_currentState == State::Lured       ? (m_lureArrived ? "Lured (dwelling)" : "Lured (moving)")
                                                                                                         : "Dead");
    ImGui::Text("NavMesh Built: %s", NavMeshManager::Instance().IsBuilt() ? "true" : "false");
    {
        auto gs = GameSequence::GetInstance();
        int roomCount = gs ? (int)gs->GetRooms().size() : 0;
        ImGui::Text("GameSequence Rooms: %d (0 means GameSequence/RoomArea not ready yet)", roomCount);
    }
    if (m_currentState == State::Wander)
    {
        ImGui::Text("Time until next HouseWander: %.1f s", m_houseWanderTimer);
        ImGui::Text("Time until next Hunt: %.1f s", m_huntTriggerTimer);
    }
    else if (m_currentState == State::HouseWander)
    {
        ImGui::Text("HouseWander returning home: %s", m_houseWanderReturning ? "true" : "false");
        ImGui::Text("Time left roaming outside: %.1f s", m_houseWanderDurationTimer);
        ImGui::Text("Stuck timer: %.1f / %.1f s", m_houseWanderStuckTimer, m_houseWanderStuckTimeout);
        ImGui::Text("Target: (%.1f, %.1f, %.1f)", m_houseWanderTargetPos.x, m_houseWanderTargetPos.y, m_houseWanderTargetPos.z);
        ImGui::Text("Time until next Hunt: %.1f s", m_huntTriggerTimer);
    }
    else if (m_currentState == State::Hunt)
    {
        ImGui::Text("Hunt time left: %.1f s", m_huntTimer);
        ImGui::Text("Hunt mode: %s", m_isSeeingPlayer ? "Chasing (seeing player)" : m_isSensingLight      ? "Chasing (flashlight)"
                                                                                : m_isTrackingPlayer      ? "Chasing (tracking)"
                                                                                : m_hasLastKnownPlayerPos ? "Going to last known pos"
                                                                                                          : "Searching rooms");
    }
}

void GhostAI::OnCollisionEnter(GameObject* other)
{
    if (m_isExorcised || m_currentState == GhostAI::State::Stun)
    {
        return;
    }

    // PlayerはECSの独立したComponent型としては登録されていないため、
    // TryGetComponent<Player>()は使えない(未登録のコンポーネント型はassertで落ちる)。
    // NativeScriptData経由でdynamic_castするか、名前で判定する。
    auto& ecs = GameManager::Instance().GetECS();
    bool isPlayer = (other->GetName() == "Player");
    if (!isPlayer)
    {
        if (auto* pScriptData = ecs.TryGetComponent<NativeScriptData>(other->GetEntityID()))
        {
            isPlayer = (dynamic_cast<Player*>(pScriptData->Instance.get()) != nullptr);
        }
    }

    if (isPlayer)
    {
        if (auto gs = GameSequence::GetInstance())
        {
            gs->NotifyGameOver();
        }
    }
}

void GhostAI::OnCollisionStay(GameObject* other)
{
}

void GhostAI::Exorcise()
{
    if (m_isExorcised)
    {
        return;
    }

    m_isExorcised = true;
    SetState(GhostAI::State::Dead);

    // 除霊演出の開始。クリア通知は演出が終わってから(UpdateExorciseEffect)
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());
    m_exorciseTimer = 0.0f;
    m_exorciseNotified = false;
    m_exorciseStartPos = cTrans.m_position;
    m_exorciseCutY = cTrans.m_position.y;
    // 札の「パシッ」+りんの「チーン」+消えていく間のきらめき(演出の長さに合わせた1本のSE)
    AudioManager::Instance().PlaySE("Asset/Sound/SE/exorcise.wav", m_exorciseSoundVolume);
    m_exorciseParticleAccum = 0.0f;
    for (int i = 0; i < m_exorciseParticleBurst; ++i)
    {
        EmitExorciseParticle(true);
    }

    // 閃光用の点光源(実行中だけの一時オブジェクト)
    if (auto* scene = GetGameObject()->GetScene())
    {
        m_exorciseLightObj = scene->CreateGameObject("ExorciseLight");
        auto& lTrans = m_exorciseLightObj->GetComponent<TransformData>();
        lTrans.m_position = cTrans.m_position + Math::Vector3(0, m_eyeHeight * 0.8f, 0);
        PointLightData light;
        light.m_color = m_exorciseColor;
        light.m_intensity = 0.0f;
        light.m_range = m_exorciseLightRange;
        m_exorciseLightObj->AddComponent<PointLightData>(light);
    }
}

void GhostAI::UpdateExorciseEffect(float deltaTime, TransformData& cTrans)
{
    if (m_exorciseNotified)
    {
        return;
    }

    m_exorciseTimer += deltaTime;
    float t = (m_exorciseDuration > 0.0f) ? std::clamp(m_exorciseTimer / m_exorciseDuration, 0.0f, 1.0f) : 1.0f;

    // 札を貼った瞬間(最初の15%)に一気に強く光る。その後は体の光を少し落として、
    // 足元から消えていく境界の光が目立つようにする
    constexpr float kFlashEnd = 0.15f;
    float flash = (t < kFlashEnd) ? (t / kFlashEnd) : 1.0f;
    // 閃光の直後に一瞬だけ跳ね上がるピーク(sinの山)
    float burst = (t < kFlashEnd * 2.0f) ? sinf(t / (kFlashEnd * 2.0f) * DirectX::XM_PI) : 0.0f;
    // 浄化の光が揺らめく感じ
    float shimmer = 0.85f + 0.15f * sinf(m_exorciseTimer * 25.0f);

    // 足元から消えていく: 閃光の後(15%〜90%)で境界を足元から頭の上まで上げる。
    // その場から動かさない(位置・向き・大きさはそのまま)。
    constexpr float kDissolveStart = 0.15f;
    constexpr float kDissolveEnd = 0.9f;
    float d = std::clamp((t - kDissolveStart) / (kDissolveEnd - kDissolveStart), 0.0f, 1.0f);
    d = d * d * (3.0f - 2.0f * d); // ゆっくり始まりゆっくり終わる
    float bottomY = m_exorciseStartPos.y - m_exorciseEdgeWidth;
    float topY = m_exorciseStartPos.y + m_exorciseModelHeight + m_exorciseEdgeWidth;
    m_exorciseCutY = bottomY + (topY - bottomY) * d;
    cTrans.m_position = m_exorciseStartPos;
    float remain = 1.0f - d; // 残っている体の割合(光の強さを合わせて落とす)

    // 消えている境界から光の粒を湧かせる(消えている間だけ)
    if (t >= kDissolveStart && t < kDissolveEnd)
    {
        m_exorciseParticleAccum += m_exorciseParticleRate * deltaTime;
        while (m_exorciseParticleAccum >= 1.0f)
        {
            EmitExorciseParticle(false);
            m_exorciseParticleAccum -= 1.0f;
        }
    }

    // ゴースト自体の発光(Bloom)
    if (m_animEntity != INVALID_ENTITY)
    {
        auto& ecs = GameManager::Instance().GetECS();
        if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_animEntity))
        {
            float glow = m_exorciseGlowStrength * (0.35f * flash + burst) * shimmer;
            pModel->m_glowColor = m_exorciseColor * glow;
            pModel->m_glowMaterial.clear();
            pModel->m_dissolveEnabled = true;
            pModel->m_dissolveHeight = m_exorciseCutY;
            pModel->m_dissolveEdgeWidth = m_exorciseEdgeWidth;
            pModel->m_dissolveEdgeColor = m_exorciseColor * (m_exorciseEdgeGlow * shimmer);
        }
    }

    // 周囲を照らす閃光
    if (m_exorciseLightObj)
    {
        auto& ecs = GameManager::Instance().GetECS();
        if (auto* pLight = ecs.TryGetComponent<PointLightData>(m_exorciseLightObj->GetEntityID()))
        {
            // 閃光のピークは最大強度、その後は4割程度の光を保ち、最後に消える
            pLight->m_intensity = m_exorciseLightIntensity * (0.4f * flash * remain + 0.6f * burst) * shimmer;
        }
    }

    if (t >= 1.0f)
    {
        SetModelVisible(false);
        if (m_exorciseLightObj)
        {
            m_exorciseLightObj->Destroy();
            m_exorciseLightObj.reset();
        }
        m_exorciseNotified = true;
        if (auto gs = GameSequence::GetInstance())
        {
            gs->NotifyExorcised();
        }
    }
}

void GhostAI::EmitExorciseParticle(bool burst)
{
    auto& rnd = Random::Instance();

    // 足元の円内のランダムな位置(中心寄りに偏らないようsqrtで半径を取る)
    float angle = rnd.Range(0.0f, DirectX::XM_2PI);
    float radius = m_exorciseParticleRadius * sqrtf(rnd.Range(0.0f, 1.0f));
    Math::Vector3 offset(cosf(angle) * radius, 0.05f, sinf(angle) * radius);

    Particle p;
    p.m_position = m_exorciseStartPos + offset;
    if (!burst)
    {
        p.m_position.y = m_exorciseCutY; // 消えている境界から出す
    }
    if (burst)
    {
        // 札を貼った瞬間: 体の高さあたりから外へ弾けて、ふわっと上へ
        p.m_position.y += rnd.Range(0.3f, m_eyeHeight);
        Math::Vector3 outward(cosf(angle), 0.0f, sinf(angle));
        p.m_velocity = outward * rnd.Range(1.0f, 2.5f) + Math::Vector3(0, rnd.Range(0.8f, 2.0f), 0);
        p.m_drag = 2.5f;
        p.m_life = rnd.Range(0.8f, 1.4f);
        p.m_sizeStart = rnd.Range(0.03f, 0.06f);
    }
    else
    {
        // 消えている境界から湧き上がって、昇りながら小さくなって消えていく
        p.m_velocity = Math::Vector3(0, rnd.Range(0.8f, 1.8f), 0) - offset * 0.3f; // 少しだけ中心へ寄る
        p.m_drag = 0.3f;
        p.m_life = rnd.Range(1.0f, 1.8f);
        p.m_sizeStart = rnd.Range(0.02f, 0.05f);
        p.m_swayAmplitude = rnd.Range(0.03f, 0.1f);
        p.m_swayFrequency = rnd.Range(2.0f, 5.0f);
        p.m_swayPhase = rnd.Range(0.0f, DirectX::XM_2PI);
    }
    p.m_sizeEnd = 0.0f;
    // 金色と白の間でばらつかせる(HDRなのでBloomで光る)
    float whiteness = rnd.Range(0.0f, 1.0f);
    Math::Vector3 color = m_exorciseColor + (Math::Vector3(1, 1, 1) - m_exorciseColor) * whiteness;
    p.m_color = color * rnd.Range(4.0f, 8.0f);

    ParticleManager::Instance().Emit(p);
}
