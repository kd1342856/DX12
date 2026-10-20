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
#include "../System/StimulusSource.h"
#include "../System/LightSwitch.h"
#include "../System/DoorLock.h"
#include "../../../../Framework/DirectX/Utility/Profiler.h"
#include <functional>

REGISTER_COMPONENT(Player);

void Player::Awake() {}

void Player::Start()
{
    // AnimationDataComponentは自分自身ではなく子の"Model"オブジェクトに付いているため、
    // 子階層を探してキャッシュしておく(毎フレーム探索しないように)
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
    CreateFlashLightBeam();
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
        if (obj->GetName() == "SmudgeStickIcon") m_incenseIconEntity = entity;
        else if (obj->GetName() == "OfudaIcon") m_amuletIconEntity = entity;
        else if (obj->GetName() == "FlashLightIcon") m_flashLightIconEntity = entity;
        else if (obj->GetName() == "InteractPrompt") m_interactPromptEntity = entity;
        else if (obj->GetName() == "Crosshair") m_crosshairEntity = entity;
        else {
            for (int i = 0; i < kItemSlotCount; ++i) {
                if (obj->GetName() == "ItemSlot" + std::to_string(i + 1)) m_itemSlotEntities[i] = entity;
            }
        }
    }

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        if (!ecs.TryGetComponent<ModelRenderData>(entity)) continue;
        auto obj = scene->GetGameObject(entity);
        if (!obj) continue;
        if (obj->GetName() == "HeldSmudgeStick") m_heldIncenseEntity = entity;
        else if (obj->GetName() == "HeldOfuda") m_heldAmuletEntity = entity;
    }

    // HeldFlashLight keeps its model on a child object, so the root itself has no ModelRenderData
    // and isn't caught by the loop above - look it up among the scene's root objects by name.
    for (const auto& obj : scene->GetGameObjects())
    {
        if (obj && obj->GetName() == "HeldFlashLight") { m_heldFlashLightEntity = obj->GetEntityID(); break; }
    }
}

void Player::UpdateItemIcons()
{
    // 画面下中央のアイテムスロット: 拾った順(m_itemOrder)に1番から詰めてアイコンを並べ、
    // 装備中のスロットだけ枠を明るい暖色で強調する(キー1〜3も同じ並び)。
    SyncItemOrder();
    auto& ecs = GameManager::Instance().GetECS();

    // まず全アイコンを隠し、所持品だけ該当スロットの位置へ動かして表示する
    for (Item item : { Item::Incense, Item::Amulet, Item::FlashLight }) {
        if (auto* sprite = ecs.TryGetComponent<SpriteData>(GetItemIconEntity(item))) sprite->m_color.w = 0.0f;
    }

    for (int i = 0; i < kItemSlotCount; ++i) {
        const bool filled = i < (int)m_itemOrder.size();
        const Item item = filled ? m_itemOrder[i] : Item::Incense;
        auto* slotTrans = ecs.TryGetComponent<TransformData>(m_itemSlotEntities[i]);

        if (filled) {
            Entity icon = GetItemIconEntity(item);
            if (auto* sprite = ecs.TryGetComponent<SpriteData>(icon)) sprite->m_color.w = 1.0f;
            auto* iconTrans = ecs.TryGetComponent<TransformData>(icon);
            if (iconTrans && slotTrans) {
                iconTrans->m_position.x = slotTrans->m_position.x;
                iconTrans->m_position.y = slotTrans->m_position.y;
            }
        }

        if (auto* sprite = ecs.TryGetComponent<SpriteData>(m_itemSlotEntities[i])) {
            const bool selected = filled && m_currentEquipped == item;
            sprite->m_color = selected ? Math::Vector4(1.0f, 0.85f, 0.55f, 1.0f) : Math::Vector4(1.0f, 1.0f, 1.0f, 0.6f);
        }
    }
}

void Player::SyncItemOrder()
{
    // 捨てた等で所持していないもの・重複を詰め、シーンから読んだ初期所持品など未登録のものを既定順で末尾に足す
    std::vector<Item> kept;
    for (Item item : m_itemOrder) {
        if (HasItem(item) && std::find(kept.begin(), kept.end(), item) == kept.end()) kept.push_back(item);
    }
    m_itemOrder = std::move(kept);
    for (Item item : { Item::Incense, Item::Amulet, Item::FlashLight }) {
        if (HasItem(item) && std::find(m_itemOrder.begin(), m_itemOrder.end(), item) == m_itemOrder.end()) {
            m_itemOrder.push_back(item);
        }
    }
}

Entity Player::GetItemIconEntity(Item item) const
{
    switch (item) {
    case ItemType::Incense:     return m_incenseIconEntity;
    case ItemType::Amulet:      return m_amuletIconEntity;
    case ItemType::FlashLight:  return m_flashLightIconEntity;
    }
    return INVALID_ENTITY;
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
    setVisible(m_heldIncenseEntity, m_hasIncense && m_currentEquipped == Item::Incense);
    setVisible(m_heldAmuletEntity, m_hasAmulet && m_currentEquipped == Item::Amulet);

    if (m_heldFlashLightEntity != INVALID_ENTITY && GetGameObject() && GetGameObject()->GetScene())
    {
        bool visible = m_hasFlashLight && m_currentEquipped == Item::FlashLight;
        // 点灯中はレンズ("Glass"マテリアル)を発光させ、Bloomで光って見えるようにする
        const Math::Vector3 glow = IsHoldingLitFlashLight() ? Math::Vector3(1.0f, 0.97f, 0.9f) * 8.0f : Math::Vector3::Zero;
        std::function<void(GameObject*)> apply = [&](GameObject* obj) {
            setVisible(obj->GetEntityID(), visible);
            if (auto* model = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID())) {
                model->m_glowColor = glow;
                model->m_glowMaterial = "Glass";
            }
            for (auto& child : obj->GetChildren()) apply(child.get());
        };
        if (auto obj = GetGameObject()->GetScene()->GetGameObject(m_heldFlashLightEntity)) apply(obj.get());
    }
}

