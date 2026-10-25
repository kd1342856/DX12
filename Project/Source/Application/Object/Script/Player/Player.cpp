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
#include "../System/HidingSpot.h"
#include "../../../../Framework/Manager/Asset/MeshManager.h"
#include "../../../../Framework/Manager/Animation/OpenableAnim.h"
#include "../../../../Framework/DirectX/Utility/Profiler.h"
#include "../../../../Graphics/Shader/PhotometricUnits.h"
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

    ApplyStartInventory();
    FindItemIconEntities();
    CreateFlashLightBeam();
    CacheStandCapsule();
}

void Player::CacheStandCapsule()
{
    // 当たり判定は子の"Collider"に付いている
    m_colliderEntity = INVALID_ENTITY;
    m_standCapsuleValid = false;
    if (!GetGameObject()) return;
    auto& ecs = GameManager::Instance().GetECS();
    for (auto& child : GetGameObject()->GetChildren()) {
        if (child->GetName() != "Collider") continue;
        auto* pCollider = ecs.TryGetComponent<ColliderData>(child->GetEntityID());
        if (!pCollider) continue;
        for (auto& shape : pCollider->m_shapes) {
            auto* capsule = dynamic_cast<CollisionShapeCapsule*>(shape.get());
            if (!capsule) continue;
            m_colliderEntity = child->GetEntityID();
            m_standCapsuleOffset = capsule->m_offset;
            m_standCapsuleHeight = capsule->height;
            m_standCapsuleRadius = capsule->radius;
            m_standCapsuleValid = true;
            return;
        }
    }
}

void Player::UpdateCrouchCollider()
{
    if (!m_standCapsuleValid) return;
    auto& ecs = GameManager::Instance().GetECS();
    auto* pCollider = ecs.TryGetComponent<ColliderData>(m_colliderEntity);
    if (!pCollider) return;
    CollisionShapeCapsule* capsule = nullptr;
    for (auto& shape : pCollider->m_shapes) {
        if ((capsule = dynamic_cast<CollisionShapeCapsule*>(shape.get()))) break;
    }
    if (!capsule) return;

    // 下端はそのままで、上端だけをしゃがみ具合に合わせて下げる(カプセルは 中心±高さ/2 に半径を足した範囲)
    const float r = m_standCapsuleRadius;
    const float bottom = m_standCapsuleOffset.y - m_standCapsuleHeight * 0.5f - r;
    const float standTop = m_standCapsuleOffset.y + m_standCapsuleHeight * 0.5f + r;
    const float crouchTop = (std::max)(bottom + 2.0f * r, (std::min)(m_crouchColliderTop, standTop));
    const float top = standTop + (crouchTop - standTop) * m_crouchBlend;
    capsule->height = (std::max)(0.0f, top - bottom - 2.0f * r);
    capsule->m_offset = m_standCapsuleOffset;
    capsule->m_offset.y = (top + bottom) * 0.5f;
}

bool Player::CanStandUp(const Math::Vector3& playerPos) const
{
    if (!m_standCapsuleValid) return true;
    // 今の頭の少し下から、立った時の頭の高さまで真上へレイを飛ばす(体の中心と前後左右の4点)
    const float r = m_standCapsuleRadius;
    const float standTop = m_standCapsuleOffset.y + m_standCapsuleHeight * 0.5f + r;
    const float fromY = (std::min)(m_crouchColliderTop, standTop) - 0.1f;
    const float length = standTop - fromY + 0.02f;
    if (length <= 0.0f) return true;
    const float s = r * 0.8f;
    const Math::Vector3 offsets[] = { {0, 0, 0}, {s, 0, 0}, {-s, 0, 0}, {0, 0, s}, {0, 0, -s} };
    for (const auto& o : offsets) {
        Math::Vector3 origin = playerPos + o + Math::Vector3(0.0f, fromY, 0.0f);
        RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(origin, Math::Vector3(0, 1, 0), length, "Stage");
        if (hit.hit) return false;
    }
    return true;
}

bool Player::StartsWith(Item item) const
{
    switch (item) {
    case ItemType::Incense:     return m_startWithIncense;
    case ItemType::Amulet:      return m_startWithAmulet;
    case ItemType::FlashLight:  return m_startWithFlashLight;
    }
    return false;
}

