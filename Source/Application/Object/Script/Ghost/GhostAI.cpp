#include "../../../../Pch.h"
#include "GhostAI.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../System/GameSequence.h"
#include "../Player/Player.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../../Framework/Manager/NavMesh/NavMeshManager.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include <functional>

REGISTER_COMPONENT(GhostAI);

void GhostAI::Awake()
{
}

void GhostAI::Start()
{
    m_changeDirTimer = 0.0f;

    // AnimationDataComponent�͎������g�ł͂Ȃ��q��"Model"�I�u�W�F�N�g�ɕt���Ă��邽�߁A
    // �q�K�w��T���ăL���b�V�����Ă���(���t���[���T�����Ȃ��悤)
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> findAnimEntity = [&](GameObject* obj) {
        if (m_animEntity != INVALID_ENTITY) return;
        if (ecs.TryGetComponent<AnimationDataComponent>(obj->GetEntityID())) {
            m_animEntity = obj->GetEntityID();
            return;
        }
        for (auto& child : obj->GetChildren()) {
            findAnimEntity(child.get());
        }
    };
    if (m_pGameObject) {
        findAnimEntity(m_pGameObject);
    }

    m_playerEntity = FindPlayerEntity();
    EnsureHouseBounds();
    EnsureStairsRoom();

    m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
    m_huntTriggerTimer = Random::Instance().Range(m_huntTriggerIntervalMin, m_huntTriggerIntervalMax);
    SetState(GhostAI::State::Wander);
}

Entity GhostAI::FindPlayerEntity()
{
    auto scene = Editor::GetScene();
    if (!scene) return INVALID_ENTITY;

    Entity found = INVALID_ENTITY;
    std::function<void(const std::shared_ptr<GameObject>&)> findPlayer = [&](const std::shared_ptr<GameObject>& obj) {
        if (found != INVALID_ENTITY) return;
        if (obj->GetName() == "Player") {
            found = obj->GetEntityID();
            return;
        }
        for (const auto& child : obj->GetChildren()) findPlayer(child);
    };
    for (auto const& obj : scene->GetGameObjects()) {
        findPlayer(obj);
    }
    return found;
}