void Player::CreateFlashLightBeam()
{
    if (m_flashLightBeamEntity != INVALID_ENTITY) return;
    if (!GetGameObject() || !GetGameObject()->GetScene()) return;

    // 実行時だけの光源(Dynamic扱いなのでシーン保存には含まれない)。位置・向きは
    // UpdateFlashLightBeam()が毎フレーム、カメラに合わせてワールド座標で直接書き込む。
    auto beam = GetGameObject()->GetScene()->CreateGameObject("FlashLightBeam");
    beam->SetDynamic(true);
    SpotLightData light;
    light.m_enabled = false;
    light.m_color = { 1.0f, 0.97f, 0.9f };
    light.m_intensity = 6.0f;
    light.m_range = 15.0f;
    light.m_innerAngle = 12.0f;
    light.m_outerAngle = 22.0f;
    GameManager::Instance().GetECS().AddComponent(beam->GetEntityID(), light);
    m_flashLightBeamEntity = beam->GetEntityID();
}

Entity Player::FindGameCameraEntity() const
{
    // GameScene::UpdateCamera()と同じ選び方: デバッグプレビュー用以外で、"MainCamera"(エディタ
    // カメラ)ではないものがゲームカメラ。無ければMainCameraを使う。
    if (!GetGameObject() || !GetGameObject()->GetScene()) return INVALID_ENTITY;
    auto* scene = GetGameObject()->GetScene();
    auto& camArray = GameManager::Instance().GetECS().GetComponentArray<CameraData>();
    Entity fallback = INVALID_ENTITY;
    for (size_t i = 0; i < camArray.GetSize(); ++i)
    {
        if ((camArray.begin() + i)->m_isDebugPreview) continue;
        Entity e = camArray.GetEntityFromIndex(i);
        auto obj = scene->GetGameObject(e);
        if (obj && obj->GetName() == "MainCamera") { fallback = e; continue; }
        return e;
    }
    return fallback;
}

void Player::UpdateFlashLightBeam(const Math::Vector3& playerPos, float playerYaw)
{
    if (m_flashLightBeamEntity == INVALID_ENTITY) CreateFlashLightBeam();
    auto& ecs = GameManager::Instance().GetECS();
    auto* pLight = ecs.TryGetComponent<SpotLightData>(m_flashLightBeamEntity);
    if (!pLight) return;

    bool on = m_hasFlashLight && m_flashLightOn;
    pLight->m_enabled = on;
    // ゴーストの干渉による点滅は見た目だけ(m_flickerOff)。点灯状態(m_enabled)は変えない
    bool flickerOff = m_flashLightFlicker.Update(GameTimer::Instance().DeltaTime());
    pLight->m_flickerOff = on && flickerOff;
    if (!on) return;

    if (m_cameraEntity == INVALID_ENTITY || !ecs.TryGetComponent<CameraData>(m_cameraEntity))
    {
        m_cameraEntity = FindGameCameraEntity();
    }
    auto* pCamTrans = ecs.TryGetComponent<TransformData>(m_cameraEntity);
    auto* pBeamTrans = ecs.TryGetComponent<TransformData>(m_flashLightBeamEntity);
    if (!pCamTrans || !pBeamTrans) { pLight->m_enabled = false; return; }

    // カメラの視線方向(ピッチも含む)に向けて、目の少し右下(手元)から照らす。
    // ビーム自体のTransformは回転なしのままにし、向きはLocalDirectionにワールドの向きをそのまま入れる。
    const Math::Matrix& camWorld = pCamTrans->m_worldMatrix;
    Math::Vector3 camPos = camWorld.Translation();
    Math::Vector3 camForward = camWorld.Backward(); // SimpleMathのBackward()が+Z(=このエンジンの前方)
    Math::Vector3 camRight = camWorld.Right();
    Math::Vector3 camUp = camWorld.Up();
    camForward.Normalize();
    camRight.Normalize();
    camUp.Normalize();

    // 光の出どころは手に持ったフライトライト(UpdateHeldItemAttachと同じ体基準のオフセット)。
    // 少し前に出してレンズ側から出ているようにする。
    // 手のボーンに追従した位置を優先し、取れなければ従来の体基準オフセット。
    Math::Vector3 origin, heldRot;
    if (ComputeHeldItemWorld(Item::FlashLight, playerPos, playerYaw, origin, heldRot))
    {
        origin += camForward * 0.15f;
    }
    else
    {
        Math::Matrix yawMat = Math::Matrix::CreateRotationY(playerYaw);
        origin = playerPos + Math::Vector3::TransformNormal(m_heldItemOffset, yawMat) + camForward * 0.15f;
    }

    // 向きは「カメラが見ている先」へ収束させる(手元から出ても照準位置が照らされるように)。
    const float kAimDistance = 10.0f;
    Math::Vector3 aimPoint = camPos + camForward * kAimDistance;
    Math::Vector3 dir = aimPoint - origin;
    dir.Normalize();

    pBeamTrans->m_position = origin;
    pBeamTrans->m_rotation = Math::Vector3::Zero;
    pBeamTrans->m_scale = Math::Vector3::One;
    pLight->m_localOffset = Math::Vector3::Zero;
    pLight->m_localDirection = dir;
}