void Player::ApplyStartInventory()
{
    m_hasIncense = m_startWithIncense;
    m_hasAmulet = m_startWithAmulet;
    m_hasFlashLight = m_startWithFlashLight;
    m_itemOrder = m_startItemOrder;
    SyncItemOrder(); // 並びに無い・重複している分を補正する
    if (!HasItem(m_currentEquipped) && !m_itemOrder.empty()) m_currentEquipped = m_itemOrder.front();
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

    // 一人称の体と手に持つ物は影を落とさない。懐中電灯の光は手元(レンズ)から出るので、
    // 体や手やライト本体が影を落とすと光をさえぎってしまう
    std::function<void(GameObject*)> disableShadow = [&](GameObject* obj)
    {
        if (!obj)
        {
            return;
        }
        if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID()))
        {
            pModel->m_castShadow = false;
        }
        for (auto& child : obj->GetChildren())
        {
            disableShadow(child.get());
        }
    };
    disableShadow(GetGameObject());
    for (Entity heldEntity : { m_heldIncenseEntity, m_heldAmuletEntity, m_heldFlashLightEntity })
    {
        if (heldEntity != INVALID_ENTITY)
        {
            disableShadow(scene->GetGameObject(heldEntity).get());
        }
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
            // 使えなくなったアイテム(燃え尽きた香)のスロットは赤くする。装備中は少し明るい赤
            const bool unusable = filled && item == Item::Incense && m_incenseBurnedOut;
            if (unusable) {
                sprite->m_color = selected ? Math::Vector4(1.0f, 0.25f, 0.2f, 1.0f) : Math::Vector4(0.85f, 0.15f, 0.12f, 0.85f);
            } else {
                sprite->m_color = selected ? Math::Vector4(1.0f, 0.85f, 0.55f, 1.0f) : Math::Vector4(1.0f, 1.0f, 1.0f, 0.6f);
            }
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
        // 発光の輝度(nit)。光っているレンズを横から見た明るさ
        constexpr float kLensLuminance = 3000.0f;
        const Math::Vector3 glow = IsHoldingLitFlashLight()
            ? PhotometricUnits::MakeLightColor(Math::Vector3(1.0f, 1.0f, 1.0f), 5000.0f) * kLensLuminance
            : Math::Vector3::Zero;
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

void Player::KeepIncenseBurnedPose()
{
    if (!m_incenseBurnedOut || m_heldIncenseEntity == INVALID_ENTITY) return;
    auto& ecs = GameManager::Instance().GetECS();
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(m_heldIncenseEntity);
    if (!pModel || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) return;
    // 燃えている最中(燃焼アニメの再生中)はアニメーションに任せる
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(m_heldIncenseEntity)) {
        if (pAnim->currentAnim.IsPlaying) return;
    }
    const auto& anims = pModel->m_spModelData->GetAnimations();
    if (anims.empty()) return;
    // 燃焼アニメ(0番、香を使った時にPlayAnimationしているもの)の最後のポーズ
    pModel->m_spModelData->UpdateAnimation(0, (std::max)(0.0f, anims[0].duration - 0.001f));
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
    // 小型のLEDライト程度(中心の光度1000cd、5000K)。2m先の壁で250lxになる
    light.m_color = { 1.0f, 1.0f, 1.0f };
    light.m_temperature = 5000.0f;
    light.m_intensity = 1000.0f;
    light.m_sourceRadius = 0.015f; // LEDと反射鏡の大きさ程度
    light.m_range = 15.0f;
    light.m_innerAngle = 12.0f;
    light.m_outerAngle = 22.0f;
    GameManager::Instance().GetECS().AddComponent(beam->GetEntityID(), light);
    m_flashLightBeamEntity = beam->GetEntityID();
}

Math::Vector3 Player::GetEyePosition()
{
    auto& ecs = GameManager::Instance().GetECS();
    if (m_cameraEntity == INVALID_ENTITY || !ecs.TryGetComponent<CameraData>(m_cameraEntity))
    {
        m_cameraEntity = FindGameCameraEntity();
    }
    if (m_cameraEntity != INVALID_ENTITY)
    {
        if (auto* pCamTrans = ecs.TryGetComponent<TransformData>(m_cameraEntity)) return pCamTrans->m_worldMatrix.Translation();
    }
    auto& trans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());
    return trans.m_position + Math::Vector3(0, m_crouchEyeHeight, 0);
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