void GhostAI::EnsureHouseBounds()
{
    if (m_houseBoundsValid) return;

    auto gs = GameSequence::GetInstance();
    if (!gs) return;

    const auto& rooms = gs->GetRooms();
    if (rooms.empty()) return;

    Math::Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
    Math::Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (RoomArea* room : rooms) {
        if (!room) continue;
        mn = Math::Vector3::Min(mn, room->m_min);
        mx = Math::Vector3::Max(mx, room->m_max);
    }

    // �������m�̌���(�L���Ȃ�)��������悤�ɏ����]�T���������Ă���
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
    if (m_stairsRoom) return;
    auto gs = GameSequence::GetInstance();
    if (!gs) return;
    for (RoomArea* room : gs->GetRooms()) {
        if (room && room->m_isStairs) {
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
    if (!m_houseBoundsValid) return pos;

    Math::Vector3 result = pos;
    result.x = std::clamp(result.x, m_houseBoundsMin.x, m_houseBoundsMax.x);
    result.z = std::clamp(result.z, m_houseBoundsMin.z, m_houseBoundsMax.z);
    return result;
}

void GhostAI::SetModelVisible(bool visible)
{
    if (m_animEntity == INVALID_ENTITY) return;
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_animEntity)) {
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
    if (!m_stairsRoom) return finalTarget;

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
    float topDist     = fabsf(finalTarget.y - m_stairsRoom->m_max.y);
    float exitLandingY = m_isCrossingFloors ? m_frozenExitLandingY
                        : (bottomDist <= topDist) ? m_stairsRoom->m_min.y : m_stairsRoom->m_max.y;

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
    if (m_recentlyExitedStairs && !IsInStairsArea(currentPos, m_stairsNavExitPadding)) {
        m_recentlyExitedStairs = false;
    }
    if (m_isCrossingFloors) {
        float exitGap = fabsf(currentPos.y - exitLandingY);
        if (exitGap <= m_floorHeightThreshold * 0.5f) {
            m_isCrossingFloors = false;
            m_recentlyExitedStairs = true;
        }
    } else if (!m_recentlyExitedStairs) {
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
        if (currentIsUpper != targetIsUpper) {
            m_isCrossingFloors = true;
            m_frozenExitLandingY = exitLandingY;
        }
    }
    if (!m_isCrossingFloors) return finalTarget;

    // GetCenter() sits at the Y midpoint between the stairs' bottom and top landing, which is
    // roughly equidistant from both - findNearestPoly can then snap to either one essentially at
    // random (and inconsistently frame to frame). Break the tie explicitly by aiming for whichever
    // landing is closer to the FINAL destination's floor (the exit side), not the ghost's own
    // current floor (the entry side): picking the entry landing seemed reasonable at first, but
    // it's self-defeating - once the ghost actually arrives there, its current Y *is* that
    // landing's Y, so this would keep re-selecting the same already-reached point forever with
    // nothing to walk toward next, which looked like stopping dead in the middle of the stairs.
    // Aiming for the exit landing instead gives it a reason to walk the entire staircase in one go.
    Math::Vector3 stairsPos = m_stairsRoom->GetCenter();
    stairsPos.y = exitLandingY;
    return stairsPos;
}

bool GhostAI::IsInStairsArea(const Math::Vector3& pos, float padOverride) const
{
    if (!m_stairsRoom) return false;
    const float pad = (padOverride >= 0.0f) ? padOverride : m_stairsPassThroughPadding;
    return pos.x >= m_stairsRoom->m_min.x - pad && pos.x <= m_stairsRoom->m_max.x + pad &&
           pos.z >= m_stairsRoom->m_min.z - pad && pos.z <= m_stairsRoom->m_max.z + pad;
}

// Called once, right when the stairs-crossing latch engages, to fix the start/end points and
// duration of the crossing. See GetStairsCrossingMove's comment for why the crossing no longer
// derives its direction from cTrans.m_position every frame.
void GhostAI::StartStairsCrossing(const Math::Vector3& currentPos, const Math::Vector3& finalTarget, float speed)
{
    if (!m_stairsRoom) return;

    const float kOvershoot = m_stairsNavExitPadding + 0.1f; // must clear IsInStairsArea's nav-exit pad
    bool towardBottom = fabsf(finalTarget.y - m_stairsRoom->m_min.y) <= fabsf(finalTarget.y - m_stairsRoom->m_max.y);
    float targetZ = towardBottom ? (m_stairsRoom->m_min.z - kOvershoot) : (m_stairsRoom->m_max.z + kOvershoot);
    float centerX = (m_stairsRoom->m_min.x + m_stairsRoom->m_max.x) * 0.5f;
    float exitY = towardBottom ? m_stairsRoom->m_min.y : m_stairsRoom->m_max.y;

    m_stairsCrossStart = currentPos;
    m_stairsCrossEnd = Math::Vector3(centerX, exitY, targetZ);
    m_stairsCrossElapsed = 0.0f;
    float dist = Math::Vector3::Distance(m_stairsCrossStart, m_stairsCrossEnd);
    m_stairsCrossDuration = (speed > 0.0001f) ? (dist / speed) : 1.0f;

    Logger::Instance().AddLog(Logger::LogLevel::Info,
        "[GhostAI] StartStairsCrossing: start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f) duration=%.2f",
        m_stairsCrossStart.x, m_stairsCrossStart.y, m_stairsCrossStart.z,
        m_stairsCrossEnd.x, m_stairsCrossEnd.y, m_stairsCrossEnd.z, m_stairsCrossDuration);
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
    float t = (m_stairsCrossDuration > 0.0001f) ? std::clamp(m_stairsCrossElapsed / m_stairsCrossDuration, 0.0f, 1.0f) : 1.0f;
    Math::Vector3 nextPos = Math::Vector3::Lerp(m_stairsCrossStart, m_stairsCrossEnd, t);
    return nextPos;
}

void GhostAI::SetDoorPassThrough(bool enabled)
{
    // The capsule ColliderData lives on a child "Collider" GameObject, not on the Ghost root
    // entity itself (mirrors how m_animEntity is found for AnimationDataComponent in Start()).
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> apply = [&](GameObject* obj) {
        if (auto* pCollider = ecs.TryGetComponent<ColliderData>(obj->GetEntityID())) {
            for (auto& shape : pCollider->m_shapes) {
                if (shape) shape->m_isTrigger = enabled;
            }
        }
        for (auto& child : obj->GetChildren()) {
            apply(child.get());
        }
    };
    if (m_pGameObject) {
        apply(m_pGameObject);
    }
}

void GhostAI::SetState(GhostAI::State state)
{
    // Toggle the fog target density when entering/leaving Hunt (ShaderManager smoothly
    // interpolates the actual density toward it, see ShaderManager::UpdateFogCB).
    if (m_currentState != State::Hunt && state == State::Hunt) {
        ShaderManager::Instance().SetHuntActive(true);
    } else if (m_currentState == State::Hunt && state != State::Hunt) {
        ShaderManager::Instance().SetHuntActive(false);
    }

    m_currentState = state;

    // ��{�I�Ɏp�͔�\���B�v���C���[�ɋC�t�����ׂ����(�n���g�E���S��)�ɂȂ����Ƃ������\������B
    switch (state) {
        case GhostAI::State::Wander:
        case GhostAI::State::HouseWander:
        case GhostAI::State::Idle:
            SetModelVisible(false);
            break;
        case GhostAI::State::Hunt:
        case GhostAI::State::Stun:
        case GhostAI::State::Dead:
            SetModelVisible(true);
            break;
    }

    if (m_animEntity == INVALID_ENTITY) return; // �A�j���[�V�����Ώۂ�������Ȃ��ꍇ�͉������Ȃ�

    switch (state) {
        case GhostAI::State::Idle:
        case GhostAI::State::Stun:
            AnimationManager::Instance().PlayAnimation(m_animEntity, m_animIdle, true);
            break;
        case GhostAI::State::Wander:
        case GhostAI::State::HouseWander:
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
    if (dir.LengthSquared() <= 0.001f) return;

    float targetAngle = atan2f(dir.x, dir.z);

    // �ŒZ�p�x�ŕ�� (Lerp�����360�x�܂����ŕs���R�ɂȂ�̂Œ���)
    float diff = targetAngle - cTrans.m_rotation.y;
    while (diff < -DirectX::XM_PI) diff += DirectX::XM_2PI;
    while (diff >  DirectX::XM_PI) diff -= DirectX::XM_2PI;

    cTrans.m_rotation.y += diff * 10.0f * deltaTime;
}

void GhostAI::UpdateWander(float deltaTime, TransformData& cTrans)
{
    // Room�ҋ@: �S�[�X�g���[��(m_targetRoom)�̒�������������
    m_changeDirTimer -= deltaTime;
    if (m_changeDirTimer <= 0.0f) {
        float randX = Random::Instance().Range(-1.0f, 1.0f);
        float randZ = Random::Instance().Range(-1.0f, 1.0f);
        m_moveDir = Math::Vector3(randX, 0.0f, randZ);
        if (m_moveDir.LengthSquared() > 0.0f)
        {
            m_moveDir.Normalize();
        }
        m_changeDirTimer = Random::Instance().Range(1.0f, 3.0f);
    }

    Math::Vector3 nextPos = cTrans.m_position + m_moveDir * m_moveSpeed * deltaTime;
    if (m_targetRoom == nullptr || m_targetRoom->IsInside(nextPos)) {
        cTrans.m_position = nextPos;
    } else {
        // �����̊O�ɏo�����ɂȂ�����A�����̒��S�֌�������
        Math::Vector3 roomCenter = (m_targetRoom->m_min + m_targetRoom->m_max) * 0.5f;
        m_moveDir = roomCenter - cTrans.m_position;
        m_moveDir.y = 0.0f;
        if (m_moveDir.LengthSquared() > 0.0f) {
            m_moveDir.Normalize();
        } else {
            // ���S�ɂ���ꍇ�̓����_���ȕ�����
            m_moveDir = Math::Vector3(Random::Instance().Range(-1.0f, 1.0f), 0.0f, Random::Instance().Range(-1.0f, 1.0f));
            m_moveDir.Normalize();
        }
        m_changeDirTimer = 1.0f; // ���΂炭�͒��S�Ɍ������ĕ�������
    }

    FacePosition(cTrans, m_moveDir, deltaTime);
}

// m_targetRoom�ȊO�̕�������A���݈ʒu����NavMesh��Ŏ��ۂɓ��B�\�Ȃ��̂������W�߂�B
// (�Ƃ����G�Ȍ`�Ȃǂ̗��R��NavMesh���q�����Ă��Ȃ�������I��ł��܂��ƁA
//  �����֌��������܂܉i���ɗ����������Ă��܂�����)
std::vector<RoomArea*> GhostAI::CollectReachableCandidateRooms(const Math::Vector3& currentPos) const
{
    std::vector<RoomArea*> candidates;
    auto gs = GameSequence::GetInstance();
    if (!gs) return candidates;

    bool navReady = NavMeshManager::Instance().IsBuilt();
    for (RoomArea* room : gs->GetRooms()) {
        if (!room || room == m_targetRoom) continue;
        // Stairs is a connector between floors, not a real destination room - its own center sits at
        // the stairs' mid-height, which GetEffectiveMoveTarget/the navmesh can resolve ambiguously to
        // either the top or bottom landing polygon, causing the ghost to climb up and down repeatedly
        // once it arrives. Never pick it as a wander target.
        if (room->m_isStairs) continue;
        if (navReady && !NavMeshManager::Instance().IsReachable(currentPos, room->GetCenter())) continue;
        candidates.push_back(room);
    }
    return candidates;
}

void GhostAI::StartHouseWander(const Math::Vector3& currentPos)
{
    std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(currentPos);

    if (candidates.empty()) {
        // �o����(���B�\��)���̕������Ȃ��ꍇ��Room�ҋ@�𑱂���
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

void GhostAI::UpdateHouseWander(float deltaTime, TransformData& cTrans)
{
    // �p�j: �S�[�X�g���[���̊O���A�Ƃ͈͓̔��Ɍ��肵��NavMesh�o�R�ŕ������
    EnsureHouseBounds();
    TryOpenNearbyDoor(cTrans.m_position, deltaTime);

    Math::Vector3 moveTarget = GetEffectiveMoveTarget(cTrans.m_position, m_houseWanderTargetPos);
    m_debugFinalTarget = m_houseWanderTargetPos;
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
    if (!m_stairsCrossingLatched && IsInStairsArea(cTrans.m_position, m_stairsNavExitPadding) && m_isCrossingFloors) {
        m_stairsCrossingLatched = true;
        StartStairsCrossing(cTrans.m_position, m_houseWanderTargetPos, m_moveSpeed);
    }
    bool crossingStairs = m_stairsCrossingLatched;
    if (crossingStairs) {
        nextPos = GetStairsCrossingMove(deltaTime);
        if (!m_isCrossingFloors) m_stairsCrossingLatched = false; // height has arrived - release the latch
        // Keep the cached NavMesh path cleared for the whole crossing so the moment it steps back
        // outside the stairwell, MoveToward computes a fresh route instead of resuming whatever
        // stale/irrelevant path happened to be cached from before it entered.
        NavMeshManager::Instance().ClearPath((int)GetGameObject()->GetEntityID());
    } else if (NavMeshManager::Instance().IsBuilt()) {
        nextPos = NavMeshManager::Instance().MoveToward(
            (int)GetGameObject()->GetEntityID(), cTrans.m_position, moveTarget, m_moveSpeed, deltaTime,
            m_pathUpdateInterval, m_pathNodeReachThreshold);
    } else {
        // NavMesh���\�z���͒����ړ��Ƀt�H�[���o�b�N
        Math::Vector3 toTarget = m_houseWanderTargetPos - cTrans.m_position;
        toTarget.y = 0.0f;
        if (toTarget.LengthSquared() > 0.0001f) toTarget.Normalize();
        nextPos = cTrans.m_position + toTarget * m_moveSpeed * deltaTime;
    }
    nextPos = ClampToHouseBounds(nextPos);

    Math::Vector3 moveDelta = nextPos - cTrans.m_position;
    moveDelta.y = 0.0f;
    cTrans.m_position.x = nextPos.x;
    cTrans.m_position.z = nextPos.z;
    if (crossingStairs) {
        // GetStairsCrossingMove already computed height directly from Z-progress along the stairs
        // (see its own comment) instead of relying on gravity/raycast, which was bouncing between
        // adjacent step treads - apply it here and skip the gravity+raycast block below for this
        // frame so it doesn't immediately override this with a raycast-derived height again.
        cTrans.m_position.y = nextPos.y;
        m_skipGravityThisFrame = true;
    }

    if (moveDelta.LengthSquared() > 0.0001f) {
        m_moveDir = moveDelta;
        m_moveDir.Normalize();
    }
    FacePosition(cTrans, m_moveDir, deltaTime);

    if (!m_houseWanderReturning) {
        m_houseWanderDurationTimer -= deltaTime;
    }

    // �����������m: �o�H���q�����Ă��Ȃ��A���邢�͉��炩�̗��R�ł��΂炭�i�߂Ă��Ȃ��ꍇ��
    // ���̖ړI�n����߂�(IsReachable�����蔲�����z��O�̃P�[�X�̕ی�)
    // ���t���[���ł͂Ȃ���̎��Ԃ��ƂɐM�s�̈ړ��ʂ��`�F�b�N����(���t���[���r�r����
    // ���Ȕ���(���̂΂������L�ĂɖY��킹��Ȃǂ�2�o�H�̊Ԃ��s�㕁�����Ă���ꍇ)��
    // ���t���[���r�r��"10cm�ȏ㓮����"�Ə������Ă��܂������ł̗~�l�ˑ��̌��m�Ȃ̂ŁA
    // ���Ȑi����m�ł��Ȃ�)
    m_houseWanderStuckCheckTimer += deltaTime;
    if (m_houseWanderStuckCheckTimer >= m_stuckCheckWindow) {
        float windowProgress = Math::Vector3::Distance(cTrans.m_position, m_houseWanderLastPos);
        if (windowProgress > m_stuckCheckMinProgress) {
            m_houseWanderStuckTimer = 0.0f;
        } else {
            m_houseWanderStuckTimer += m_houseWanderStuckCheckTimer;
        }
        m_houseWanderLastPos = cTrans.m_position;
        m_houseWanderStuckCheckTimer = 0.0f;
    }

    // Quick first-line recovery: if it hasn't made progress in a bit (but hasn't hit the full
    // give-up timeout yet), grant temporary pass-through so it can physically push through whatever
    // it's caught on (a wall/corner the approximate NavMesh geometry didn't perfectly account for)
    // rather than just standing there fighting collision until the full timeout gives up entirely.
    if (m_houseWanderStuckTimer >= m_quickStuckPassThroughDelay && m_doorPassThroughTimer <= 0.0f) {
        m_doorPassThroughTimer = m_doorPassThroughDuration;
    }

    if (m_houseWanderStuckTimer >= m_houseWanderStuckTimeout) {
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
            "[GhostAI] UpdateHouseWander: stuck for %.1fs at (%.2f,%.2f,%.2f) moveTarget=(%.2f,%.2f,%.2f) heading to (%.1f, %.1f, %.1f), giving up this destination.",
            m_houseWanderStuckTimer, cTrans.m_position.x, cTrans.m_position.y, cTrans.m_position.z,
            moveTarget.x, moveTarget.y, moveTarget.z,
            m_houseWanderTargetPos.x, m_houseWanderTargetPos.y, m_houseWanderTargetPos.z);
        m_houseWanderStuckTimer = 0.0f;

        if (m_houseWanderReturning) {
            // �A�蓹�ł��������������ꍇ�́A���̏�ŃS�[�X�g���[�������ɂ��Ďd�؂蒼��
            m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
            SetState(GhostAI::State::Wander);
            return;
        }

        std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(cTrans.m_position);
        if (!candidates.empty()) {
            RoomArea* dest = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
            m_houseWanderTargetPos = dest->GetCenter();
            m_houseWanderLastPos = cTrans.m_position;
        } else if (m_targetRoom) {
            m_houseWanderTargetPos = m_targetRoom->GetCenter();
            m_houseWanderReturning = true;
            m_houseWanderLastPos = cTrans.m_position;
        }
        return;
    }

    float distToTarget = Math::Vector3::Distance(cTrans.m_position, m_houseWanderTargetPos);
    if (distToTarget > m_roomArriveThreshold) return; // �܂��������Ă��Ȃ�

    if (m_houseWanderReturning) {
        // �S�[�X�g���[���ւ̋A�Ҋ��� -> Room�ҋ@��
        m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
        SetState(GhostAI::State::Wander);
        return;
    }

    if (m_houseWanderDurationTimer <= 0.0f) {
        // �p�j���Ԑ؂� -> �S�[�X�g���[���֖߂�
        if (m_targetRoom) {
            m_houseWanderTargetPos = m_targetRoom->GetCenter();
        }
        m_houseWanderReturning = true;
        m_houseWanderLastPos = cTrans.m_position;
        return;
    }

    // �܂��p�j�𑱂���: ���B�\�ȕʂ̕�����ڎw��
    std::vector<RoomArea*> candidates = CollectReachableCandidateRooms(cTrans.m_position);
    if (!candidates.empty()) {
        RoomArea* dest = candidates[Random::Instance().Range(0, (int)candidates.size() - 1)];
        m_houseWanderTargetPos = dest->GetCenter();
    } else if (m_targetRoom) {
        // ���ɍs���镔�����Ȃ���΋A�҂���
        m_houseWanderTargetPos = m_targetRoom->GetCenter();
        m_houseWanderReturning = true;
    }
    m_houseWanderLastPos = cTrans.m_position;
}

bool GhostAI::CanSeePlayer(const TransformData& cTrans, Math::Vector3& outPlayerPos)
{
    if (m_playerEntity == INVALID_ENTITY) {
        m_playerEntity = FindPlayerEntity();
    }
    if (m_playerEntity == INVALID_ENTITY) return false;

    auto& ecs = GameManager::Instance().GetECS();
    auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity);
    if (!pPlayerTrans) return false;

    outPlayerPos = pPlayerTrans->m_position;

    // ���_�ʒu�ƁA�_���ڕW�ʒu(�������ƕs���R�Ȃ̂ŁA�������_��)
    Math::Vector3 eyePos = cTrans.m_position + Math::Vector3(0.0f, m_eyeHeight, 0.0f);
    Math::Vector3 targetPos = outPlayerPos + Math::Vector3(0.0f, 1.0f, 0.0f);

    Math::Vector3 toPlayer = targetPos - eyePos;
    float distance = toPlayer.Length();
    if (distance > m_visionRange) return false;
    // �������߂�����ƈȍ~�̃��C�L���X�g���s����(�ő勗�����ق�0�ɂȂ�)�ɂȂ邽�߁A
    // ���̏ꍇ�͖������Ŏ��F�������Ƃɂ���
    if (distance <= 0.3f) return true;

    Math::Vector3 dir = toPlayer;
    dir /= distance;

    // ����p�`�F�b�N(���������̂݁BGhost�̌���=rotation.y����ɂ���)
    Math::Vector3 forward(sinf(cTrans.m_rotation.y), 0.0f, cosf(cTrans.m_rotation.y));
    Math::Vector3 flatDir(dir.x, 0.0f, dir.z);
    if (flatDir.LengthSquared() > 0.0001f) {
        flatDir.Normalize();
        float cosHalfFov = cosf(DirectX::XMConvertToRadians(m_visionAngle * 0.5f));
        if (forward.Dot(flatDir) < cosHalfFov) return false;
    }

    // �Օ��`�F�b�N: Player�܂ł̂�������Stage���b�V��(�ǂȂ�)�ɓ��������猩���Ă��Ȃ�
    RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(eyePos, dir, distance - 0.2f, "Stage");
    if (hit.hit) return false;

    return true;
}

// HouseWander/Hunt�ňړ����ɁA�߂��ɂ���܂��Ă���h�A�������I�ɊJ����B
// ���t���[���S�G���e�B�e�B�𑖍�����̂͏d���̂ŁAm_doorCheckInterval�ŊԈ����B
// Player�̎蓮�h�A����(Player::TryInteractDoor)�Ǝ����d�g�݂����AGhost�͊J���邾���ŕ߂Ȃ��B
void GhostAI::TryOpenNearbyDoor(const Math::Vector3& ghostPos, float deltaTime)
{
    m_doorCheckTimer -= deltaTime;
    if (m_doorCheckTimer > 0.0f) return;
    m_doorCheckTimer = m_doorCheckInterval;

    auto& ecs = GameManager::Instance().GetECS();
    auto& animMgr = AnimationManager::Instance();

    Entity bestEntity = INVALID_ENTITY;
    int    bestAnimIdx = -1;
    float  bestDist = FLT_MAX;

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel     = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);
        if (!pModel || !pTransform) continue;
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty()) continue;

        Math::Vector3 entityPos = pTransform->m_position;
        if ((ghostPos - entityPos).Length() > 50.0f) continue; // �������肵�����؂�

        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
        if (!pAnim) {
            ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});
            pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
            if (!pAnim) continue;
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

            // ���ɊJ���Ă���(�܂��͊J���Ă���Œ���)�h�A�͑ΏۊO - ���Ă�����̂����J����
            const auto& state = pAnim->multiAnims[i];
            if (state.IsPlaying || state.ProgressTime > 0.0f) continue;

            float dist = (ghostPos - doorPos).Length();
            if (dist < m_doorOpenRange && dist < bestDist)
            {
                bestDist = dist;
                bestEntity = entity;
                bestAnimIdx = i;
            }
        }
    }

    if (bestEntity == INVALID_ENTITY || bestAnimIdx < 0) return;

    animMgr.PlayMultiAnimation(bestEntity, bestAnimIdx, false, 1.0f);

    // While the door swings open, its collider is briefly non-static and can physically shove the
    // Ghost's own capsule collider back away from the doorway (both are dynamic colliders, so the
    // push gets split between them) - make the Ghost's own collider a trigger for a few seconds so
    // it doesn't get pushed around while walking through.
    m_doorPassThroughTimer = m_doorPassThroughDuration;
    SetDoorPassThrough(true);

    //�h�A�A�j���[�V�������̓R���C�_�[�𓮓I�����Ė��t���[��AABB���X�V�����悤�ɂ���
    // (Player::TryInteractDoor�Ɠ����d�g��)
    auto* pCollider = ecs.TryGetComponent<ColliderData>(bestEntity);
    if (pCollider && pCollider->m_isStatic)
    {
        pCollider->m_isStatic = false;
    }
}