void Player::UpdateHeldItemAttach(const Math::Vector3& playerPos, float playerYaw, bool isMoving)
{
    Entity visibleEntity = INVALID_ENTITY;
    if (m_hasIncense && m_currentEquipped == Item::Incense) visibleEntity = m_heldIncenseEntity;
    else if (m_hasAmulet && m_currentEquipped == Item::Amulet) visibleEntity = m_heldAmuletEntity;
    else if (m_hasFlashLight && m_currentEquipped == Item::FlashLight) visibleEntity = m_heldFlashLightEntity;
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

    // 手のボーンがあれば毎フレーム追従する(アニメーションに合わせて持ち物も動く)。
    {
        Math::Vector3 pos, rotRad;
        if (ComputeHeldItemWorld(m_currentEquipped, playerPos, playerYaw, pos, rotRad))
        {
            pHeldTrans->m_position = pos;
            pHeldTrans->m_rotation = rotRad;
            return;
        }
    }

    // ボーンが無い場合のフォールバック: 体基準の固定オフセット(yawで回してプレイヤー位置に加える)。
    Math::Matrix yawMat = Math::Matrix::CreateRotationY(playerYaw);
    pHeldTrans->m_position = playerPos + Math::Vector3::TransformNormal(m_heldItemOffset, yawMat);
    const float kDegToRad = 3.14159265f / 180.0f;
    pHeldTrans->m_rotation = Math::Vector3(
        m_heldItemExtraRotationDeg.x * kDegToRad,
        playerYaw + m_heldItemExtraRotationDeg.y * kDegToRad,
        m_heldItemExtraRotationDeg.z * kDegToRad);
}

bool Player::GetHandWorld(const Math::Vector3& playerPos, float playerYaw, Math::Vector3& outPos, Math::Quaternion& outRot) const
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
    if (nodeIndex < 0) return false;

    // m_worldMatrixはTransformSystemが1フレームに1回しか更新しないので、CalibrateHeldItemOffset同様
    // ローカルのPosition/Rotation/Scaleとプレイヤーの位置・yawから自前でワールド行列を組む。
    Math::Matrix modelLocalMat = Math::Matrix::CreateScale(pModelTrans->m_scale)
        * Math::Matrix::CreateFromYawPitchRoll(pModelTrans->m_rotation.y, pModelTrans->m_rotation.x, pModelTrans->m_rotation.z)
        * Math::Matrix::CreateTranslation(pModelTrans->m_position);
    Math::Matrix playerWorldMat = Math::Matrix::CreateRotationY(playerYaw) * Math::Matrix::CreateTranslation(playerPos);
    Math::Matrix handWorld = nodes[nodeIndex].globalTransform * modelLocalMat * playerWorldMat;

    Math::Vector3 scale;
    if (!handWorld.Decompose(scale, outRot, outPos)) return false;
    outRot.Normalize();
    return true;
}

bool Player::ComputeHeldItemWorld(Item item, const Math::Vector3& playerPos, float playerYaw, Math::Vector3& outPos, Math::Vector3& outRotRad) const
{
    int idx = (int)item;
    if (idx < 1 || idx > 4) return false;

    Math::Vector3 handPos;
    Math::Quaternion handRot;
    if (!GetHandWorld(playerPos, playerYaw, handPos, handRot)) return false;

    const HeldPose& pose = m_heldPose[idx];
    const float kDegToRad = 3.14159265f / 180.0f;
    // 行ベクトル規約: ローカル回転を先に、親(手)の回転を後に掛ける。
    Math::Quaternion localRot = Math::Quaternion::CreateFromYawPitchRoll(
        pose.rotDeg.y * kDegToRad, pose.rotDeg.x * kDegToRad, pose.rotDeg.z * kDegToRad);
    Math::Quaternion worldRot = localRot * handRot;
    worldRot.Normalize();

    outPos = handPos + Math::Vector3::Transform(pose.pos, handRot);
    outRotRad = Math::Matrix::CreateFromQuaternion(worldRot).ToEuler(); // (pitch,yaw,roll) = (x,y,z)
    return true;
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

    float stimulusDist = FLT_MAX;
    Entity stimulusEntity = FindNearestStimulus(playerPos, forward, kInteractRange, stimulusDist);
    if (stimulusEntity != INVALID_ENTITY && stimulusDist < pickupDist)
    {
        // ピックアップとランプ/ラジカセが両方範囲内なら近い方を対象にする
        pickupEntity = INVALID_ENTITY;
        pickupDist = stimulusDist;
    }
    else
    {
        stimulusEntity = INVALID_ENTITY;
    }

    if (hasDoor && ((pickupEntity == INVALID_ENTITY && stimulusEntity == INVALID_ENTITY) || doorDist <= pickupDist))
    {
        m_interactKind = InteractKind::Door;
        m_interactEntity = INVALID_ENTITY;
    }
    else if (pickupEntity != INVALID_ENTITY)
    {
        m_interactKind = InteractKind::Pickup;
        m_interactEntity = pickupEntity;
    }
    else if (stimulusEntity != INVALID_ENTITY)
    {
        m_interactKind = InteractKind::Stimulus;
        m_interactEntity = stimulusEntity;
    }
    else
    {
        m_interactKind = InteractKind::None;
        m_interactEntity = INVALID_ENTITY;
    }
}

void Player::UpdateInteractPrompt()
{
    auto& ecs = GameManager::Instance().GetECS();
    const float alpha = (m_interactKind != InteractKind::None) ? 1.0f : 0.0f;
    if (auto* sprite = ecs.TryGetComponent<SpriteData>(m_interactPromptEntity))
    {
        sprite->m_color.w = alpha;
    }
    if (auto* sprite = ecs.TryGetComponent<SpriteData>(m_crosshairEntity))
    {
        sprite->m_color.w = alpha;
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
            if (dir.Dot(forward) < 0.5f) continue; // 正面方向の円錐の外(半角60度)
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
            if (dir.Dot(forward) < 0.5f) continue; // 正面方向の円錐の外(半角60度)
        }
        if (dist < outDist) { outDist = dist; best = obj->GetEntityID(); }
    }
    return best;
}