bool Player::ComputeFlashLightLensWorld(const Math::Vector3& heldPos, const Math::Vector3& heldRotRad, Math::Vector3& outLens) const
{
    // フラッシュライトのモデル(Asset/Model/Item/Flash Light/scene.gltf)のモデル空間でのレンズ("Glass"マテリアル)の中心。
    // 本体はY方向に-1〜10.33、レンズの面はY=10.13にある
    static const Math::Vector3 kLensModelPos(0.0f, 10.13f, 0.0f);
    if (m_heldFlashLightEntity == INVALID_ENTITY || !GetGameObject() || !GetGameObject()->GetScene()) return false;
    auto& ecs = GameManager::Instance().GetECS();
    auto* pParent = ecs.TryGetComponent<TransformData>(m_heldFlashLightEntity);
    auto obj = GetGameObject()->GetScene()->GetGameObject(m_heldFlashLightEntity);
    if (!pParent || !obj) return false;
    TransformData* pModel = nullptr;
    for (auto& child : obj->GetChildren()) {
        if (ecs.TryGetComponent<ModelRenderData>(child->GetEntityID())) { pModel = ecs.TryGetComponent<TransformData>(child->GetEntityID()); break; }
    }
    if (!pModel) return false;
    // ワールド行列は1フレームに1回しか更新されないので、今フレームの手の位置・向きから自前で組む(GetHandWorldと同じ規約)
    auto local = [](const Math::Vector3& s, const Math::Vector3& r, const Math::Vector3& t) {
        return Math::Matrix::CreateScale(s) * Math::Matrix::CreateFromYawPitchRoll(r.y, r.x, r.z) * Math::Matrix::CreateTranslation(t);
    };
    Math::Matrix world = local(pModel->m_scale, pModel->m_rotation, pModel->m_position) * local(pParent->m_scale, heldRotRad, heldPos);
    outLens = Math::Vector3::Transform(kLensModelPos, world);
    return true;
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

    // 光の出どころは手に持ったフラッシュライトのレンズ。
    // 以前は持ち手の位置からカメラの前方へ0.15m出していたため、持ち方によってはレンズより前から光が出て、
    // ライト本体が光の円錐の後ろに入って暗く見えなくなっていた。モデルのレンズ位置から求め、
    // そこからm_flashLightLensOffsetだけ前に置く。
    // 手のボーンに追従した位置を優先し、取れなければ従来の体基準オフセット。
    Math::Vector3 origin, heldRot;
    if (ComputeHeldItemWorld(Item::FlashLight, playerPos, playerYaw, origin, heldRot))
    {
        Math::Vector3 lens;
        if (ComputeFlashLightLensWorld(origin, heldRot, lens)) origin = lens + camForward * m_flashLightLensOffset;
        else origin += camForward * 0.15f;
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
    // UseItem_Idle/FlashLight_Idle the same frame an item is first equipped, but that switch only reaches
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

// クロスヘア(画面中央)の視線。カメラのワールド行列は前フレームのものだが、操作対象を決めるには十分
bool Player::GetCrosshairRay(Math::Vector3& outOrigin, Math::Vector3& outDir)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (m_cameraEntity == INVALID_ENTITY || !ecs.TryGetComponent<CameraData>(m_cameraEntity))
    {
        m_cameraEntity = FindGameCameraEntity();
    }
    auto* pCamTrans = ecs.TryGetComponent<TransformData>(m_cameraEntity);
    if (!pCamTrans) return false;
    const Math::Matrix& camWorld = pCamTrans->m_worldMatrix;
    outOrigin = camWorld.Translation();
    outDir = camWorld.Backward(); // SimpleMathのBackward()が+Z(=このエンジンの前方)
    if (outDir.LengthSquared() < 1e-6f) return false;
    outDir.Normalize();
    return true;
}

// モデルのメッシュを(向き付きの)箱として視線と交差判定し、当たった中で一番近い距離をioNearestに入れる。
// メッシュの頂点はモデル空間なので、ノードのワールド行列は animDeltaTransform * エンティティのワールド行列
// (CollisionSolverと同じ)。開いた扉はアニメーションで動いた位置で当たる。
// onlyNodesを渡すとそのノードのメッシュだけ(扉の板など)を調べる。
static bool RayHitModelBoxes(const ModelRenderData& model, const Math::Matrix& entityWorld, const std::vector<int>* onlyNodes,
                             const Math::Vector3& origin, const Math::Vector3& dir, float maxDist, float& ioNearest)
{
    if (!model.m_spModelData || !model.m_spModelData->IsLoaded()) return false;
    const auto& nodes = model.m_spModelData->GetNodes();
    const float kPad = 0.03f; // 薄い物(お札・扉の縁)も狙いやすいよう少しだけ大きめにする
    bool hit = false;
    auto testNode = [&](int ni) {
        if (ni < 0 || ni >= (int)nodes.size()) return;
        const auto& node = nodes[ni];
        if (node.meshes.empty()) return;
        Math::Matrix nodeWorld = node.animDeltaTransform * entityWorld;
        DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&nodeWorld);
        for (const auto& handle : node.meshes)
        {
            Mesh* mesh = MeshManager::Instance().Get(handle);
            if (!mesh) continue;
            DirectX::BoundingBox aabb = mesh->GetLocalAABB();
            aabb.Extents.x += kPad; aabb.Extents.y += kPad; aabb.Extents.z += kPad;
            DirectX::BoundingOrientedBox obb;
            DirectX::BoundingOrientedBox::CreateFromBoundingBox(obb, aabb);
            obb.Transform(obb, m);
            float t = 0.0f;
            if (obb.Intersects(DirectX::XMLoadFloat3(&origin), DirectX::XMLoadFloat3(&dir), t) && t <= maxDist && t < ioNearest)
            {
                ioNearest = t;
                hit = true;
            }
        }
    };
    if (onlyNodes) { for (int ni : *onlyNodes) testNode(ni); }
    else { for (int ni = 0; ni < (int)nodes.size(); ++ni) testNode(ni); }
    return hit;
}

// GameObjectとその子孫のモデル全部を視線と交差判定する(ピックアップ・照明・スイッチ用)
static bool RayHitGameObject(GameObject* obj, const Math::Vector3& origin, const Math::Vector3& dir, float maxDist, float& ioNearest)
{
    if (!obj) return false;
    auto& ecs = GameManager::Instance().GetECS();
    bool hit = false;
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID());
    auto* pTrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID());
    if (pModel && pTrans && RayHitModelBoxes(*pModel, pTrans->m_worldMatrix, nullptr, origin, dir, maxDist, ioNearest)) hit = true;
    for (const auto& child : obj->GetChildren())
    {
        if (RayHitGameObject(child.get(), origin, dir, maxDist, ioNearest)) hit = true;
    }
    return hit;
}