void GhostAI::UpdateHunt(float deltaTime, TransformData& cTrans)
{
    // �n���g: �ʒu�⎋�F�Ɋւ�炸�J�n����̂ŁAPlayer �̌��݈ʒu�𒼐ڒǐՂ���
    // (CanSeePlayer �ɂ�鎋�F�Q�[�g��Hunt�J�n�E�p���ǂ���ɂ��g��Ȃ�)
    if (m_playerEntity == INVALID_ENTITY) {
        m_playerEntity = FindPlayerEntity();
    }

    Math::Vector3 target = cTrans.m_position;
    auto& ecs = GameManager::Instance().GetECS();
    if (m_playerEntity != INVALID_ENTITY) {
        if (auto* pPlayerTrans = ecs.TryGetComponent<TransformData>(m_playerEntity)) {
            target = pPlayerTrans->m_position;
            m_lastKnownPlayerPos = target;
            m_hasLastKnownPlayerPos = true;
        }
    }

    EnsureHouseBounds();
    TryOpenNearbyDoor(cTrans.m_position, deltaTime);

    Math::Vector3 moveTarget = GetEffectiveMoveTarget(cTrans.m_position, target);
    m_debugFinalTarget = target;
    m_debugMoveTarget = moveTarget;

    // NavMesh���\�z�ς݂Ȃ炻����̌o�H�ňړ�����(�ǂ�˂��������Ƀh�A��ʘH���o�R�ł���)
    Math::Vector3 nextPos;
    float huntSpeed = m_moveSpeed * m_huntSpeedMultiplier;
    if (!m_stairsCrossingLatched && IsInStairsArea(cTrans.m_position, m_stairsNavExitPadding) && m_isCrossingFloors) {
        m_stairsCrossingLatched = true;
        StartStairsCrossing(cTrans.m_position, target, huntSpeed);
    }
    bool crossingStairsHunt = m_stairsCrossingLatched;
    if (crossingStairsHunt) {
        nextPos = GetStairsCrossingMove(deltaTime);
        if (!m_isCrossingFloors) m_stairsCrossingLatched = false;
        NavMeshManager::Instance().ClearPath((int)GetGameObject()->GetEntityID());
    } else if (NavMeshManager::Instance().IsBuilt()) {
        nextPos = NavMeshManager::Instance().MoveToward(
            (int)GetGameObject()->GetEntityID(), cTrans.m_position, moveTarget, huntSpeed, deltaTime,
            m_pathUpdateInterval, m_pathNodeReachThreshold);
    } else {
        // NavMesh���\�z���͒����ړ��Ƀt�H�[���o�b�N
        Math::Vector3 toTarget = target - cTrans.m_position;
        toTarget.y = 0.0f;
        if (toTarget.LengthSquared() > 0.0001f) toTarget.Normalize();
        nextPos = cTrans.m_position + toTarget * huntSpeed * deltaTime;
    }
    nextPos = ClampToHouseBounds(nextPos);

    Math::Vector3 moveDelta = nextPos - cTrans.m_position;
    moveDelta.y = 0.0f;
    cTrans.m_position.x = nextPos.x;
    cTrans.m_position.z = nextPos.z;
    if (crossingStairsHunt) {
        cTrans.m_position.y = nextPos.y;
        m_skipGravityThisFrame = true;
    }

    if (moveDelta.LengthSquared() > 0.0001f) {
        m_moveDir = moveDelta;
        m_moveDir.Normalize();
    }

    FacePosition(cTrans, m_moveDir, deltaTime);

    // Hunt previously had no stuck-detection at all (unlike UpdateHouseWander), so once physically
    // caught on something the ghost would just stand there fighting collision for the rest of the
    // Hunt duration with no recovery. Mirror the same quick pass-through grant used there.
    // Progress is measured over a window (see m_stuckCheckWindow's comment) rather than
    // frame-to-frame, so rapid back-and-forth oscillation - which looks like progress on any
    // single frame - is correctly detected as stuck too.
    m_huntStuckCheckTimer += deltaTime;
    if (m_huntStuckCheckTimer >= m_stuckCheckWindow) {
        float windowProgress = Math::Vector3::Distance(cTrans.m_position, m_huntStuckLastPos);
        if (windowProgress > m_stuckCheckMinProgress) {
            m_huntStuckTimer = 0.0f;
        } else {
            m_huntStuckTimer += m_huntStuckCheckTimer;
        }
        m_huntStuckLastPos = cTrans.m_position;
        m_huntStuckCheckTimer = 0.0f;
    }
    if (m_huntStuckTimer >= m_quickStuckPassThroughDelay && m_doorPassThroughTimer <= 0.0f) {
        m_doorPassThroughTimer = m_doorPassThroughDuration;
    }
}