Entity Player::FindNearestStimulus(const Math::Vector3& playerPos, const Math::Vector3& forward, float maxRange, float& outDist) const
{
    auto& ecs = GameManager::Instance().GetECS();
    Entity best = INVALID_ENTITY;
    outDist = FLT_MAX;

    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
    {
        // 照明/ラジカセ本体(StimulusSource)と、廊下照明などの壁スイッチ(LightSwitch)が対象
        GameObject* obj = nullptr;
        Math::Vector3 srcPos;
        // 遮蔽判定のレイを発生源の何m手前で止めるか
        float rayStopMargin = 0.3f;
        if (auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get())) {
            if (!src->m_interactable) continue;
            obj = src->GetGameObject();
            srcPos = src->GetWorldPosition();
        }
        else if (auto* sw = dynamic_cast<LightSwitch*>(scriptData.Instance.get())) {
            if (!sw->m_interactable) continue;
            obj = sw->GetGameObject();
            srcPos = sw->GetWorldPosition();
            // 壁スイッチは壁に貼り付いているので、0.3m手前で止めると裏側の壁(厚さ約0.2m)を素通りしてしまう
            rayStopMargin = 0.05f;
            // スイッチの正面(ローカル-Z)側にいる時だけ操作できる(壁の裏側からは不可)
            if (auto* pSwTrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID())) {
                const auto& m = pSwTrans->m_worldMatrix;
                Math::Vector3 front(-m._31, -m._32, -m._33);
                front.y = 0.0f;
                Math::Vector3 toPlayer = playerPos - srcPos;
                toPlayer.y = 0.0f;
                if (front.LengthSquared() > 1e-6f && toPlayer.Dot(front) <= 0.0f) continue;
            }
        }
        if (!obj) continue;

        // 天井照明のように頭より高い位置にある物も操作できるよう、高さは無視して水平距離・水平方向で判定する
        Math::Vector3 toSrc = srcPos - playerPos;
        toSrc.y = 0.0f;
        float dist = toSrc.Length();
        if (dist >= maxRange || dist >= outDist) continue;
        if (dist > 0.001f)
        {
            Math::Vector3 dir = toSrc;
            dir.Normalize();
            if (dir.Dot(forward) < 0.5f) continue; // 正面方向の円錐の外(半角60度)
        }

        // 高さを無視しているので、真上/真下の別の階にある照明も同じ距離に見えてしまう。
        // 目の高さから発生源までの間に床・天井(Stageメッシュ)があれば対象外にする。
        Math::Vector3 eyePos = playerPos + Math::Vector3(0.0f, 1.5f, 0.0f);
        Math::Vector3 toSrc3D = srcPos - eyePos;
        float dist3D = toSrc3D.Length();
        if (dist3D > rayStopMargin + 0.05f)
        {
            toSrc3D /= dist3D;
            // 発生源のすぐ手前で止める(天井照明は天井面に接しているので、天井自体に当たらないよう余裕を持たせる)
            RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(eyePos, toSrc3D, dist3D - rayStopMargin, "Stage");
            if (hit.hit) continue;
        }

        outDist = dist;
        best = obj->GetEntityID();
    }
    return best;
}

bool Player::IsHoldingVisibleItem() const
{
    return (m_hasIncense && m_currentEquipped == Item::Incense)
        || (m_hasAmulet && m_currentEquipped == Item::Amulet)
        || (m_hasFlashLight && m_currentEquipped == Item::FlashLight);
}

void Player::UpdateAnimationState(bool isMoving)
{
    if (m_animEntity == INVALID_ENTITY) return;

    const char* clip;
    if (m_isCrouching) clip = isMoving ? "CrouchWalk" : "Crouch";
    else                clip = isMoving ? "Walk" : "Idle";

    std::string clipName = clip;
    if (IsHoldingVisibleItem()) clipName = std::string("UseItem_") + clip;

    // 停止状態のしゃがみ(Crouch)は「立ち→しゃがみ」の遷移アニメなので、ループせず最終ポーズで固定する。
    // 再生終了後(IsPlaying=false)に毎フレーム呼ぶと先頭から再生し直してしまうため、クリップが変わった時だけ再生する。
    const bool holdLastPose = (std::string(clip) == "Crouch");
    if (holdLastPose && clipName == m_lastAnimClip) return;
    // しゃがみ歩き→停止の場合は既にしゃがんでいるので、遷移アニメを飛ばして最終ポーズから始める。
    const bool fromCrouchWalk = holdLastPose && m_lastAnimClip.find("CrouchWalk") != std::string::npos;
    m_lastAnimClip = clipName;

    float speed = isMoving ? m_animWalkSpeed : 1.0f;
    AnimationManager::Instance().PlayAnimationByName(m_animEntity, clipName, !holdLastPose, speed);

    if (fromCrouchWalk)
    {
        auto& ecs = GameManager::Instance().GetECS();
        auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(m_animEntity);
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_animEntity);
        if (pAnim && pModel && pModel->m_spModelData)
        {
            const auto& anims = pModel->m_spModelData->GetAnimations();
            int idx = pAnim->currentAnim.AnimationIndex;
            // 終端の直前へ。次の更新で終端に達して最終ポーズのまま停止する。
            if (idx >= 0 && idx < (int)anims.size())
                pAnim->currentAnim.ProgressTime = anims[idx].duration - 0.001f;
        }
    }
}

void Player::AddItem(ItemType item)
{
    bool alreadyHad = HasItem(item);
    // 所持フラグを立てる前に既存の所持品を登録しておき、拾ったものはその後ろ(末尾)に入れる。
    // (フラグを立てた後にSyncすると、そこで一度追加された上にpush_backでも追加されて重複していた)
    SyncItemOrder();
    switch (item) {
    case ItemType::Incense:     m_hasIncense = true; break;
    case ItemType::Amulet:      m_hasAmulet = true; break;
    case ItemType::FlashLight:  m_hasFlashLight = true; break;
    }
    if (!alreadyHad) {
        m_itemOrder.push_back(item); // 拾ったものをスロットの末尾へ
        m_currentEquipped = item; // auto-equip on first pickup
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Item acquired.");
    }
}