void Player::UpdateInteractionTarget(const Math::Vector3& playerPos)
{
    // クロスヘアが実際に重なっている物だけを操作対象にする。以前は「近くて、だいたい正面」なら何でも対象にしていたため、
    // 天井越しに2階のドアが開いたり、襖を開けるつもりが横の照明を点けたりしていた。
    // 視線(カメラ中央のレイ)と各対象の形(メッシュの箱)の交差を調べ、一番手前で当たった物を選ぶ。
    // 家の壁・床・天井に先に当たったら、その奥の物は対象外。
    const float kInteractRange = 2.2f; // カメラからの距離

    m_interactKind = InteractKind::None;
    m_interactEntity = INVALID_ENTITY;
    m_doorTargetEntity = INVALID_ENTITY;
    m_doorTargetAnimIdx = -1;
    const bool ghostHunting = IsGhostHunting();

    Math::Vector3 origin, dir;
    if (!GetCrosshairRay(origin, dir)) return;

    if (!m_doorCandidateCacheBuilt) RebuildDoorCandidateCache();

    // 家の壁・床・天井までの距離(扉の板は開け閉めの対象なので除く。閉じた扉の奥の物は、扉の方が手前で当たるので選ばれない)
    float blockDist = kInteractRange;
    {
        RaycastHit wall = CollisionManager::Instance().RaycastAgainstMesh(origin, dir, kInteractRange, "Stage", &m_openableNodeNames);
        if (wall.hit) blockDist = wall.distance;
    }
    const float kSurfaceEpsilon = 0.05f; // 壁に貼り付いたスイッチ・天井に付いた照明が壁や天井に負けないための余裕
    const float maxDist = (std::min)(kInteractRange, blockDist + kSurfaceEpsilon);
    float nearest = FLT_MAX;
    auto& ecs = GameManager::Instance().GetECS();

    // ドア・襖・戸棚など
    {
        PROFILE_CPU_SCOPE("Player::FindDoorOnRay");
        for (const auto& door : m_doorCandidateCache)
        {
            auto* pModel = ecs.TryGetComponent<ModelRenderData>(door.entity);
            auto* pTrans = ecs.TryGetComponent<TransformData>(door.entity);
            if (!pModel || !pTrans) continue;
            if (RayHitModelBoxes(*pModel, pTrans->m_worldMatrix, &door.nodeIndices, origin, dir, maxDist, nearest))
            {
                m_interactKind = InteractKind::Door;
                m_interactEntity = INVALID_ENTITY;
                m_doorTargetEntity = door.entity;
                m_doorTargetAnimIdx = door.animIndex;
            }
        }
    }

    // ピックアップ・照明・スイッチ
    {
        PROFILE_CPU_SCOPE("Player::FindPickupStimulusOnRay");
        for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>())
        {
            GameObject* obj = nullptr;
            InteractKind kind = InteractKind::None;
            if (auto* pickup = dynamic_cast<PickupItem*>(scriptData.Instance.get())) {
                if (pickup->m_collected) continue;
                obj = pickup->GetGameObject();
                kind = InteractKind::Pickup;
            }
            else if (auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get())) {
                if (!src->m_interactable) continue;
                // ハント中は照明(紐スイッチ・ランプ)を操作できない(Fの表示も出さない)
                if (ghostHunting && src->m_type == StimulusType::Light) continue;
                obj = src->GetGameObject();
                kind = InteractKind::Stimulus;
            }
            else if (auto* sw = dynamic_cast<LightSwitch*>(scriptData.Instance.get())) {
                if (!sw->m_interactable) continue;
                // ハント中は壁スイッチを操作できない(レバーも動かず音も鳴らない)
                if (ghostHunting) continue;
                obj = sw->GetGameObject();
                kind = InteractKind::Stimulus;
                // スイッチの正面(ローカル-Z)側にいる時だけ操作できる(壁の裏側からは不可)
                if (obj) {
                    if (auto* pSwTrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID())) {
                        const auto& m = pSwTrans->m_worldMatrix;
                        Math::Vector3 front(-m._31, -m._32, -m._33);
                        front.y = 0.0f;
                        Math::Vector3 toPlayer = playerPos - sw->GetWorldPosition();
                        toPlayer.y = 0.0f;
                        if (front.LengthSquared() > 1e-6f && toPlayer.Dot(front) <= 0.0f) continue;
                    }
                }
            }
            if (!obj) continue;
            if (RayHitGameObject(obj, origin, dir, maxDist, nearest))
            {
                m_interactKind = kind;
                m_interactEntity = obj->GetEntityID();
                m_doorTargetEntity = INVALID_ENTITY;
                m_doorTargetAnimIdx = -1;
            }
        }
    }
}