void GhostAI::Update(float deltaTime)
{
    if (m_isExorcised) return;

    EnsureStairsRoom();

    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    if (m_currentState == GhostAI::State::Stun) {
        m_stunTimer -= deltaTime;
        if (m_stunTimer <= 0.0f) {
            SetState(GhostAI::State::Wander);
        }
        return;
    }

    // �n���g�͈ʒu�⎋�F�Ɋ֌W�Ȃ��A�����I�ɖ������ŊJ�n����(Room�ҋ@�E�p�j���̂�)
    if (m_currentState == GhostAI::State::Wander || m_currentState == GhostAI::State::HouseWander) {
        // Debug: force-start a Hunt immediately, bypassing the random trigger timer, so testing
        // doesn't require waiting out m_huntTriggerIntervalMin/Max.
        bool debugForceHunt = Input::Instance().IsKeyTrigger('H');
        m_huntTriggerTimer -= deltaTime;
        if (m_huntTriggerTimer <= 0.0f || debugForceHunt) {
            SetState(GhostAI::State::Hunt);
            m_huntTimer = Random::Instance().Range(m_huntDurationMin, m_huntDurationMax);
            m_huntTriggerTimer = Random::Instance().Range(m_huntTriggerIntervalMin, m_huntTriggerIntervalMax);
            m_huntStuckTimer = 0.0f;
            m_huntStuckLastPos = cTrans.m_position;
            if (debugForceHunt) {
                Logger::Instance().AddLog(Logger::LogLevel::Info, "[GhostAI] Debug: Hunt force-started via H key.");
            }
        }
    }

    switch (m_currentState) {
        case GhostAI::State::Wander:
            m_houseWanderTimer -= deltaTime;
            if (m_houseWanderTimer <= 0.0f) {
                StartHouseWander(cTrans.m_position);
            } else {
                UpdateWander(deltaTime, cTrans);
            }
            break;

        case GhostAI::State::HouseWander:
            UpdateHouseWander(deltaTime, cTrans);
            break;

        case GhostAI::State::Hunt:
            m_huntTimer -= deltaTime;
            if (m_huntTimer <= 0.0f) {
                // �n���g�I��: �e���|�[�g�����A�S�[�X�g���[���܂�NavMesh�o�R�œk���ŋA��
                m_hasLastKnownPlayerPos = false;
                if (m_targetRoom && NavMeshManager::Instance().IsBuilt()) {
                    m_houseWanderTargetPos = m_targetRoom->GetCenter();
                    m_houseWanderReturning = true;
                    m_houseWanderDurationTimer = 0.0f;
                    m_houseWanderStuckTimer = 0.0f;
                    m_houseWanderLastPos = cTrans.m_position;
                    SetState(GhostAI::State::HouseWander);
                } else {
                    // �ڕW�̕������Ȃ��A�܂���NavMesh�����\�z�Ȃ炻�̏��Room�ҋ@�����ɂ���
                    m_houseWanderTimer = Random::Instance().Range(m_houseWanderIntervalMin, m_houseWanderIntervalMax);
                    SetState(GhostAI::State::Wander);
                }
            } else {
                UpdateHunt(deltaTime, cTrans);
            }
            break;

        default:
            break;
    }

    if (m_doorPassThroughTimer > 0.0f) {
        m_doorPassThroughTimer -= deltaTime;
    }

    bool wantPassThrough = (m_doorPassThroughTimer > 0.0f) || IsInStairsArea(cTrans.m_position);
    if (wantPassThrough != m_wasPassThroughActive) {
        SetDoorPassThrough(wantPassThrough);
        m_wasPassThroughActive = wantPassThrough;
    }

    //---�d��(�ǂ̃X�e�[�g�ł���ɓK�p) ---
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
    if (!m_skipGravityThisFrame && !holdAtExitLanding) {
        m_velocityY -= m_gravityStrength * deltaTime;
        cTrans.m_position.y += m_velocityY * deltaTime;

        // --- �n�ʃ`�F�b�N (���C�L���X�g) ---
        // origin �͏�� position + 0.5 (�Œ�)
        // groundOffset = ���f�����_���牽m�ɂ��邩(�v���X�l)
        Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0);
        Math::Vector3 rayDir(0, -1, 0);
        RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(origin, rayDir, 1000.0f, "Stage");

        // 臒l = groundOffset(���_���瑫���܂ł̋���) + 0.5(origin offset) + 0.1(�]�T)
        float snapThreshold = m_groundOffset + 0.5f + 0.1f;

        if (hit.hit && hit.distance <= snapThreshold)
        {
            m_isGrounded = true;
            if (m_velocityY < 0.0f)
            {
                m_velocityY = 0.0f;
                // �ڒn�ʒu�ɍ��킹��: �V����Y = origin.y - distance
                // ���_�𑫌��Ƃ��� groundOffset �Ԃ��ɒu��
                cTrans.m_position.y = (origin.y - hit.distance) + m_groundOffset;
            }
        }
        else
        {
            m_isGrounded = false;
        }
    } else {
        m_velocityY = 0.0f;
        m_isGrounded = true;
        if (holdAtExitLanding) {
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
    if (m_currentState == State::Hunt) {
        ShaderManager::Instance().SetHuntActive(false);
    }
}

void GhostAI::Draw()
{
    if (!m_showPathDebug) return;
    if (m_currentState != State::HouseWander && m_currentState != State::Hunt) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto* pTrans = ecs.TryGetComponent<TransformData>(GetGameObject()->GetEntityID());
    if (!pTrans) return;
    Math::Vector3 pos = pTrans->m_position;

    auto& cm = CollisionManager::Instance();
    const ImU32 colorFinalTarget = IM_COL32(255, 60, 60, 255);  // red: ultimate destination (room center / player position)
    const ImU32 colorMoveTarget  = IM_COL32(255, 200, 0, 255);  // orange: GetEffectiveMoveTarget's actual NavMesh target (may be a stairs waypoint instead of the real destination)
    const ImU32 colorPath        = IM_COL32(0, 220, 255, 255);  // cyan: the currently cached path waypoints being followed

    auto drawMarker = [&](const Math::Vector3& p, ImU32 color) {
        cm.AddDebugLineAlways(p, p + Math::Vector3(0, 1.0f, 0), color);
    };

    drawMarker(m_debugFinalTarget, colorFinalTarget);
    cm.AddDebugLineAlways(pos + Math::Vector3(0, 0.1f, 0), m_debugFinalTarget + Math::Vector3(0, 0.1f, 0), colorFinalTarget);

    if ((m_debugMoveTarget - m_debugFinalTarget).LengthSquared() > 0.01f) {
        drawMarker(m_debugMoveTarget, colorMoveTarget);
        cm.AddDebugLineAlways(pos + Math::Vector3(0, 0.3f, 0), m_debugMoveTarget + Math::Vector3(0, 0.3f, 0), colorMoveTarget);
    }

    if (const auto* path = NavMeshManager::Instance().GetCachedPath((int)GetGameObject()->GetEntityID())) {
        Math::Vector3 prev = pos;
        for (const auto& wp : *path) {
            cm.AddDebugLineAlways(prev + Math::Vector3(0, 0.5f, 0), wp + Math::Vector3(0, 0.5f, 0), colorPath);
            drawMarker(wp, colorPath);
            prev = wp;
        }
    }
}

void GhostAI::Serialize(nlohmann::json& out) const
{
    out["moveSpeed"]    = m_moveSpeed;
    out["animIdle"]     = m_animIdle;
    out["animWander"]   = m_animWander;
    out["animHunt"]     = m_animHunt;
    out["animDead"]     = m_animDead;
    out["groundOffset"] = m_groundOffset;
    out["houseBoundsMargin"] = m_houseBoundsMargin;
    out["houseWanderIntervalMin"] = m_houseWanderIntervalMin;
    out["houseWanderIntervalMax"] = m_houseWanderIntervalMax;
    out["houseWanderDurationMin"] = m_houseWanderDurationMin;
    out["houseWanderDurationMax"] = m_houseWanderDurationMax;
    out["roomArriveThreshold"]    = m_roomArriveThreshold;
    out["houseWanderStuckTimeout"] = m_houseWanderStuckTimeout;
    out["huntDurationMin"]    = m_huntDurationMin;
    out["huntDurationMax"]    = m_huntDurationMax;
    out["huntSpeedMultiplier"] = m_huntSpeedMultiplier;
    out["huntTriggerIntervalMin"] = m_huntTriggerIntervalMin;
    out["huntTriggerIntervalMax"] = m_huntTriggerIntervalMax;
    out["visionRange"]        = m_visionRange;
    out["visionAngle"]        = m_visionAngle;
    out["eyeHeight"]          = m_eyeHeight;
    out["searchGiveUpDistance"] = m_searchGiveUpDistance;
    out["doorCheckInterval"]  = m_doorCheckInterval;
    out["doorOpenRange"]      = m_doorOpenRange;
    out["pathNodeReachThreshold"] = m_pathNodeReachThreshold;
}

void GhostAI::Deserialize(const nlohmann::json& in)
{
    if (in.contains("moveSpeed"))    m_moveSpeed    = in["moveSpeed"];
    if (in.contains("animIdle"))     m_animIdle     = in["animIdle"];
    if (in.contains("animWander"))   m_animWander   = in["animWander"];
    if (in.contains("animHunt"))     m_animHunt     = in["animHunt"];
    if (in.contains("animDead"))     m_animDead     = in["animDead"];
    if (in.contains("groundOffset")) m_groundOffset = in["groundOffset"];
    if (in.contains("houseBoundsMargin")) m_houseBoundsMargin = in["houseBoundsMargin"];
    if (in.contains("houseWanderIntervalMin")) m_houseWanderIntervalMin = in["houseWanderIntervalMin"];
    if (in.contains("houseWanderIntervalMax")) m_houseWanderIntervalMax = in["houseWanderIntervalMax"];
    if (in.contains("houseWanderDurationMin")) m_houseWanderDurationMin = in["houseWanderDurationMin"];
    if (in.contains("houseWanderDurationMax")) m_houseWanderDurationMax = in["houseWanderDurationMax"];
    if (in.contains("roomArriveThreshold"))    m_roomArriveThreshold    = in["roomArriveThreshold"];
    if (in.contains("houseWanderStuckTimeout")) m_houseWanderStuckTimeout = in["houseWanderStuckTimeout"];
    if (in.contains("huntDurationMin"))     m_huntDurationMin     = in["huntDurationMin"];
    if (in.contains("huntDurationMax"))     m_huntDurationMax     = in["huntDurationMax"];
    if (in.contains("huntSpeedMultiplier")) m_huntSpeedMultiplier = in["huntSpeedMultiplier"];
    if (in.contains("huntTriggerIntervalMin")) m_huntTriggerIntervalMin = in["huntTriggerIntervalMin"];
    if (in.contains("huntTriggerIntervalMax")) m_huntTriggerIntervalMax = in["huntTriggerIntervalMax"];
    if (in.contains("visionRange"))         m_visionRange         = in["visionRange"];
    if (in.contains("visionAngle"))         m_visionAngle         = in["visionAngle"];
    if (in.contains("eyeHeight"))           m_eyeHeight           = in["eyeHeight"];
    if (in.contains("searchGiveUpDistance")) m_searchGiveUpDistance = in["searchGiveUpDistance"];
    if (in.contains("doorCheckInterval"))   m_doorCheckInterval    = in["doorCheckInterval"];
    if (in.contains("doorOpenRange"))       m_doorOpenRange        = in["doorOpenRange"];
    if (in.contains("pathNodeReachThreshold")) m_pathNodeReachThreshold = in["pathNodeReachThreshold"];
}

void GhostAI::ImGuiUpdate()
{
    ImGui::DragFloat("Move Speed",     &m_moveSpeed,     0.1f,  0.1f, 10.0f);
    ImGui::DragFloat("Ground Offset",  &m_groundOffset,  0.01f, -2.0f, 2.0f);
    ImGui::Separator();
    ImGui::Text("House Bounds");
    ImGui::DragFloat("Bounds Margin", &m_houseBoundsMargin, 0.1f, 0.0f, 20.0f);
    if (ImGui::Button("Recompute Bounds")) {
        m_houseBoundsValid = false;
        EnsureHouseBounds();
    }
    if (m_houseBoundsValid) {
        ImGui::Text("Min: (%.1f, %.1f, %.1f)", m_houseBoundsMin.x, m_houseBoundsMin.y, m_houseBoundsMin.z);
        ImGui::Text("Max: (%.1f, %.1f, %.1f)", m_houseBoundsMax.x, m_houseBoundsMax.y, m_houseBoundsMax.z);
    } else {
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
    ImGui::DragFloat("Hunt Speed Multiplier", &m_huntSpeedMultiplier, 0.1f, 0.1f, 5.0f);
    ImGui::DragFloat("Hunt Trigger Interval Min", &m_huntTriggerIntervalMin, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Hunt Trigger Interval Max", &m_huntTriggerIntervalMax, 0.5f, 1.0f, 300.0f);
    ImGui::DragFloat("Path Node Reach Threshold", &m_pathNodeReachThreshold, 0.02f, 0.05f, 1.0f);
    ImGui::Checkbox("Show Path Debug (red=destination, orange=NavMesh target, cyan=path)", &m_showPathDebug);
    ImGui::Separator();
    ImGui::Text("Door");
    ImGui::DragFloat("Door Open Range", &m_doorOpenRange, 0.1f, 0.5f, 10.0f);
    ImGui::DragFloat("Door Check Interval", &m_doorCheckInterval, 0.05f, 0.05f, 2.0f);
    ImGui::Separator();
    ImGui::Text("Vision (currently unused for Hunt trigger)");
    ImGui::DragFloat("Vision Range", &m_visionRange, 0.5f, 0.0f, 50.0f);
    ImGui::DragFloat("Vision Angle", &m_visionAngle, 1.0f, 0.0f, 360.0f);
    ImGui::DragFloat("Eye Height", &m_eyeHeight, 0.05f, 0.0f, 3.0f);
    ImGui::DragFloat("Search GiveUp Dist", &m_searchGiveUpDistance, 0.1f, 0.1f, 10.0f);
    ImGui::Text("State: %s", m_currentState == State::Idle ? "Idle" :
                              m_currentState == State::Wander ? "Wander (Room)" :
                              m_currentState == State::HouseWander ? "HouseWander" :
                              m_currentState == State::Hunt ? "Hunt" :
                              m_currentState == State::Stun ? "Stun" : "Dead");
    ImGui::Text("NavMesh Built: %s", NavMeshManager::Instance().IsBuilt() ? "true" : "false");
    {
        auto gs = GameSequence::GetInstance();
        int roomCount = gs ? (int)gs->GetRooms().size() : 0;
        ImGui::Text("GameSequence Rooms: %d (0 means GameSequence/RoomArea not ready yet)", roomCount);
    }
    if (m_currentState == State::Wander) {
        ImGui::Text("Time until next HouseWander: %.1f s", m_houseWanderTimer);
        ImGui::Text("Time until next Hunt: %.1f s", m_huntTriggerTimer);
    } else if (m_currentState == State::HouseWander) {
        ImGui::Text("HouseWander returning home: %s", m_houseWanderReturning ? "true" : "false");
        ImGui::Text("Time left roaming outside: %.1f s", m_houseWanderDurationTimer);
        ImGui::Text("Stuck timer: %.1f / %.1f s", m_houseWanderStuckTimer, m_houseWanderStuckTimeout);
        ImGui::Text("Target: (%.1f, %.1f, %.1f)", m_houseWanderTargetPos.x, m_houseWanderTargetPos.y, m_houseWanderTargetPos.z);
        ImGui::Text("Time until next Hunt: %.1f s", m_huntTriggerTimer);
    } else if (m_currentState == State::Hunt) {
        ImGui::Text("Hunt time left: %.1f s", m_huntTimer);
    }
}

void GhostAI::OnCollisionEnter(GameObject* other)
{
    if (m_isExorcised || m_currentState == GhostAI::State::Stun) return;

    // Player��ECS�̓Ɨ�����Component�^�Ƃ��Ă͓o�^����Ă��Ȃ����߁A
    // TryGetComponent<Player>()�͎g���Ȃ�(���o�^�̃R���|�[�l���g�^��assert�ŗ�����)�B
    // NativeScriptData�o�R��dynamic_cast���邩�A���O�Ŕ��肷��B
    auto& ecs = GameManager::Instance().GetECS();
    bool isPlayer = (other->GetName() == "Player");
    if (!isPlayer) {
        if (auto* pScriptData = ecs.TryGetComponent<NativeScriptData>(other->GetEntityID())) {
            isPlayer = (dynamic_cast<Player*>(pScriptData->Instance.get()) != nullptr);
        }
    }

    if (isPlayer) {
        if (auto gs = GameSequence::GetInstance()) {
            gs->NotifyGameOver();
        }
    }
}

void GhostAI::OnCollisionStay(GameObject* other)
{
}

void GhostAI::Exorcise()
{
    if (m_isExorcised) return;

    m_isExorcised = true;
    SetState(GhostAI::State::Dead);

    if (auto gs = GameSequence::GetInstance()) {
        gs->NotifyExorcised();
    }
}