void Player::DropEquippedItem(const Math::Vector3& playerPos, const Math::Vector3& forward, float playerYaw)
{
    Item item = m_currentEquipped;
    if (!HasItem(item)) return;

    const char* prefabPath = nullptr;
    switch (item) {
    case ItemType::Incense:     prefabPath = "Asset/Data/Prefab/SmudgeStick.json"; break;
    case ItemType::Amulet:      prefabPath = "Asset/Data/Prefab/Ofuda.json"; break;
    case ItemType::FlashLight:  prefabPath = "Asset/Data/Prefab/FlashLight.json"; break;
    }
    auto* scene = GetGameObject() ? GetGameObject()->GetScene() : nullptr;
    if (!prefabPath || !scene) return;

    // 手元(腰〜胸の高さ、少し前)から、視線の方向へ軽く放る。手元のすぐ先が壁なら壁の手前から出す。
    const float kHandHeight = 1.1f;
    const float kHandForward = 0.35f;
    const float kWallClearance = 0.15f;
    const float kThrowSpeed = 2.5f;   // 視線方向の初速(m/s)
    const float kThrowUpSpeed = 1.2f; // 少し山なりにするための上向きの初速(m/s)
    auto& cm = CollisionManager::Instance();
    Math::Vector3 handBase = playerPos + Math::Vector3(0, kHandHeight, 0);
    float handDist = kHandForward;
    RaycastHit wallHit = cm.RaycastAgainstMesh(handBase, forward, kHandForward + kWallClearance, "Stage");
    if (wallHit.hit) handDist = (std::max)(0.0f, wallHit.distance - kWallClearance);
    Math::Vector3 spawnPos = handBase + forward * handDist;

    // 投げる向きはカメラの視線(上下も含む)。カメラが見つからなければ体の向き。
    Math::Vector3 throwDir = forward;
    if (m_cameraEntity == INVALID_ENTITY) m_cameraEntity = FindGameCameraEntity();
    if (auto* pCamTrans = GameManager::Instance().GetECS().TryGetComponent<TransformData>(m_cameraEntity)) {
        Math::Vector3 camForward = pCamTrans->m_worldMatrix.Backward();
        if (camForward.LengthSquared() > 0.0001f) { camForward.Normalize(); throwDir = camForward; }
    }

    // Instantiate()で出したものはDynamic扱いになり、シーン保存には含まれない(実行中だけの落とし物)
    auto obj = scene->Instantiate(prefabPath, spawnPos);
    if (!obj) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "[Player] Drop failed: could not instantiate %s", prefabPath);
        return;
    }
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pTrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID())) {
        pTrans->m_rotation.y = playerYaw; // プレイヤーの向きに合わせて放る
    }
    if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(obj->GetEntityID())) {
        if (auto* pickup = dynamic_cast<PickupItem*>(pScript->Instance.get())) {
            pickup->Launch(throwDir * kThrowSpeed + Math::Vector3(0, kThrowUpSpeed, 0));
            // 点灯したまま落としたら、落ちたライトも点灯したままにする
            if (item == ItemType::FlashLight) pickup->SetLightOn(m_flashLightOn);
        }
    }

    switch (item) {
    case ItemType::Incense:     m_hasIncense = false; break;
    case ItemType::Amulet:      m_hasAmulet = false; break;
    case ItemType::FlashLight:  m_hasFlashLight = false; m_flashLightOn = false; break;
    }

    // 残っているアイテムがあればスロットの先頭のものに持ち替える(何も無ければ手ぶら)
    SyncItemOrder();
    if (!m_itemOrder.empty()) m_currentEquipped = m_itemOrder.front();
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Item dropped.");
}