bool Player::IsGhostHunting() const
{
    for (auto& scriptData : GameManager::Instance().GetECS().GetComponentArray<NativeScriptData>()) {
        if (auto* ghost = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            if (!ghost->IsExorcised() && ghost->GetState() == GhostAI::State::Hunt) return true;
        }
    }
    return false;
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
    m_openableNodeNames.clear();

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);
        if (!pModel || !pTransform) continue;
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty()) continue;
        const auto& nodes = pModel->m_spModelData->GetNodes();

        for (int i = 0; i < (int)anims.size(); ++i)
        {
            const std::string& animName = anims[i].name;
            if (!OpenableAnim::IsOpenable(animName)) continue; // ドア・襖・戸棚・押入れなど
            if (anims[i].channels.empty()) continue;

            // アニメーションが動かすノード(扉の板)と、その子孫のノード。視線との当たり判定に使う
            DoorCandidate cand{ entity, i, {} };
            for (const auto& channel : anims[i].channels)
            {
                for (int ni = 0; ni < (int)nodes.size(); ++ni)
                {
                    if (nodes[ni].name != channel.nodeName) continue;
                    std::vector<int> stack{ ni };
                    while (!stack.empty())
                    {
                        int cur = stack.back(); stack.pop_back();
                        if (std::find(cand.nodeIndices.begin(), cand.nodeIndices.end(), cur) != cand.nodeIndices.end()) continue;
                        cand.nodeIndices.push_back(cur);
                        m_openableNodeNames.insert(nodes[cur].name);
                        for (int c : nodes[cur].children) stack.push_back(c);
                    }
                    break;
                }
            }
            if (!cand.nodeIndices.empty()) m_doorCandidateCache.push_back(std::move(cand));
        }
    }

    m_doorCandidateCacheBuilt = true;
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
    // ライトは他のアイテムと持ち方が違うので専用のFlashLight_*、それ以外のアイテムはUseItem_*
    if (IsHoldingVisibleItem())
        clipName = std::string(m_currentEquipped == Item::FlashLight ? "FlashLight_" : "UseItem_") + clip;

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

