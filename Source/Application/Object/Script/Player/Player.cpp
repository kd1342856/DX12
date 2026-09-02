#include "../../../../Pch.h"
#include "Player.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../System/GameSequence.h"
#include "../System/RoomArea.h"
#include "../Ghost/GhostAI.h"
#include "../Item/PickupItem.h"
#include "../../../../Framework/DirectX/Utility/Profiler.h"
#include <functional>

REGISTER_COMPONENT(Player);

void Player::Awake() {}

void Player::Start()
{
    // AnimationDataComponent�͎������g�ł͂Ȃ��q��"Model"�I�u�W�F�N�g�ɕt���Ă��邽�߁A
    // �q�K�w��T���ăL���b�V�����Ă���(���t���[���T�����Ȃ��悤��)
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
    if (GetGameObject()) {
        findAnimEntity(GetGameObject());
    }

    FindItemIconEntities();
}

void Player::FindItemIconEntities()
{
    // Looks up the HUD icon Sprite entities by name and caches them.
    // If no matching entity exists in the scene it stays INVALID_ENTITY and the icon update just no-ops.
    if (!GetGameObject() || !GetGameObject()->GetScene()) return;
    auto& ecs = GameManager::Instance().GetECS();
    auto* scene = GetGameObject()->GetScene();

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        if (!ecs.TryGetComponent<SpriteData>(entity)) continue;
        auto obj = scene->GetGameObject(entity);
        if (!obj) continue;
        if (obj->GetName() == "ThermometerIcon") m_thermometerIconEntity = entity;
        else if (obj->GetName() == "SmudgeStickIcon") m_incenseIconEntity = entity;
        else if (obj->GetName() == "OfudaIcon") m_amuletIconEntity = entity;
        else if (obj->GetName() == "InteractPrompt") m_interactPromptEntity = entity;
    }

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        if (!ecs.TryGetComponent<ModelRenderData>(entity)) continue;
        auto obj = scene->GetGameObject(entity);
        if (!obj) continue;
        if (obj->GetName() == "HeldThermometer") m_heldThermometerEntity = entity;
        else if (obj->GetName() == "HeldSmudgeStick") m_heldIncenseEntity = entity;
        else if (obj->GetName() == "HeldOfuda") m_heldAmuletEntity = entity;
    }
}

void Player::UpdateItemIcons()
{
    // Only the currently equipped AND owned item's icon is shown (alpha 1); the rest are hidden (alpha 0).
    auto& ecs = GameManager::Instance().GetECS();
    auto setAlpha = [&](Entity e, bool visible) {
        if (e == INVALID_ENTITY) return;
        if (auto* sprite = ecs.TryGetComponent<SpriteData>(e)) {
            sprite->m_color.w = visible ? 1.0f : 0.0f;
        }
    };
    setAlpha(m_thermometerIconEntity, m_hasThermometer && m_currentEquipped == Item::Thermometer);
    setAlpha(m_incenseIconEntity, m_hasIncense && m_currentEquipped == Item::Incense);
    setAlpha(m_amuletIconEntity, m_hasAmulet && m_currentEquipped == Item::Amulet);
}

void Player::UpdateHeldItem()
{
    // Only the currently equipped AND owned item's held-in-hand model is shown.
    auto& ecs = GameManager::Instance().GetECS();
    auto setVisible = [&](Entity e, bool visible) {
        if (e == INVALID_ENTITY) return;
        if (auto* model = ecs.TryGetComponent<ModelRenderData>(e)) {
            model->m_isVisible = visible;
        }
    };
    setVisible(m_heldThermometerEntity, m_hasThermometer && m_currentEquipped == Item::Thermometer);
    setVisible(m_heldIncenseEntity, m_hasIncense && m_currentEquipped == Item::Incense);
    setVisible(m_heldAmuletEntity, m_hasAmulet && m_currentEquipped == Item::Amulet);
}