bool Player::HasItem(ItemType item) const
{
    switch (item) {
    case ItemType::Incense:     return m_hasIncense;
    case ItemType::Amulet:      return m_hasAmulet;
    case ItemType::FlashLight:  return m_hasFlashLight;
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

    // キー1〜4は画面下のスロット番号(拾った順)に対応
    SyncItemOrder();
    for (int i = 0; i < (int)m_itemOrder.size() && i < kItemSlotCount; ++i) {
        if (input.IsKeyTrigger('1' + i)) m_currentEquipped = m_itemOrder[i];
    }

    // FlashLight: T toggles it whenever owned (even while another item is equipped)
    if (input.IsKeyTrigger('T') && m_hasFlashLight) SetFlashLightOnWithSound(!m_flashLightOn);

    // G: drop the equipped item in front of the player
    if (input.IsKeyTrigger('G')) DropEquippedItem(cTrans.m_position, forward, cTrans.m_rotation.y);

    GhostAI* ghost = nullptr;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            ghost = g;
            break;
        }
    }

    // --- 落ちているフラッシュライトを左クリックでオンオフ(Fで拾える範囲・正面にあるもの) ---
    if (input.IsMouseLeftTrigger() && m_interactKind == InteractKind::Pickup && m_interactEntity != INVALID_ENTITY)
    {
        if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(m_interactEntity))
        {
            auto* pickup = dynamic_cast<PickupItem*>(pScript->Instance.get());
            if (pickup && pickup->m_itemType == ItemType::FlashLight) {
                pickup->ToggleLight();
                AudioManager::Instance().PlaySE(pickup->IsLightOn() ? "Asset/Sound/SE/kaityudento_on.wav" : "Asset/Sound/SE/kaityudento_off.wav", m_flashLightSoundVolume);
            }
        }
    }

    // --- Use equipped item: right-click ---
    // (G で全部落とした後は m_currentEquipped が手放したアイテムのまま残るので、所持も確認する)
    if (input.IsMouseRightTrigger() && HasItem(m_currentEquipped)) {
        if (m_currentEquipped == Item::Incense && ghost) {
            auto& gTrans = ecs.GetComponent<TransformData>(ghost->GetGameObject()->GetEntityID());
            float dist = (cTrans.m_position - gTrans.m_position).Length();
            if (dist < 10.0f) {
                // 効くかどうかはゴーストの種類ごとの条件次第(影女は近くで明かりが点いている時だけ等)
                if (ghost->TryIncenseStun(5.0f)) {
                    Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense Used: Ghost Stunned!");
                } else {
                    Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense Used: no effect on this ghost right now.");
                }
                if (m_heldIncenseEntity != INVALID_ENTITY) {
                    AnimationManager::Instance().PlayAnimation(m_heldIncenseEntity, 0, false, 1.0f);
                }
                // 燻る音: 使うたびに頭から鳴らし直す
                if (!m_smudgeSound) m_smudgeSound = AudioManager::Instance().CreateSEInstance("Asset/Sound/SE/smadge.wav");
                if (m_smudgeSound) {
                    m_smudgeSound->Stop();
                    m_smudgeSound->SetVolume(m_smudgeSoundVolume * AudioManager::Instance().GetSEVolume());
                    m_smudgeSound->Play(false);
                    m_smudgeSoundTimer = m_smudgeSoundDuration;
                }
            }
        }
        else if (m_currentEquipped == Item::FlashLight && m_hasFlashLight) {
            SetFlashLightOnWithSound(!m_flashLightOn);
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
    cTrans.m_position += moveDir * (m_moveSpeedKmh / 3.6f) * GameTimer::Instance().DeltaTime();

    // --- Animation state --- (Idle/Walk/Crouch/CrouchWalk x UseItem_* picked in UpdateAnimationState())
    // Ctrl: toggles crouch on/off.
    bool crouchToggled = false;
    if (input.IsKeyTrigger(VK_CONTROL))
    {
        m_isCrouching = !m_isCrouching;
        crouchToggled = true;
    }

    UpdateAnimationState(isMoving);

    // しゃがみ具合を滑らかに補間する(カメラの高さもこれに追従させる)。
    // Ctrlを押した瞬間はアニメーション側に少し溜めがあって動き出しが遅れるため、
    // カメラも少しだけ遅らせてから動き始めることでズレを合わせる。
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

    if (m_interactCooldown > 0.0f) {
        m_interactCooldown -= GameTimer::Instance().DeltaTime();
    }

    if (input.IsKeyTrigger('F') && m_interactCooldown <= 0.0f)
    {
        if (m_interactKind == InteractKind::Pickup && m_interactEntity != INVALID_ENTITY)
        {
            if (auto* scriptData = ecs.TryGetComponent<NativeScriptData>(m_interactEntity)) {
                if (auto* pickup = dynamic_cast<PickupItem*>(scriptData->Instance.get())) {
                    pickup->Collect(this);
                }
            }
        }
        else if (m_interactKind == InteractKind::Stimulus && m_interactEntity != INVALID_ENTITY)
        {
            if (auto* scriptData = ecs.TryGetComponent<NativeScriptData>(m_interactEntity)) {
                if (auto* src = dynamic_cast<StimulusSource*>(scriptData->Instance.get())) {
                    src->Toggle();
                    src->PlayToggleSound(); // 紐スイッチの点灯/消灯音
                }
                else if (auto* sw = dynamic_cast<LightSwitch*>(scriptData->Instance.get())) {
                    sw->Toggle();
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
        m_interactCooldown = 0.5f; // 0.5s cooldown to avoid repeat-triggering on a held key
    }

    UpdateItemIcons();
    UpdateHeldItem();
    UpdateHeldItemAttach(cTrans.m_position, cTrans.m_rotation.y, isMoving);
    UpdateFlashLightBeam(cTrans.m_position, cTrans.m_rotation.y);

    // 耳の位置(おおよその目線の高さ)をリスナーにして、ゴーストや照明の音を距離・方向で鳴らし分ける
    AudioManager::Instance().SetListener(cTrans.m_position + Math::Vector3(0, 1.6f, 0), forward);
    UpdateSounds(isMoving, GameTimer::Instance().DeltaTime());
}

void Player::SetFlashLightOnWithSound(bool on)
{
    if (m_flashLightOn == on) return;
    m_flashLightOn = on;
    AudioManager::Instance().PlaySE(on ? "Asset/Sound/SE/kaityudento_on.wav" : "Asset/Sound/SE/kaityudento_off.wav", m_flashLightSoundVolume);
}

FloorType Player::DetectFloorType(const Math::Vector3& pos) const
{
    // 足元ぴったりだと部屋の床(minY)より僅かに下になることがあるので少し持ち上げて判定する
    const Math::Vector3 p = pos + Math::Vector3(0.0f, 0.3f, 0.0f);
    Math::Vector3 houseMin(FLT_MAX, FLT_MAX, FLT_MAX), houseMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool anyRoom = false;

    auto& ecs = GameManager::Instance().GetECS();
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        auto* room = dynamic_cast<RoomArea*>(scriptData.Instance.get());
        if (!room) continue;
        if (room->IsInside(p)) return room->m_floorType;
        houseMin = Math::Vector3::Min(houseMin, room->m_min);
        houseMax = Math::Vector3::Max(houseMax, room->m_max);
        anyRoom = true;
    }

    // どの部屋にも入っていなくても、家の範囲内なら部屋同士の隙間(壁の厚み・ドア枠)なので直前の床のまま。
    // 家の範囲の外に出たら庭(土)
    const float margin = 0.3f;
    const bool insideHouse = anyRoom &&
        p.x >= houseMin.x - margin && p.x <= houseMax.x + margin &&
        p.z >= houseMin.z - margin && p.z <= houseMax.z + margin;
    return insideHouse ? m_currentFloor : FloorType::Soil;
}

void Player::UpdateSounds(bool isMoving, float deltaTime)
{
    // --- 足音: 接地して歩いている間だけ、今いる床の種類の音をループ再生。しゃがみ歩きは小さく、ゆっくりめに ---
    static const char* kFootstepPaths[3] = {
        "Asset/Sound/SE/wood-walk.wav",   // FloorType::Wood
        "Asset/Sound/SE/tatami-walk.wav", // FloorType::Tatami
        "Asset/Sound/SE/soil-walk.wav",   // FloorType::Soil
    };
    const bool walking = isMoving && m_isGrounded;
    if (walking)
    {
        auto& cTrans = GameManager::Instance().GetECS().GetComponent<TransformData>(GetGameObject()->GetEntityID());
        m_currentFloor = DetectFloorType(cTrans.m_position);
    }
    for (int i = 0; i < 3; ++i)
    {
        auto& sound = m_footstepSounds[i];
        const bool active = walking && i == static_cast<int>(m_currentFloor);
        if (active)
        {
            if (!sound) sound = AudioManager::Instance().CreateSEInstance(kFootstepPaths[i]);
            if (!sound) continue;
            float vol = m_footstepVolumes[i] * (m_isCrouching ? m_crouchFootstepScale : 1.0f);
            sound->SetVolume(vol * AudioManager::Instance().GetSEVolume());
            sound->SetPitch(m_isCrouching ? -0.25f : 0.0f);
            if (sound->GetState() != DirectX::SoundState::PLAYING) sound->Play(true);
        }
        else if (sound && sound->GetState() == DirectX::SoundState::PLAYING)
        {
            // 止めると次に歩き出した時(床が変わった時)に頭から(1歩目から)鳴る
            sound->Stop();
        }
    }

    // --- お香: 一定時間鳴らしたら最後の1秒でフェードアウトして止める ---
    if (m_smudgeSound && m_smudgeSoundTimer > 0.0f)
    {
        m_smudgeSoundTimer -= deltaTime;
        if (m_smudgeSoundTimer <= 0.0f) m_smudgeSound->Stop();
        else if (m_smudgeSoundTimer < 1.0f) m_smudgeSound->SetVolume(m_smudgeSoundVolume * AudioManager::Instance().GetSEVolume() * m_smudgeSoundTimer);
    }
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

        // 動的に AnimationDataComponent を追加する
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

    if (bestEntity != INVALID_ENTITY && bestAnimIdx >= 0 && DoorLock::IsLocked(bestEntity, bestAnimIdx))
    {
        // ハント中は玄関ドアが施錠されている(GhostAIが施錠/解錠する)
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] The door is locked.");
        AudioManager::Instance().PlaySE("Asset/Sound/SE/door_nobu.wav", m_doorLockedSoundVolume); // ドアノブをガチャガチャ
        return;
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
            const bool opening = (!data.IsPlaying && currentTime <= 0.0f);
            if (opening)
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
            // 開け閉めどちらも同じ軋み音(閉める時は少し低めにして聞き分けられるようにする)
            AudioManager::Instance().PlaySE("Asset/Sound/SE/door_open.wav", m_doorSoundVolume, opening ? 0.0f : -0.15f);

            // 追加: アニメーションするエンティティは動的オブジェクトとして扱い、
            // 衝突判定(CollisionManager::Solve)で毎フレームAABBが更新されるようにする
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
    out["moveSpeedKmh"] = m_moveSpeedKmh;
    out["useGravity"] = m_useGravity;
    out["gravityStrength"] = m_gravityStrength;
    out["animWalkSpeed"] = m_animWalkSpeed;
    out["crouchBlendSpeed"] = m_crouchBlendSpeed;
    out["crouchCameraDelay"] = m_crouchCameraDelay;
    out["footstepVolume"] = m_footstepVolumes[0];
    out["tatamiFootstepVolume"] = m_footstepVolumes[1];
    out["soilFootstepVolume"] = m_footstepVolumes[2];
    out["crouchFootstepScale"] = m_crouchFootstepScale;
    out["flashLightSoundVolume"] = m_flashLightSoundVolume;
    out["doorSoundVolume"] = m_doorSoundVolume;
    out["doorLockedSoundVolume"] = m_doorLockedSoundVolume;
    out["smudgeSoundVolume"] = m_smudgeSoundVolume;
    out["smudgeSoundDuration"] = m_smudgeSoundDuration;
    out["hasIncense"] = m_hasIncense;
    out["hasAmulet"] = m_hasAmulet;
    out["hasFlashLight"] = m_hasFlashLight;
    // アイテムスロットの並び(拾った順)。ItemTypeの値(1〜4)の配列で保存する
    {
        std::vector<Item> order = m_itemOrder;
        order.erase(std::remove_if(order.begin(), order.end(), [&](Item item) { return !HasItem(item); }), order.end());
        nlohmann::json orderJson = nlohmann::json::array();
        for (Item item : order) orderJson.push_back(static_cast<int>(item));
        out["itemOrder"] = orderJson;
    }
    out["heldItemOffset"] = { m_heldItemOffset.x, m_heldItemOffset.y, m_heldItemOffset.z };
    out["heldItemOffsetCalibrated"] = m_heldItemOffsetCalibrated;
    out["heldItemExtraRotationDeg"] = { m_heldItemExtraRotationDeg.x, m_heldItemExtraRotationDeg.y, m_heldItemExtraRotationDeg.z };
    for (int i = (int)Item::Incense; i <= (int)Item::FlashLight; ++i)
    {
        const HeldPose& p = m_heldPose[i];
        out["heldPose" + std::to_string(i)] = { p.pos.x, p.pos.y, p.pos.z, p.rotDeg.x, p.rotDeg.y, p.rotDeg.z };
    }
}

void Player::Deserialize(const nlohmann::json& in)
{
    if (in.contains("moveSpeed"))       m_moveSpeedKmh = in["moveSpeed"].get<float>() * 3.6f; // 旧形式(m/s)
    if (in.contains("moveSpeedKmh"))    m_moveSpeedKmh = in["moveSpeedKmh"];
    if (in.contains("useGravity"))      m_useGravity = in["useGravity"];
    if (in.contains("gravityStrength")) m_gravityStrength = in["gravityStrength"];
    if (in.contains("animWalkSpeed"))       m_animWalkSpeed = in["animWalkSpeed"];
    if (in.contains("crouchBlendSpeed"))    m_crouchBlendSpeed = in["crouchBlendSpeed"];
    if (in.contains("crouchCameraDelay"))   m_crouchCameraDelay = in["crouchCameraDelay"];
    if (in.contains("footstepVolume"))          m_footstepVolumes[0] = in["footstepVolume"];
    if (in.contains("tatamiFootstepVolume"))    m_footstepVolumes[1] = in["tatamiFootstepVolume"];
    if (in.contains("soilFootstepVolume"))      m_footstepVolumes[2] = in["soilFootstepVolume"];
    if (in.contains("crouchFootstepScale"))     m_crouchFootstepScale = in["crouchFootstepScale"];
    if (in.contains("flashLightSoundVolume"))   m_flashLightSoundVolume = in["flashLightSoundVolume"];
    if (in.contains("doorSoundVolume"))         m_doorSoundVolume = in["doorSoundVolume"];
    if (in.contains("doorLockedSoundVolume"))   m_doorLockedSoundVolume = in["doorLockedSoundVolume"];
    if (in.contains("smudgeSoundVolume"))       m_smudgeSoundVolume = in["smudgeSoundVolume"];
    if (in.contains("smudgeSoundDuration"))     m_smudgeSoundDuration = in["smudgeSoundDuration"];
    if (in.contains("hasIncense"))          m_hasIncense = in["hasIncense"];
    if (in.contains("hasAmulet"))           m_hasAmulet = in["hasAmulet"];
    if (in.contains("hasFlashLight"))       m_hasFlashLight = in["hasFlashLight"];
    if (in.contains("itemOrder") && in["itemOrder"].is_array()) {
        m_itemOrder.clear();
        for (const auto& v : in["itemOrder"]) {
            if (!v.is_number_integer()) continue;
            int n = v.get<int>();
            if (n >= static_cast<int>(Item::Incense) && n <= static_cast<int>(Item::FlashLight)) {
                m_itemOrder.push_back(static_cast<Item>(n));
            }
        }
        // 所持フラグと食い違う分(重複・未所持・抜け)はここで補正する
        SyncItemOrder();
    }
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
    for (int i = (int)Item::Incense; i <= (int)Item::FlashLight; ++i)
    {
        std::string key = "heldPose" + std::to_string(i);
        if (in.contains(key) && in[key].size() >= 6)
        {
            const auto& a = in[key];
            m_heldPose[i].pos    = { a[0], a[1], a[2] };
            m_heldPose[i].rotDeg = { a[3], a[4], a[5] };
        }
    }
}

void Player::ImGuiUpdate()
{
    ImGui::DragFloat("Move Speed (km/h)", &m_moveSpeedKmh, 0.1f, 0.1f, 100.0f, "%.1f km/h");
    ImGui::Checkbox("Use Gravity", &m_useGravity);
    if (m_useGravity)
    {
        ImGui::DragFloat("Gravity Strength", &m_gravityStrength, 0.1f, 0.0f, 100.0f);
    }

    ImGui::Separator();
    ImGui::Text("[Held Item] Attach_RightHand bone-local pose per item (tracks the hand every frame)");
    {
        static const char* kNames[5] = { "", "", "Incense", "Amulet", "FlashLight" };
        for (int i = (int)Item::Incense; i <= (int)Item::FlashLight; ++i)
        {
            ImGui::PushID(i);
            ImGui::Text("%s", kNames[i]);
            ImGui::DragFloat3("Hand Offset (m)", &m_heldPose[i].pos.x, 0.005f, -1.0f, 1.0f);
            ImGui::DragFloat3("Hand Rotation (deg)", &m_heldPose[i].rotDeg.x, 1.0f, -180.0f, 180.0f);
            ImGui::PopID();
        }
    }
    ImGui::Separator();
    ImGui::Text("[Held Item] Fallback (no hand bone): offset from body (right/up/forward, meters)");
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
    ImGui::Text("[Sound] Volume (0-1)");
    ImGui::SliderFloat("Footstep (Wood)", &m_footstepVolumes[0], 0.0f, 1.0f);
    ImGui::SliderFloat("Footstep (Tatami)", &m_footstepVolumes[1], 0.0f, 1.0f);
    ImGui::SliderFloat("Footstep (Soil)", &m_footstepVolumes[2], 0.0f, 1.0f);
    ImGui::SliderFloat("Crouch Footstep Scale", &m_crouchFootstepScale, 0.0f, 1.0f);
    {
        static const char* kFloorNames[3] = { "Wood", "Tatami", "Soil" };
        ImGui::Text("[Debug] Floor: %s", kFloorNames[static_cast<int>(m_currentFloor)]);
    }
    ImGui::SliderFloat("FlashLight Switch", &m_flashLightSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Door Open/Close", &m_doorSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Door Locked", &m_doorLockedSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Smudge Stick", &m_smudgeSoundVolume, 0.0f, 1.0f);
    ImGui::DragFloat("Smudge Sound Duration (s)", &m_smudgeSoundDuration, 0.1f, 1.0f, 40.0f);

    ImGui::Separator();
    const char* itemName = "Incense";
    if (m_currentEquipped == Item::Amulet) itemName = "Amulet";
    else if (m_currentEquipped == Item::FlashLight) itemName = "FlashLight";
    ImGui::Text("[Debug] Equipped: %s", itemName);
    ImGui::Text("[Debug] Has: Incense=%s Amulet=%s FlashLight=%s",
        m_hasIncense ? "true" : "false",
        m_hasAmulet ? "true" : "false",
        m_hasFlashLight ? "true" : "false");
    ImGui::Text("[Debug] FlashLight: %s (T / right-click to toggle)", m_flashLightOn ? "ON" : "OFF");
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