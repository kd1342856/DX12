#pragma once
#include "../Item/ItemTypes.h"

class Player : public NativeScript {
public:
    using Item = ItemType;

    void Awake() override;
    void Start() override;
    void Update(float deltaTime) override;
    void PostUpdate() override;
    void PreDraw() override;
    void Draw() override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    void OnCollisionEnter(GameObject* other) override;
    void OnCollisionStay(GameObject* other) override;

    // Called by PickupItem. Sets the matching possession flag and auto-equips if not yet equipped.
    void AddItem(ItemType item);
    bool HasItem(ItemType item) const;

    //���Ⴊ�݋(0=����,1=���S�ɂ��Ⴊ��)�BCrouch�A�j���[�V�����̎��ۂ̍Đ��ʒu�Ɠ������Ă���̂ŁA
    // �J�����̍���������ɍ��킹��ƃA�j���[�V�����ƃY���Ȃ��B
    float GetCrouchAmount() const { return m_crouchBlend; }
    bool IsCrouching() const { return m_isCrouching; }

private:
    // F�L�[�Ńh�A���J���鏈��
    void TryInteractDoor(const Math::Vector3& playerPos);
    float m_moveSpeed = 10.0f;
    bool m_useGravity = true;
    float m_gravityStrength = 9.8f;
    float m_velocityY = 0.0f;
    bool m_isGrounded = false;

    Item m_currentEquipped = Item::Thermometer;
    float m_temperature = 20.0f;

    // --- Item possession state ---
    // Whether each item has been picked up yet. Cannot equip an item that isn't owned (guarded in Update()).
    bool m_hasThermometer = false;
    bool m_hasIncense = false;
    bool m_hasAmulet = false;

    // The 3 HUD icon Sprite entities (expected to exist in GameScene.json named
    // "ThermometerIcon"/"SmudgeStickIcon"/"OfudaIcon"). Looked up by name and cached in Start();
    // stays INVALID_ENTITY if not found.
    Entity m_thermometerIconEntity = INVALID_ENTITY;
    Entity m_incenseIconEntity = INVALID_ENTITY;
    Entity m_amuletIconEntity = INVALID_ENTITY;
    void FindItemIconEntities();
    void UpdateItemIcons();

    // The 3 held-item model entities (children of Player named "HeldThermometer"/"HeldSmudgeStick"/
    // "HeldOfuda" in GameScene.json). Only the currently equipped+owned one is made visible.
    Entity m_heldThermometerEntity = INVALID_ENTITY;
    Entity m_heldIncenseEntity = INVALID_ENTITY;
    Entity m_heldAmuletEntity = INVALID_ENTITY;
    void UpdateHeldItem();
    // Scrubs the held thermometer's baked "mercury drop" animation to match m_temperature
    // (no auto-playback - we just set ProgressTime directly each frame).
    void UpdateThermometerAnimation();

    // Held items are top-level entities (not children of Player) so their Position/Rotation can be
    // driven directly here each frame: a fixed local offset from the player's own body
    // (m_heldItemOffset, tunable live from ImGuiUpdate()), rotated by yaw only. Deliberately NOT
    // tracking the animated "Attach_RightHand" hand bone every frame - that surfaced a different
    // visible problem each time (wobble, turning lag, a mode-switch teleport, drift) because the
    // hand swings a lot at the end of a long bone chain. Instead, m_heldItemOffset is auto-calibrated
    // ONCE from the real Attach_RightHand bone's idle rest position (see CalibrateHeldItemOffset), so
    // it starts out actually at the hand, then stays perfectly fixed there for the rest of the session.
    Math::Vector3 m_heldItemOffset = { 0.35f, 1.0f, 0.3f };
    bool m_heldItemOffsetCalibrated = false;
    // Extra local rotation (degrees, X/Y/Z) applied on top of the player's yaw, to correct the held
    // model's own base orientation (e.g. the thermometer's authored "up" not matching how it should
    // sit in-hand). Tunable live from ImGuiUpdate().
    Math::Vector3 m_heldItemExtraRotationDeg = { 0.0f, 90.0f, 0.0f };
    // Waits for the player to stand still holding the same item for a bit before calibrating, so
    // the UseItem_Idle "presenting" pose has actually taken effect (see UpdateHeldItemAttach).
    Entity m_calibWaitEntity = INVALID_ENTITY;
    float m_calibWaitTimer = 0.0f;
    void UpdateHeldItemAttach(const Math::Vector3& playerPos, float playerYaw, bool isMoving);
    // One-shot: reads Attach_RightHand's current world position and converts it into a body-local
    // offset (undoing the player's current position/yaw), storing it into m_heldItemOffset. Only
    // runs while the player is standing still, so it captures the neutral idle hand pose rather than
    // a mid-swing extreme. No-ops (and can be retried next frame) if the model/node isn't ready yet.
    bool CalibrateHeldItemOffset(const Math::Vector3& playerPos, float playerYaw);