void Player::UpdateThermometerAnimation()
{
    if (m_heldThermometerEntity == INVALID_ENTITY) return;
    if (!(m_hasThermometer && m_currentEquipped == Item::Thermometer)) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_heldThermometerEntity);
    auto* pAnimData = ecs.TryGetComponent<AnimationDataComponent>(m_heldThermometerEntity);
    if (!pModel || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded() || !pAnimData) return;

    const auto& anims = pModel->m_spModelData->GetAnimations();
    if (anims.empty()) return;

    float duration = anims[0].duration;
    if (duration <= 0.0f) return;

    // m_temperature ranges 20 (normal) down to 5 (inside the ghost's room); map that to
    // the baked "TempDrop" animation's 0..1 range (0 = full mercury, 1 = nearly empty).
    float t = std::clamp((20.0f - m_temperature) / (20.0f - 5.0f), 0.0f, 1.0f);

    pAnimData->currentAnim.AnimationIndex = 0;
    pAnimData->currentAnim.IsPlaying = false;
    pAnimData->currentAnim.ProgressTime = t * duration;
}

void Player::UpdateHeldItemAttach(const Math::Vector3& playerPos, float playerYaw, bool isMoving)
{
    Entity visibleEntity = INVALID_ENTITY;
    if (m_hasThermometer && m_currentEquipped == Item::Thermometer) visibleEntity = m_heldThermometerEntity;
    else if (m_hasIncense && m_currentEquipped == Item::Incense) visibleEntity = m_heldIncenseEntity;
    else if (m_hasAmulet && m_currentEquipped == Item::Amulet) visibleEntity = m_heldAmuletEntity;
    if (visibleEntity == INVALID_ENTITY) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto* pHeldTrans = ecs.TryGetComponent<TransformData>(visibleEntity);
    if (!pHeldTrans) return;

    // Auto-calibrate m_heldItemOffset from the real hand bone once the player has been standing
    // still (holding this item) for a little while. UpdateAnimationState() switches the clip to
    // UseItem_Idle the same frame an item is first equipped, but that switch only reaches
    // node.globalTransform once AnimationSystem processes it later this frame - calibrating
    // immediately would still read last frame's PRE-switch pose (relaxed arm at the side, not the
    // presenting pose), so wait long enough for the new pose to actually settle in first.
    if (!m_heldItemOffsetCalibrated)
    {
        if (!isMoving && visibleEntity == m_calibWaitEntity)
        {
            m_calibWaitTimer += GameTimer::Instance().DeltaTime();
            if (m_calibWaitTimer > 0.3f)
            {
                CalibrateHeldItemOffset(playerPos, playerYaw);
            }
        }
        else
        {
            m_calibWaitEntity = visibleEntity;
            m_calibWaitTimer = 0.0f;
        }
    }

    // Fixed body-relative offset (rotated by yaw, added to the player's position) - no per-frame
    // bone-tracking. See m_heldItemOffset's declaration for why.
    Math::Matrix yawMat = Math::Matrix::CreateRotationY(playerYaw);
    pHeldTrans->m_position = playerPos + Math::Vector3::TransformNormal(m_heldItemOffset, yawMat);
    const float kDegToRad = 3.14159265f / 180.0f;
    pHeldTrans->m_rotation = Math::Vector3(
        m_heldItemExtraRotationDeg.x * kDegToRad,
        playerYaw + m_heldItemExtraRotationDeg.y * kDegToRad,
        m_heldItemExtraRotationDeg.z * kDegToRad);
}