void Player::TogglePresentationMode(GhostAI* ghost)
{
    m_presentationMode = !m_presentationMode;
    if (m_presentationMode) {
        if (ghost) {
            m_ghostTypeBeforePresentation = static_cast<int>(ghost->GetGhostType());
            ghost->SetGhostType(GhostAI::GhostType::Kageonna);
        }
        // 香を持っていなければ渡す。燃え尽きていても使えるように戻す
        if (!HasItem(Item::Incense)) AddItem(Item::Incense);
        m_currentEquipped = Item::Incense;
        m_incenseBurnedOut = false;
    } else {
        if (ghost && m_ghostTypeBeforePresentation >= 0) {
            ghost->SetGhostType(static_cast<GhostAI::GhostType>(m_ghostTypeBeforePresentation));
        }
        m_ghostTypeBeforePresentation = -1;
    }
    if (auto gs = GameSequence::GetInstance()) gs->SetPlayerInvincible(m_presentationMode);
    Logger::Instance().SetPinnedLine("Presentation Mode", m_presentationMode ? "ON (Kageonna / unlimited incense / invincible)" : "OFF");
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Presentation mode: %s", m_presentationMode ? "ON" : "OFF");
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
    // 手元の高さはしゃがみ具合に合わせて下げる(押し入れの中でしゃがんで投げた時、中段の棚の上や棚の中から出ないように)。
    const float kStandHandHeight = 1.1f;
    const float kCrouchHandHeight = (std::max)(0.3f, m_crouchEyeHeight - 0.35f);
    const float kHandForward = 0.35f;
    const float kWallClearance = 0.15f;
    const float kThrowSpeed = 2.5f;   // 視線方向の初速(m/s)
    const float kThrowUpSpeed = 1.2f; // 少し山なりにするための上向きの初速(m/s)
    auto& cm = CollisionManager::Instance();
    float handHeight = kStandHandHeight + (kCrouchHandHeight - kStandHandHeight) * m_crouchBlend;
    // 足元から手元までの間に棚や天井があれば、その下から出す
    {
        const float kFromY = 0.2f;
        if (handHeight > kFromY) {
            RaycastHit overhead = cm.RaycastAgainstMesh(playerPos + Math::Vector3(0, kFromY, 0), Math::Vector3(0, 1, 0),
                handHeight - kFromY + kWallClearance, "Stage");
            if (overhead.hit) handHeight = (std::max)(kFromY, kFromY + overhead.distance - kWallClearance);
        }
    }
    Math::Vector3 handBase = playerPos + Math::Vector3(0, handHeight, 0);
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

    // 捕まった後(ゲームオーバー中)は操作を受け付けない。視点はGameScene側でゴーストに固定される。
    // 手に持ったアイテムとライトはカメラの向きに合わせて動かし続ける
    if (auto gs = GameSequence::GetInstance(); gs && gs->IsPlayerCaught()) {
        UpdateHeldItemAttach(cTrans.m_position, cTrans.m_rotation.y, false);
        UpdateFlashLightBeam(cTrans.m_position, cTrans.m_rotation.y);
        AudioManager::Instance().SetListener(cTrans.m_position + Math::Vector3(0, 1.6f, 0), forward);
        UpdateSounds(false, GameTimer::Instance().DeltaTime());
        return;
    }

    // 0: 発表用デバッグモードの切り替え
    if (input.IsKeyTrigger('0')) {
        GhostAI* ghostForDebug = nullptr;
        for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
            if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) { ghostForDebug = g; break; }
        }
        TogglePresentationMode(ghostForDebug);
    }

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
        if (m_currentEquipped == Item::Incense && m_incenseBurnedOut && !m_presentationMode) {
            Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense: already burned out.");
        }
        else if (m_currentEquipped == Item::Incense && ghost) {
            auto& gTrans = ecs.GetComponent<TransformData>(ghost->GetGameObject()->GetEntityID());
            float dist = (cTrans.m_position - gTrans.m_position).Length();
            if (dist < 10.0f) {
                // 燃やした時点で使い切り(効いたかどうかに関係なく)。発表用デバッグモード中は何度でも使える
                if (!m_presentationMode) m_incenseBurnedOut = true;
                // 効くかどうかはゴーストの種類ごとの条件次第(影女は近くで明かりが点いている時だけ等)
                if (ghost->TryIncenseStun(5.0f)) {
                    Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense Used: Ghost Stunned!");
                } else {
                    Logger::Instance().AddLog(Logger::LogLevel::Info, "Incense Used: no effect on this ghost right now.");
                }
                if (m_heldIncenseEntity != INVALID_ENTITY) {
                    AnimationManager::Instance().PlayAnimation(m_heldIncenseEntity, 0, false, 1.0f);
                }
                // 燻る音
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

    // --- Animation state --- (Idle/Walk/Crouch/CrouchWalk x FlashLight_*/UseItem_* picked in UpdateAnimationState())
    // Ctrl: toggles crouch on/off.
    bool crouchToggled = false;
    if (input.IsKeyTrigger(VK_CONTROL))
    {
        if (m_isCrouching && !CanStandUp(cTrans.m_position)) {
            // 頭の上がつかえている(押し入れの中など)。しゃがんだまま
            Logger::Instance().AddLog(Logger::LogLevel::Info, "[Player] Cannot stand up here.");
        } else {
            m_isCrouching = !m_isCrouching;
            crouchToggled = true;
        }
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
    UpdateCrouchCollider();

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
        UpdateInteractionTarget(cTrans.m_position);
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
            // クロスヘアで捉えている扉があれば開け閉めする(無ければ何もしない)
            TryInteractDoor();
        }
        m_interactCooldown = 0.5f; // 0.5s cooldown to avoid repeat-triggering on a held key
    }

    UpdateItemIcons();
    UpdateHeldItem();
    KeepIncenseBurnedPose();
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

void Player::TryInteractDoor()
{
    auto& ecs = GameManager::Instance().GetECS();
    auto& animMgr = AnimationManager::Instance();

    // クロスヘアで捉えている物(UpdateInteractionTargetが選んだもの)だけを開け閉めする
    Entity bestEntity = m_doorTargetEntity;
    int    bestAnimIdx = m_doorTargetAnimIdx;
    if (bestEntity != INVALID_ENTITY)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(bestEntity);
        if (!pModel || !pModel->m_spModelData || bestAnimIdx < 0 || bestAnimIdx >= (int)pModel->m_spModelData->GetAnimations().size())
        {
            bestEntity = INVALID_ENTITY;
            bestAnimIdx = -1;
        }
        else if (!ecs.TryGetComponent<AnimationDataComponent>(bestEntity))
        {
            ecs.AddComponent<AnimationDataComponent>(bestEntity, AnimationDataComponent{});
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

        // 押入れの中から襖を操作した時: クロスヘアが閉まっている隣の襖を捉えていても、同じ押入れに開いている襖が
        // あればそちらを閉める(入ってきた襖を閉めたつもりで隣を開けてしまい、隠れられないのを防ぐ)
        if (auto* pTrans = ecs.TryGetComponent<TransformData>(GetGameObject()->GetEntityID()))
        {
            auto isOpen = [&](int idx) {
                auto found = pAnim->multiAnims.find(idx);
                return found != pAnim->multiAnims.end() && (found->second.IsPlaying || found->second.ProgressTime > 0.0f);
            };
            for (const auto& spot : HidingSpot::GetSpots())
            {
                if (spot.entity != bestEntity || !HidingSpot::IsInside(spot, pTrans->m_position)) continue;
                if (std::find(spot.fusumaAnims.begin(), spot.fusumaAnims.end(), bestAnimIdx) == spot.fusumaAnims.end()) break;
                if (isOpen(bestAnimIdx)) break;
                for (int idx : spot.fusumaAnims)
                {
                    if (isOpen(idx)) { bestAnimIdx = idx; break; }
                }
                break;
            }
        }

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
            // ドアは開け閉めどちらも同じ軋み音(閉める時は少し低めにして聞き分けられるようにする)。襖・押入れは専用の音
            {
                const std::string& animName = ecs.GetComponent<ModelRenderData>(bestEntity).m_spModelData->GetAnimations()[bestAnimIdx].name;
                if (OpenableAnim::IsSliding(animName)) {
                    AudioManager::Instance().PlaySE(opening ? OpenableAnim::OpenSound(animName) : OpenableAnim::CloseSound(animName), m_doorSoundVolume);
                } else {
                    AudioManager::Instance().PlaySE(OpenableAnim::OpenSound(animName), m_doorSoundVolume, opening ? 0.0f : -0.15f);
                }
            }

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
    out["crouchColliderTop"] = m_crouchColliderTop;
    out["flashLightLensOffset"] = m_flashLightLensOffset;
    out["crouchEyeHeight"] = m_crouchEyeHeight;
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
    // 初期所持品だけを保存する(プレイ中の所持状態は保存しない。キー名は以前と同じ)
    out["hasIncense"] = m_startWithIncense;
    out["hasAmulet"] = m_startWithAmulet;
    out["hasFlashLight"] = m_startWithFlashLight;
    // 開始時のアイテムスロットの並び。ItemTypeの値の配列で保存する
    {
        std::vector<Item> order = m_startItemOrder;
        order.erase(std::remove_if(order.begin(), order.end(), [&](Item item) { return !StartsWith(item); }), order.end());
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
    if (in.contains("crouchColliderTop"))   m_crouchColliderTop = in["crouchColliderTop"];
    if (in.contains("flashLightLensOffset")) m_flashLightLensOffset = in["flashLightLensOffset"];
    if (in.contains("crouchEyeHeight"))     m_crouchEyeHeight = in["crouchEyeHeight"];
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
    // シーンの所持フラグ・並びは初期所持品として読む
    if (in.contains("hasIncense"))          m_startWithIncense = in["hasIncense"];
    if (in.contains("hasAmulet"))           m_startWithAmulet = in["hasAmulet"];
    if (in.contains("hasFlashLight"))       m_startWithFlashLight = in["hasFlashLight"];
    if (in.contains("itemOrder") && in["itemOrder"].is_array()) {
        m_startItemOrder.clear();
        for (const auto& v : in["itemOrder"]) {
            if (!v.is_number_integer()) continue;
            int n = v.get<int>();
            if (n >= static_cast<int>(Item::Incense) && n <= static_cast<int>(Item::FlashLight)) {
                m_startItemOrder.push_back(static_cast<Item>(n));
            }
        }
    }
    ApplyStartInventory();
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
    ImGui::DragFloat(u8"移動速度 (km/h)", &m_moveSpeedKmh, 0.1f, 0.1f, 100.0f, "%.1f km/h");
    ImGui::Checkbox(u8"重力を使う", &m_useGravity);
    if (m_useGravity)
    {
        ImGui::DragFloat(u8"重力の強さ", &m_gravityStrength, 0.1f, 0.0f, 100.0f);
    }

    ImGui::Separator();
    ImGui::Text(u8"[手に持つアイテム] アイテムごとの右手 (Attach_RightHand) からの位置と向き (毎フレーム手に追従)");
    {
        static const char* kNames[5] = { "", "", u8"お香", u8"お札", u8"懐中電灯" };
        for (int i = (int)Item::Incense; i <= (int)Item::FlashLight; ++i)
        {
            ImGui::PushID(i);
            ImGui::Text("%s", kNames[i]);
            ImGui::DragFloat3(u8"手からの位置 (m)", &m_heldPose[i].pos.x, 0.005f, -1.0f, 1.0f);
            ImGui::DragFloat3(u8"手からの回転 (度)", &m_heldPose[i].rotDeg.x, 1.0f, -180.0f, 180.0f);
            ImGui::PopID();
        }
    }
    ImGui::Separator();
    ImGui::Text(u8"[手に持つアイテム] 手のボーンが無い時: 体からの位置 (右/上/前, m)");
    ImGui::DragFloat3(u8"アイテムの位置", &m_heldItemOffset.x, 0.01f, -2.0f, 2.0f);
    ImGui::Text(u8"[手に持つアイテム] Attach_RightHand から調整済み: %s", m_heldItemOffsetCalibrated ? u8"はい" : u8"いいえ");
    if (ImGui::Button(u8"取り付け位置から調整し直す")) {
        auto& ecsCalib = GameManager::Instance().GetECS();
        auto& cTransCalib = ecsCalib.GetComponent<TransformData>(GetGameObject()->GetEntityID());
        m_heldItemOffsetCalibrated = false;
        CalibrateHeldItemOffset(cTransCalib.m_position, cTransCalib.m_rotation.y);
    }
    ImGui::Text(u8"[手に持つアイテム] モデルに追加する回転 (度)");
    ImGui::DragFloat3(u8"アイテムの追加回転", &m_heldItemExtraRotationDeg.x, 1.0f, -180.0f, 180.0f);

    ImGui::Separator();
    ImGui::Text(u8"[アニメ] 状態: Idle/Walk/Crouch/CrouchWalk。持っている間は自動で FlashLight_* (懐中電灯) / UseItem_* (他のアイテム) を付ける");
    ImGui::DragFloat(u8"歩きアニメの速さ", &m_animWalkSpeed, 0.05f, 0.1f, 3.0f);
    ImGui::DragFloat(u8"しゃがみ時のカメラ移動の速さ", &m_crouchBlendSpeed, 0.1f, 0.5f, 20.0f);
    ImGui::DragFloat(u8"しゃがみ時のカメラの遅れ", &m_crouchCameraDelay, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat(u8"懐中電灯のレンズ位置 (m)", &m_flashLightLensOffset, 0.005f, -0.2f, 0.3f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"懐中電灯の光が出る位置。レンズからこの距離だけ前(マイナスで後ろ)");
    ImGui::DragFloat(u8"しゃがみ時の当たり判定の高さ (m)", &m_crouchColliderTop, 0.01f, 0.3f, 2.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"しゃがんだ時の当たり判定の上端(足元から)。押し入れの中段の下より低くすると入れる");
    ImGui::DragFloat(u8"しゃがみ時の目の高さ (m)", &m_crouchEyeHeight, 0.01f, 0.2f, 2.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"しゃがんだ時の目線の高さ(足元から)。当たり判定の上端より少し低くする");
    ImGui::Text(u8"[デバッグ] しゃがみ中: %s (ブレンド=%.2f)", m_isCrouching ? u8"はい" : u8"いいえ", m_crouchBlend);

    ImGui::Separator();
    ImGui::Text(u8"[音] 音量 (0-1)");
    ImGui::SliderFloat(u8"足音 (木の床)", &m_footstepVolumes[0], 0.0f, 1.0f);
    ImGui::SliderFloat(u8"足音 (畳)", &m_footstepVolumes[1], 0.0f, 1.0f);
    ImGui::SliderFloat(u8"足音 (土)", &m_footstepVolumes[2], 0.0f, 1.0f);
    ImGui::SliderFloat(u8"しゃがみ時の足音の倍率", &m_crouchFootstepScale, 0.0f, 1.0f);
    {
        static const char* kFloorNames[3] = { u8"木の床", u8"畳", u8"土" };
        ImGui::Text(u8"[デバッグ] 床: %s", kFloorNames[static_cast<int>(m_currentFloor)]);
    }
    ImGui::SliderFloat(u8"懐中電灯のスイッチ", &m_flashLightSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat(u8"ドアの開け閉め", &m_doorSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat(u8"ドアの施錠", &m_doorLockedSoundVolume, 0.0f, 1.0f);
    ImGui::SliderFloat(u8"お香", &m_smudgeSoundVolume, 0.0f, 1.0f);
    ImGui::DragFloat(u8"お香の音の長さ (秒)", &m_smudgeSoundDuration, 0.1f, 1.0f, 40.0f);

    ImGui::Separator();
    ImGui::Text(u8"[初期所持品] シーンに保存される。プレイ中に拾ったり落としたりしても変わらない");
    {
        auto startToggle = [&](const char* label, bool& flag, Item item) {
            if (ImGui::Checkbox(label, &flag)) {
                m_startItemOrder.erase(std::remove(m_startItemOrder.begin(), m_startItemOrder.end(), item), m_startItemOrder.end());
                if (flag) m_startItemOrder.push_back(item); // 付けたものはスロットの末尾へ
            }
        };
        startToggle(u8"最初からお香を持つ", m_startWithIncense, Item::Incense);
        startToggle(u8"最初からお札を持つ", m_startWithAmulet, Item::Amulet);
        startToggle(u8"最初から懐中電灯を持つ", m_startWithFlashLight, Item::FlashLight);
        if (ImGui::Button(u8"初期の持ち物を今すぐ反映")) ApplyStartInventory();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"今のプレイ中の所持状態を初期所持品に戻す");
    }

    ImGui::Separator();
    const char* itemName = u8"お香";
    if (m_currentEquipped == Item::Amulet) itemName = u8"お札";
    else if (m_currentEquipped == Item::FlashLight) itemName = u8"懐中電灯";
    ImGui::Text(u8"[デバッグ] 装備中: %s", itemName);
    ImGui::Text(u8"[デバッグ] 所持 (実行中): お香=%s お札=%s 懐中電灯=%s",
        m_hasIncense ? u8"はい" : u8"いいえ",
        m_hasAmulet ? u8"はい" : u8"いいえ",
        m_hasFlashLight ? u8"はい" : u8"いいえ");
    ImGui::Text(u8"[デバッグ] 懐中電灯: %s (T / 右クリックで切り替え)", m_flashLightOn ? u8"ON" : u8"OFF");
    ImGui::Text(u8"[デバッグ] お香: %s", m_incenseBurnedOut ? u8"燃え尽きた" : u8"未使用");
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());
    ImGui::Text(u8"[デバッグ] 位置: (%.1f, %.1f, %.1f)", cTrans.m_position.x, cTrans.m_position.y, cTrans.m_position.z);
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            if (auto* room = g->GetTargetRoom()) {
                ImGui::Text(u8"[デバッグ] 部屋の最小: (%.1f,%.1f,%.1f)", room->m_min.x, room->m_min.y, room->m_min.z);
                ImGui::Text(u8"[デバッグ] 部屋の最大: (%.1f,%.1f,%.1f)", room->m_max.x, room->m_max.y, room->m_max.z);
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), u8"[デバッグ] 対象の部屋がありません (TargetRoom = nullptr)");
            }
            break;
        }
    }
}

void Player::OnCollisionEnter(GameObject* other) {}
void Player::OnCollisionStay(GameObject* other) {}