    // --- F-key interaction (doors + pickups) ---
    enum class InteractKind { None, Door, Pickup };
    InteractKind m_interactKind = InteractKind::None;
    // Only meaningful when m_interactKind == Pickup: the targeted PickupItem's entity.
    Entity m_interactEntity = INVALID_ENTITY;
    // "Press F" HUD prompt Sprite entity (named "InteractPrompt" in GameScene.json).
    Entity m_interactPromptEntity = INVALID_ENTITY;
    void UpdateInteractionTarget(const Math::Vector3& playerPos, const Math::Vector3& forward);
    void UpdateInteractPrompt();
    // Lightweight, non-logging re-scan of TryInteractDoor's door search, plus a facing-cone check,
    // used only to decide whether a door is in view (the actual open/close scan+action still happens
    // in TryInteractDoor() on F press).
    bool FindNearestDoor(const Math::Vector3& playerPos, const Math::Vector3& forward, float maxRange, float& outDist) const;
    Entity FindNearestPickup(const Math::Vector3& playerPos, const Math::Vector3& forward, float maxRange, float& outDist) const;

    // FindNearestDoorは以前、「近くにドアがあるか」に答えるためだけに、毎フレーム全
    // エンティティの全アニメーションを再スキャンし(ドアだった場合はさらにモデルの
    // ノードリスト全体を辿ってDecomposeまでして)いた - Debugで計測すると約5ms。
    // ドアの蝶番は開閉時に平行移動せず(回転するだけ)、そのワールド位置は不変なので
    // 一度計算すれば十分 - 遅延キャッシュしておき、毎フレームは距離+向きの安い判定
    // だけを行うようにする。
    struct DoorCandidate { Entity entity; int animIndex; Math::Vector3 worldPos; };
    mutable std::vector<DoorCandidate> m_doorCandidateCache;
    mutable bool m_doorCandidateCacheBuilt = false;
    void RebuildDoorCandidateCache() const;

    // --- �A�j���[�V���� ---
    // AnimationDataComponent�͎������g�ł͂Ȃ��q��"Model"�I�u�W�F�N�g�ɕt���Ă��邽�߁A
    // �����T���Ċo���Ă���(���t���[���T�����Ȃ��Ă����悤��Start()�ŃL���b�V������)
    Entity m_animEntity = INVALID_ENTITY;

    bool m_isCrouching = false;
    float m_crouchBlend = 0.0f; // 0=����,1=���S�ɂ��Ⴊ�݁B���t���[���ڕW�l�֊��炩�ɋ߂Â���
    float m_crouchBlendSpeed = 6.0f; // �傫���قǑf�����Ǐ]����
    // Ctrl�������Ă���J�����������o���܂ł̒x��(�b)�B
    // �A�j���[�V�������̓����o���̃��O�ɍ��킹�Ē�������B
    float m_crouchCameraDelay = 0.1f;
    float m_crouchCameraDelayTimer = 0.0f;

    // Walk/CrouchWalk�n�̃A�j���[�V�����Đ����x�{���B1.0�����{�B
    float m_animWalkSpeed = 1.0f;

    // Animation state machine: picks one of Idle/Walk/Crouch/CrouchWalk (or its UseItem_* variant
    // when a possessed item is currently equipped/displayed) and plays it by name via
    // AnimationManager::PlayAnimationByName - it only actually restarts the clip when the resolved
    // name changes, so calling this every frame is cheap.
    void UpdateAnimationState(bool isMoving);
    // True while the equipped item is owned and its held-in-hand model is being shown
    // (same condition UpdateHeldItem() uses to decide visibility).
    bool IsHoldingVisibleItem() const;
};