bool Player::CalibrateHeldItemOffset(const Math::Vector3& playerPos, float playerYaw)
{
    if (m_animEntity == INVALID_ENTITY) return false;

    auto& ecs = GameManager::Instance().GetECS();
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_animEntity);
    auto* pModelTrans = ecs.TryGetComponent<TransformData>(m_animEntity);
    if (!pModel || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded() || !pModelTrans) return false;

    const auto& nodes = pModel->m_spModelData->GetNodes();
    int nodeIndex = -1;
    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (nodes[i].name == "Attach_RightHand") { nodeIndex = i; break; }
    }
    if (nodeIndex < 0) return false; // no attach point authored on this model

    // Build the Model child's world matrix ourselves from its LOCAL Position/Rotation/Scale (always
    // valid immediately) combined with the player's own position/yaw passed in here, rather than
    // trusting pModelTrans->m_worldMatrix - that field is only refreshed once per frame by
    // TransformSystem, and if calibration runs before that pass has executed yet this frame (e.g.
    // right after a scene (re)load) it still holds a stale/identity matrix unrelated to where the
    // player actually is, producing wildly wrong (multi-meter) offsets.
    Math::Matrix modelLocalMat = Math::Matrix::CreateScale(pModelTrans->m_scale)
        * Math::Matrix::CreateFromYawPitchRoll(pModelTrans->m_rotation.y, pModelTrans->m_rotation.x, pModelTrans->m_rotation.z)
        * Math::Matrix::CreateTranslation(pModelTrans->m_position);
    Math::Matrix playerWorldMat = Math::Matrix::CreateRotationY(playerYaw) * Math::Matrix::CreateTranslation(playerPos);
    Math::Matrix modelWorldMat = modelLocalMat * playerWorldMat;

    // Use the LIVE animated globalTransform, not originalGlobalTransform (the bind/rest pose) -
    // tried that, but this model's bind pose is a T-pose (arm out to the side, above shoulder
    // height), which is worse than the actual Idle stance. The live pose reflects wherever the
    // hand currently rests in whatever clip is playing (calibration only runs while standing still).
    Math::Matrix worldMat = nodes[nodeIndex].globalTransform * modelWorldMat;
    Math::Vector3 scale, worldPos;
    Math::Quaternion rot;
    worldMat.Decompose(scale, rot, worldPos);

    // Undo the player's current position/yaw to get a body-local offset.
    Math::Matrix invYaw = Math::Matrix::CreateRotationY(-playerYaw);
    m_heldItemOffset = Math::Vector3::TransformNormal(worldPos - playerPos, invYaw);
    m_heldItemOffsetCalibrated = true;

    //char msg[160];
    //sprintf_s(msg, "[Player] Held item offset calibrated from Attach_RightHand: (%.3f, %.3f, %.3f)",
    //    m_heldItemOffset.x, m_heldItemOffset.y, m_heldItemOffset.z);
    //Logger::Instance().AddLog(Logger::LogLevel::Info, msg);
    return true;
}

void Player::UpdateInteractionTarget(const Math::Vector3& playerPos, const Math::Vector3& forward)
{
    const float kInteractRange = 2.0f;

    float doorDist = FLT_MAX;
    bool hasDoor;
    {
        PROFILE_CPU_SCOPE("Player::FindNearestDoor");
        hasDoor = FindNearestDoor(playerPos, forward, kInteractRange, doorDist);
    }

    float pickupDist = FLT_MAX;
    Entity pickupEntity;
    {
        PROFILE_CPU_SCOPE("Player::FindNearestPickup");
        pickupEntity = FindNearestPickup(playerPos, forward, kInteractRange, pickupDist);
    }

    if (hasDoor && (pickupEntity == INVALID_ENTITY || doorDist <= pickupDist))
    {
        m_interactKind = InteractKind::Door;
        m_interactEntity = INVALID_ENTITY;
    }
    else if (pickupEntity != INVALID_ENTITY)
    {
        m_interactKind = InteractKind::Pickup;
        m_interactEntity = pickupEntity;
    }
    else
    {
        m_interactKind = InteractKind::None;
        m_interactEntity = INVALID_ENTITY;
    }
}

void Player::UpdateInteractPrompt()
{
    if (m_interactPromptEntity == INVALID_ENTITY) return;
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* sprite = ecs.TryGetComponent<SpriteData>(m_interactPromptEntity))
    {
        sprite->m_color.w = (m_interactKind != InteractKind::None) ? 1.0f : 0.0f;
    }
}

void Player::RebuildDoorCandidateCache() const
{
    auto& ecs = GameManager::Instance().GetECS();
    m_doorCandidateCache.clear();

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);
        if (!pModel || !pTransform) continue;
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty()) continue;

        for (int i = 0; i < (int)anims.size(); ++i)
        {
            const std::string& animName = anims[i].name;
            if (animName.find("Door") == std::string::npos && animName.find("door") == std::string::npos) continue;

            Math::Vector3 doorPos = pTransform->m_position;
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

            m_doorCandidateCache.push_back({ entity, i, doorPos });
        }
    }

    m_doorCandidateCacheBuilt = true;
}

bool Player::FindNearestDoor(const Math::Vector3& playerPos, const Math::Vector3& forward, float maxRange, float& outDist) const
{
    if (!m_doorCandidateCacheBuilt) RebuildDoorCandidateCache();

    bool found = false;
    outDist = FLT_MAX;

    for (const auto& door : m_doorCandidateCache)
    {
        Math::Vector3 toDoor = door.worldPos - playerPos;
        float dist = toDoor.Length();
        if (dist >= maxRange) continue;
        if (dist > 0.001f)
        {
            Math::Vector3 dir = toDoor;
            dir.Normalize();
            if (dir.Dot(forward) < 0.5f) continue; // outside the facing cone (60 degree half-angle)
        }
        if (dist < outDist) { outDist = dist; found = true; }
    }
    return found;
}

Entity Player::FindNearestPickup(const Math::Vector3& playerPos, const Math::Vector3& forward, float maxRange, float& outDist) const
{
    auto& ecs = GameManager::Instance().GetECS();
    Entity best = INVALID_ENTITY;
    outDist = FLT_MAX;

    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        auto* pickup = dynamic_cast<PickupItem*>(scriptData.Instance.get());
        if (!pickup || pickup->m_collected) continue;
        GameObject* obj = pickup->GetGameObject();
        if (!obj) continue;
        auto* pTransform = ecs.TryGetComponent<TransformData>(obj->GetEntityID());
        if (!pTransform) continue;

        Math::Vector3 toItem = pTransform->m_position - playerPos;
        float dist = toItem.Length();
        if (dist >= maxRange) continue;
        if (dist > 0.001f)
        {
            Math::Vector3 dir = toItem;
            dir.Normalize();
            if (dir.Dot(forward) < 0.5f) continue; // outside the facing cone (60 degree half-angle)
        }
        if (dist < outDist) { outDist = dist; best = obj->GetEntityID(); }
    }
    return best;
}

bool Player::IsHoldingVisibleItem() const
{
    return (m_hasThermometer && m_currentEquipped == Item::Thermometer)
        || (m_hasIncense && m_currentEquipped == Item::Incense)
        || (m_hasAmulet && m_currentEquipped == Item::Amulet);
}

void Player::UpdateAnimationState(bool isMoving)
{
    if (m_animEntity == INVALID_ENTITY) return;

    const char* clip;
    if (m_isCrouching) clip = isMoving ? "CrouchWalk" : "Crouch";
    else                clip = isMoving ? "Walk" : "Idle";

    std::string clipName = clip;
    if (IsHoldingVisibleItem()) clipName = std::string("UseItem_") + clip;

    float speed = isMoving ? m_animWalkSpeed : 1.0f;
    AnimationManager::Instance().PlayAnimationByName(m_animEntity, clipName, true, speed);
}

void Player::AddItem(ItemType item)
{
    bool alreadyHad = HasItem(item);
    switch (item) {
    case ItemType::Thermometer: m_hasThermometer = true; break;
    case ItemType::Incense:     m_hasIncense = true; break;
    case ItemType::Amulet:      m_hasAmulet = true; break;
    }
    if (!alreadyHad) {
        m_currentEquipped = item; // auto-equip on first pickup
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Item acquired.");
    }
}

bool Player::HasItem(ItemType item) const
{
    switch (item) {
    case ItemType::Thermometer: return m_hasThermometer;
    case ItemType::Incense:     return m_hasIncense;
    case ItemType::Amulet:      return m_hasAmulet;
    }
    return false;
}

void Player::Update(float deltaTime)
{
    auto& input = Input::Instance();
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    // --- Facing direction (used for movement below, and for the interaction facing-cone) ---
    Math::Matrix playerRot = Math::Matrix::CreateRotationY(cTrans.m_rotation.y);
    Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), playerRot);
    Math::Vector3 right   = Math::Vector3::TransformNormal(Math::Vector3(1, 0, 0), playerRot);

    if (input.IsKeyTrigger('1') && m_hasThermometer) m_currentEquipped = Item::Thermometer;
    if (input.IsKeyTrigger('2') && m_hasIncense)      m_currentEquipped = Item::Incense;
    if (input.IsKeyTrigger('3') && m_hasAmulet)       m_currentEquipped = Item::Amulet;

    GhostAI* ghost = nullptr;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            ghost = g;
            break;
        }
    }

    m_temperature = 20.0f;
    if (ghost) {
        if (auto room = ghost->GetTargetRoom()) {
            if (room->IsInside(cTrans.m_position)) {
                m_temperature = 5.0f;
            }
        }
    }

    // --- Use equipped item: right-click ---
    if (input.IsMouseRightTrigger()) {
        if (m_currentEquipped == Item::Incense && ghost) {
            auto& gTrans = ecs.GetComponent<TransformData>(ghost->GetGameObject()->GetEntityID());
            float dist = (cTrans.m_position - gTrans.m_position).Length();
            if (dist < 10.0f) {
                ghost->ApplyStun(5.0f);
                Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense Used: Ghost Stunned!");
                if (m_heldIncenseEntity != INVALID_ENTITY) {
                    AnimationManager::Instance().PlayAnimation(m_heldIncenseEntity, 0, false, 1.0f);
                }
            }
        }
        else if (m_currentEquipped == Item::Amulet && ghost) {
            if (ghost->GetState() == GhostAI::State::Stun) {
                auto& gTrans = ecs.GetComponent<TransformData>(ghost->GetGameObject()->GetEntityID());
                float dist = (cTrans.m_position - gTrans.m_position).Length();
                if (dist < 5.0f) {
                    ghost->Exorcise();
                    Logger::Instance().AddLog(Logger::LogLevel::Info, "Amulet Used: Ghost Exorcised!");
                }
            }
        }
    }

    // --- Move direction ---
    Math::Vector3 moveDir(0, 0, 0);
    if (input.IsKeyHold('W')) moveDir += forward;
    if (input.IsKeyHold('S')) moveDir -= forward;
    if (input.IsKeyHold('A')) moveDir -= right;
    if (input.IsKeyHold('D')) moveDir += right;

    bool isMoving = (moveDir.LengthSquared() > 0.0f);
    if (isMoving) { moveDir.Normalize(); }
    cTrans.m_position += moveDir * m_moveSpeed * GameTimer::Instance().DeltaTime();

    // --- Animation state --- (Idle/Walk/Crouch/CrouchWalk x UseItem_* picked in UpdateAnimationState())
    // Ctrl: toggles crouch on/off.
    bool crouchToggled = false;
    if (input.IsKeyTrigger(VK_CONTROL))
    {
        m_isCrouching = !m_isCrouching;
        crouchToggled = true;
    }

    UpdateAnimationState(isMoving);

    // ���Ⴊ�݋�����炩�ɕ�Ԃ���(�J�����̍���������ɒǏ]������)�B
    // Ctrl���������u�Ԃ̓A�j���[�V�������ɏ������O�������ē����o�����x��邽�߁A
    // �J���������������x�点�Ă��瓮�����n�߂邱�ƂŃY�������킹��B
    if (crouchToggled)
    {
        m_crouchCameraDelayTimer = m_crouchCameraDelay;
    }
    if (m_crouchCameraDelayTimer > 0.0f)
    {
        m_crouchCameraDelayTimer -= GameTimer::Instance().DeltaTime();
    }
    else
    {
        float crouchTarget = m_isCrouching ? 1.0f : 0.0f;
        float t = std::min(1.0f, m_crouchBlendSpeed * GameTimer::Instance().DeltaTime());
        m_crouchBlend += (crouchTarget - m_crouchBlend) * t;
    }

    // --- Jump ---
    if (input.IsKeyTrigger(VK_SPACE) && m_isGrounded) { m_velocityY = 5.0f; }

    // --- Gravity ---
    if (m_useGravity)
    {
        m_velocityY -= m_gravityStrength * GameTimer::Instance().DeltaTime();
        cTrans.m_position.y += m_velocityY * GameTimer::Instance().DeltaTime();
    }

    // --- Ground check via raycast ---
    Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0);
    Math::Vector3 rayDir(0, -1, 0);
    RaycastHit hit;
    {
        PROFILE_CPU_SCOPE("Player::GroundRaycast");
        hit = CollisionManager::Instance().RaycastAgainstMesh(origin, rayDir, 1000.0f, "Stage");
    }

    if (hit.hit && hit.distance <= 0.6f)
    {
        m_isGrounded = true;
        if (m_velocityY < 0.0f)
        {
            m_velocityY = 0.0f;
            cTrans.m_position.y = origin.y - hit.distance;
        }
    }
    else
    {
        m_isGrounded = false;
    }

    // --- F key: open/close nearest door, or pick up the nearest item in view ---
    {
        PROFILE_CPU_SCOPE("Player::UpdateInteractionTarget");
        UpdateInteractionTarget(cTrans.m_position, forward);
    }
    UpdateInteractPrompt();

    static float interactCooldown = 0.0f;
    if (interactCooldown > 0.0f) {
        interactCooldown -= GameTimer::Instance().DeltaTime();
    }

    if (input.IsKeyTrigger('F') && interactCooldown <= 0.0f)
    {
        if (m_interactKind == InteractKind::Pickup && m_interactEntity != INVALID_ENTITY)
        {
            if (auto* scriptData = ecs.TryGetComponent<NativeScriptData>(m_interactEntity)) {
                if (auto* pickup = dynamic_cast<PickupItem*>(scriptData->Instance.get())) {
                    pickup->Collect(this);
                }
            }
        }
        else
        {
            // TryInteractDoor() has always had its own, more forgiving proximity-only door search
            // (no facing-cone requirement). m_interactKind's Door detection is intentionally tighter
            // (used for the F-prompt icon), so gating the actual open/close action behind it made F
            // stop working at approach angles the icon didn't happen to catch. Just always attempt a
            // door interaction here - TryInteractDoor() itself is a no-op if nothing is in range.
            TryInteractDoor(cTrans.m_position);
        }
        interactCooldown = 0.5f; // 0.5s cooldown to avoid repeat-triggering on a held key
    }

    UpdateItemIcons();
    UpdateHeldItem();
    UpdateHeldItemAttach(cTrans.m_position, cTrans.m_rotation.y, isMoving);
    UpdateThermometerAnimation();
}

void Player::TryInteractDoor(const Math::Vector3& playerPos)
{
    auto& ecs = GameManager::Instance().GetECS();
    auto& animMgr = AnimationManager::Instance();

    const float kDoorInteractRange = 3.0f;
    Entity bestEntity = INVALID_ENTITY;
    int    bestAnimIdx = -1;
    float  bestDist = FLT_MAX;

    // Scan all entities for ModelRenderData + TransformData
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel     = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);

        if (!pModel || !pTransform) continue;
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty()) continue;

        // ���I�� AnimationDataComponent ��ǉ�����
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
        if (!pAnim)
        {
            ecs.AddComponent<AnimationDataComponent>(entity, AnimationDataComponent{});
            pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
            if (!pAnim) continue; // just in case
        }

        // Rough distance check against entity position
        Math::Vector3 entityPos = pTransform->m_position;
        float entityDist = (playerPos - entityPos).Length();

        // Find animations whose name contains "Door"
        for (int i = 0; i < (int)anims.size(); ++i)
        {
            const std::string& animName = anims[i].name;
            if (animName.find("Door") == std::string::npos &&
                animName.find("door") == std::string::npos)
            {
                continue;
            }

            // Use the first channel's node to get the door's world position
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

            float dist = (playerPos - doorPos).Length();

            if (dist < kDoorInteractRange && dist < bestDist)
            {
                bestDist = dist;
                bestEntity = entity;
                bestAnimIdx = i;
            }
        }
    }

    if (bestEntity != INVALID_ENTITY && bestAnimIdx >= 0)
    {
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(bestEntity);
        if (!pAnim) return;

        auto it = pAnim->multiAnims.find(bestAnimIdx);
        if (pAnim)
        {
            auto& data = pAnim->multiAnims[bestAnimIdx];
            float currentTime = data.ProgressTime;
            if (!data.IsPlaying && currentTime <= 0.0f)
            {
                // Closed -> play forward to open
                animMgr.PlayMultiAnimation(bestEntity, bestAnimIdx, false, 1.0f);
            }
            else
            {
                // Fully open -> play backward to close
                animMgr.PlayMultiAnimation(bestEntity, bestAnimIdx, false, -1.0f);
                pAnim->multiAnims[bestAnimIdx].ProgressTime = currentTime;
            }

            // �ǉ�: �A�j���[�V��������G���e�B�e�B�͓��I�I�u�W�F�N�g�Ƃ��Ĉ����A
            // �������Z(CollisionManager::Solve)�Ŗ��t���[��AABB���X�V�����悤�ɂ���
            auto* pCollider = ecs.TryGetComponent<ColliderData>(bestEntity);
            if (pCollider && pCollider->m_isStatic)
            {
                pCollider->m_isStatic = false;
                Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Changed entity collider to dynamic for door animation.");
            }
        }
    }
    else
    {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] No valid door in range to interact.");
    }
}

void Player::PostUpdate() {}
void Player::PreDraw() {}
void Player::Draw() {}

void Player::Serialize(nlohmann::json& out) const
{
    out["moveSpeed"] = m_moveSpeed;
    out["useGravity"] = m_useGravity;
    out["gravityStrength"] = m_gravityStrength;
    out["animWalkSpeed"] = m_animWalkSpeed;
    out["crouchBlendSpeed"] = m_crouchBlendSpeed;
    out["crouchCameraDelay"] = m_crouchCameraDelay;
    out["hasThermometer"] = m_hasThermometer;
    out["hasIncense"] = m_hasIncense;
    out["hasAmulet"] = m_hasAmulet;
    out["heldItemOffset"] = { m_heldItemOffset.x, m_heldItemOffset.y, m_heldItemOffset.z };
    out["heldItemOffsetCalibrated"] = m_heldItemOffsetCalibrated;
    out["heldItemExtraRotationDeg"] = { m_heldItemExtraRotationDeg.x, m_heldItemExtraRotationDeg.y, m_heldItemExtraRotationDeg.z };
}

void Player::Deserialize(const nlohmann::json& in)
{
    if (in.contains("moveSpeed"))       m_moveSpeed = in["moveSpeed"];
    if (in.contains("useGravity"))      m_useGravity = in["useGravity"];
    if (in.contains("gravityStrength")) m_gravityStrength = in["gravityStrength"];
    if (in.contains("animWalkSpeed"))       m_animWalkSpeed = in["animWalkSpeed"];
    if (in.contains("crouchBlendSpeed"))    m_crouchBlendSpeed = in["crouchBlendSpeed"];
    if (in.contains("crouchCameraDelay"))   m_crouchCameraDelay = in["crouchCameraDelay"];
    if (in.contains("hasThermometer"))      m_hasThermometer = in["hasThermometer"];
    if (in.contains("hasIncense"))          m_hasIncense = in["hasIncense"];
    if (in.contains("hasAmulet"))           m_hasAmulet = in["hasAmulet"];
    if (in.contains("heldItemOffset")) {
        m_heldItemOffset.x = in["heldItemOffset"][0];
        m_heldItemOffset.y = in["heldItemOffset"][1];
        m_heldItemOffset.z = in["heldItemOffset"][2];
    }
    if (in.contains("heldItemOffsetCalibrated")) m_heldItemOffsetCalibrated = in["heldItemOffsetCalibrated"];
    if (in.contains("heldItemExtraRotationDeg")) {
        m_heldItemExtraRotationDeg.x = in["heldItemExtraRotationDeg"][0];
        m_heldItemExtraRotationDeg.y = in["heldItemExtraRotationDeg"][1];
        m_heldItemExtraRotationDeg.z = in["heldItemExtraRotationDeg"][2];
    }
}

void Player::ImGuiUpdate()
{
    ImGui::DragFloat("Move Speed", &m_moveSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::Checkbox("Use Gravity", &m_useGravity);
    if (m_useGravity)
    {
        ImGui::DragFloat("Gravity Strength", &m_gravityStrength, 0.1f, 0.0f, 100.0f);
    }

    ImGui::Separator();
    ImGui::Text("[Held Item] Offset from body (right/up/forward, meters)");
    ImGui::DragFloat3("Held Item Offset", &m_heldItemOffset.x, 0.01f, -2.0f, 2.0f);
    ImGui::Text("[Held Item] Calibrated from Attach_RightHand: %s", m_heldItemOffsetCalibrated ? "true" : "false");
    if (ImGui::Button("Recalibrate from Attach Point")) {
        auto& ecsCalib = GameManager::Instance().GetECS();
        auto& cTransCalib = ecsCalib.GetComponent<TransformData>(GetGameObject()->GetEntityID());
        m_heldItemOffsetCalibrated = false;
        CalibrateHeldItemOffset(cTransCalib.m_position, cTransCalib.m_rotation.y);
    }
    ImGui::Text("[Held Item] Extra rotation on top of the model (degrees)");
    ImGui::DragFloat3("Held Item Extra Rotation", &m_heldItemExtraRotationDeg.x, 1.0f, -180.0f, 180.0f);

    ImGui::Separator();
    ImGui::Text("[Anim] State: Idle/Walk/Crouch/CrouchWalk, auto UseItem_* prefix while holding an owned item");
    ImGui::DragFloat("Walk Anim Speed", &m_animWalkSpeed, 0.05f, 0.1f, 3.0f);
    ImGui::DragFloat("Crouch Camera Speed", &m_crouchBlendSpeed, 0.1f, 0.5f, 20.0f);
    ImGui::DragFloat("Crouch Camera Delay", &m_crouchCameraDelay, 0.01f, 0.0f, 1.0f);
    ImGui::Text("[Debug] Crouching: %s (blend=%.2f)", m_isCrouching ? "true" : "false", m_crouchBlend);

    ImGui::Separator();
    ImGui::Text("[Debug] Temperature: %.1f C", m_temperature);
    const char* itemName = "Thermometer";
    if (m_currentEquipped == Item::Incense) itemName = "Incense";
    else if (m_currentEquipped == Item::Amulet) itemName = "Amulet";
    ImGui::Text("[Debug] Equipped: %s", itemName);
    ImGui::Text("[Debug] Has: Thermo=%s Incense=%s Amulet=%s",
        m_hasThermometer ? "true" : "false",
        m_hasIncense ? "true" : "false",
        m_hasAmulet ? "true" : "false");
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());
    ImGui::Text("[Debug] Pos: (%.1f, %.1f, %.1f)", cTrans.m_position.x, cTrans.m_position.y, cTrans.m_position.z);
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            if (auto* room = g->GetTargetRoom()) {
                ImGui::Text("[Debug] Room Min: (%.1f,%.1f,%.1f)", room->m_min.x, room->m_min.y, room->m_min.z);
                ImGui::Text("[Debug] Room Max: (%.1f,%.1f,%.1f)", room->m_max.x, room->m_max.y, room->m_max.z);
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), "[Debug] TargetRoom = nullptr!");
            }
            break;
        }
    }
}

void Player::OnCollisionEnter(GameObject* other) {}
void Player::OnCollisionStay(GameObject* other) {}