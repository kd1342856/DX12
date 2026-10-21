#pragma once
#include <unordered_set>
#include "../Item/ItemTypes.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"
#include "../System/RoomArea.h"

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
    void SetFlashLightOn(bool on) { m_flashLightOn = on; }
    bool IsFlashLightOn() const { return m_flashLightOn; }
    // Ghost interference: makes the beam flicker for a while (stays on - only the look flickers).
    void FlickerFlashLight(float duration) { m_flashLightFlicker.Start(duration); }
    bool IsFlashLightFlickering() const { return m_flashLightFlicker.IsFlickering(); }
    // 点灯したフラッシュライトを手に持っている(装備中)か。ハント中のGhostはこれを目印に向かってくる
    bool IsHoldingLitFlashLight() const { return m_flashLightOn && m_hasFlashLight && m_currentEquipped == Item::FlashLight; }
    // G key: lightly tosses the equipped item from hand height along the camera's view, as a
    // PickupItem (spawned from Asset/Data/Prefab/<item>.json, see PickupItem::Launch), and equips
    // the next owned item, if any.
    void DropEquippedItem(const Math::Vector3& playerPos, const Math::Vector3& forward, float playerYaw);

    //しゃがみ具合(0=立ち,1=完全にしゃがみ)。Crouchアニメーションの実際の再生位置と同期しているので、
    // カメラの高さもこれに合わせるとアニメーションとズレない。
    float GetCrouchAmount() const { return m_crouchBlend; }
    bool IsCrouching() const { return m_isCrouching; }
    // 完全にしゃがんだ時の目線の高さ(足元から, m)。GameSceneがカメラを下げる量の計算に使う
    float GetCrouchEyeHeight() const { return m_crouchEyeHeight; }
    // 目の位置(ゲームカメラのワールド座標)。押入れで見つかった時にゴーストが顔を近づける先に使う
    Math::Vector3 GetEyePosition();

private:
    // Fキーでドアを開閉する処理
    void TryInteractDoor();

    // --- Sound ---
    // 足音は歩いている間ループ再生し、止まったら停止する。床の種類(FloorType)ごとに音を切り替える:
    // いる部屋(RoomArea)の床の種類、どの部屋にも入っていなければ家の外(土)とみなす
    std::unique_ptr<DirectX::SoundEffectInstance> m_footstepSounds[3]; // FloorTypeの順(木/畳/土)
    float m_footstepVolumes[3] = { 0.6f, 0.6f, 0.6f };
    float m_crouchFootstepScale = 0.4f;     // しゃがみ歩きの足音の音量倍率
    FloorType m_currentFloor = FloorType::Wood;
    FloorType DetectFloorType(const Math::Vector3& pos) const;
    float m_flashLightSoundVolume = 1.0f;
    float m_doorSoundVolume = 0.9f;
    float m_doorLockedSoundVolume = 1.0f;
    float m_smudgeSoundVolume = 1.0f;
    // お香(smadge.wav)は長い燻る音なので、使ってから一定時間だけ鳴らしてフェードアウトする
    std::unique_ptr<DirectX::SoundEffectInstance> m_smudgeSound;
    float m_smudgeSoundTimer = 0.0f;
    float m_smudgeSoundDuration = 6.0f;
    // 香(スマッジスティック)は使い捨て: 一度燃やしたら燃え尽きて、以後は右クリックしても使えない。
    // 香は1本だけなので、落として拾い直しても燃え尽きたまま(Playerが持つ)。
    bool m_incenseBurnedOut = false;

    // 発表用デバッグモード(0キーで切り替え): ゴーストを影女にし、香を何度でも使えるようにして、無敵にする
    bool m_presentationMode = false;
    int  m_ghostTypeBeforePresentation = -1; // OFFにした時に戻す種類(GhostType)
    void TogglePresentationMode(class GhostAI* ghost);
    void SetFlashLightOnWithSound(bool on);
    void UpdateSounds(bool isMoving, float deltaTime);
    float m_moveSpeedKmh = 10.8f; // 移動速度(km/h)。移動計算では/3.6してm/sにする
    bool m_useGravity = true;
    float m_gravityStrength = 9.8f;
    float m_velocityY = 0.0f;
    bool m_isGrounded = false;

    Item m_currentEquipped = Item::Incense;

    // --- Item possession state (runtime) ---
    // Whether each item is currently held. Cannot equip an item that isn't owned (guarded in Update()).
    // Not saved: it starts from the start inventory below every time the scene starts.
    bool m_hasIncense = false;
    bool m_hasAmulet = false;
    bool m_hasFlashLight = false;

    // --- 初期所持品(シーンに保存する設定) ---
    // プレイ中の所持状態(m_has*/m_itemOrder)とは分けて持ち、Start()でプレイ中の状態へコピーする。
    // シーンに保存するのはこちらだけなので、プレイ中に拾ったり落としたりした後に保存しても初期所持品は変わらない。
    // (以前は所持状態をそのまま保存していたため、香を落とした状態で保存すると香が消えていた)
    bool m_startWithIncense = false;
    bool m_startWithAmulet = false;
    bool m_startWithFlashLight = false;
    std::vector<Item> m_startItemOrder; // 開始時のスロットの並び
    void ApplyStartInventory();
    bool StartsWith(Item item) const;

    // --- FlashLight ---
    // Toggled with T at any time while owned, or right-click while it's the equipped item. The beam
    // is a runtime-only (dynamic, never saved) entity holding a SpotLightData, placed at the game
    // camera each frame and aimed along its view direction (so it follows pitch as well as yaw) -
    // it keeps shining even while another item is equipped, like a light clipped to the chest.
    bool m_flashLightOn = false;
    LightFlickerState m_flashLightFlicker;
    Entity m_flashLightBeamEntity = INVALID_ENTITY;
    Entity m_cameraEntity = INVALID_ENTITY;
    void CreateFlashLightBeam();
    void UpdateFlashLightBeam(const Math::Vector3& playerPos, float playerYaw);
    // 手に持ったフラッシュライトのレンズのワールド位置(heldPos/heldRotRadは今フレームの持ち手の位置・向き)
    bool ComputeFlashLightLensWorld(const Math::Vector3& heldPos, const Math::Vector3& heldRotRad, Math::Vector3& outLens) const;
    float m_flashLightLensOffset = 0.02f; // 光を出す位置: レンズからこの距離だけ前(m)
    Entity FindGameCameraEntity() const;

    // HUD item slot bar (bottom center). Icon Sprite entities are expected in GameScene.json named
    // "SmudgeStickIcon"/"OfudaIcon"/"FlashLightIcon", and their slot frames "ItemSlot1".."ItemSlot3"
    // (same order as the 1-3 equip keys). Looked up by name and cached in Start(); stays
    // INVALID_ENTITY if not found.
    static constexpr int kItemSlotCount = 3;
    Entity m_incenseIconEntity = INVALID_ENTITY;
    Entity m_amuletIconEntity = INVALID_ENTITY;
    Entity m_flashLightIconEntity = INVALID_ENTITY;
    Entity m_itemSlotEntities[kItemSlotCount] = { INVALID_ENTITY, INVALID_ENTITY, INVALID_ENTITY };

    // 所持アイテムを拾った順に並べたもの。スロットN(とキーN)にはN番目に拾ったアイテムが入る。
    // AddItemで末尾に追加し、捨てたら詰める。シーンから読んだ初期所持品はSyncItemOrder()が既定順で補う。
    std::vector<Item> m_itemOrder;
    void SyncItemOrder();
    Entity GetItemIconEntity(Item item) const;
    void FindItemIconEntities();
    void UpdateItemIcons();

    // The held-item model entities (named "HeldSmudgeStick"/"HeldOfuda" in GameScene.json).
    // Only the currently equipped+owned one is made visible.
    Entity m_heldIncenseEntity = INVALID_ENTITY;
    Entity m_heldAmuletEntity = INVALID_ENTITY;
    // "HeldFlashLight": unlike the others, its model sits on a child "Model" object (to apply
    // the model's own scale/orientation fix), so visibility is applied to the whole subtree.
    Entity m_heldFlashLightEntity = INVALID_ENTITY;
    void UpdateHeldItem();
    // 燃え尽きた香の見た目を保つ。ポーズは同じモデルを使う全員で共有され(ModelData)、再生中にしか更新されない上、
    // 香を捨てた時にprefabの読み込みでResetAnimation()されて燃える前に戻ってしまうため、毎フレーム燃焼アニメの
    // 最後のポーズを書き直す。共有なので手持ちの香にも床に落ちた香にも効く。
    void KeepIncenseBurnedPose();

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
    // model's own base orientation (e.g. a model's authored "up" not matching how it should sit
    // in-hand). Tunable live from ImGuiUpdate().
    Math::Vector3 m_heldItemExtraRotationDeg = { 0.0f, 90.0f, 0.0f };
    // Waits for the player to stand still holding the same item for a bit before calibrating, so
    // the UseItem_Idle "presenting" pose has actually taken effect (see UpdateHeldItemAttach).
    Entity m_calibWaitEntity = INVALID_ENTITY;
    float m_calibWaitTimer = 0.0f;
    void UpdateHeldItemAttach(const Math::Vector3& playerPos, float playerYaw, bool isMoving);

    // --- 手のボーン追従(Attach_RightHand) ---
    // 持ち物は毎フレーム、手のボーンのワールド位置・回転に追従する。アイテムごとに「手から見た
    // ローカルのオフセット(m)と回転(度)」を持ち、ImGuiUpdate()で調整できる(添字は(int)ItemType、2〜4)。
    // ボーンが無いモデルでは従来の体基準の固定オフセット(m_heldItemOffset)にフォールバックする。
    struct HeldPose
    {
        Math::Vector3 pos = Math::Vector3::Zero;
        Math::Vector3 rotDeg = Math::Vector3::Zero;
    };
    HeldPose m_heldPose[5];
    // 手のボーンのワールド位置・回転(スケールは除く)。取得できなければfalse。
    bool GetHandWorld(const Math::Vector3& playerPos, float playerYaw, Math::Vector3& outPos, Math::Quaternion& outRot) const;
    // itemを手に持ったときのワールド位置・回転(オイラー角,ラジアン)。ボーンが無ければfalse。
    bool ComputeHeldItemWorld(Item item, const Math::Vector3& playerPos, float playerYaw, Math::Vector3& outPos, Math::Vector3& outRotRad) const;
    // One-shot: reads Attach_RightHand's current world position and converts it into a body-local
    // offset (undoing the player's current position/yaw), storing it into m_heldItemOffset. Only
    // runs while the player is standing still, so it captures the neutral idle hand pose rather than
    // a mid-swing extreme. No-ops (and can be retried next frame) if the model/node isn't ready yet.
    bool CalibrateHeldItemOffset(const Math::Vector3& playerPos, float playerYaw);

    // --- F-key interaction (doors + pickups) ---
    enum class InteractKind { None, Door, Pickup, Stimulus };
    InteractKind m_interactKind = InteractKind::None;
    // Only meaningful when m_interactKind == Pickup/Stimulus: the targeted PickupItem's/StimulusSource's entity.
    Entity m_interactEntity = INVALID_ENTITY;
    // Fキー連打防止のクールダウン(秒)。以前は関数内staticで全Player共有だったため、
    // 余分なPlayerスクリプトが1つあるだけで本物のPlayerのF入力が潰れていた。
    float m_interactCooldown = 0.0f;
    // "Press F" HUD prompt Sprite entity (named "InteractPrompt" in GameScene.json).
    Entity m_interactPromptEntity = INVALID_ENTITY;
    // 画面中央の点クロスヘア(GameScene.jsonの"Crosshair")。Fプロンプトと同じく操作対象を捉えている間だけ表示
    Entity m_crosshairEntity = INVALID_ENTITY;
    // クロスヘア(カメラ中央の視線)が重なっている、一番手前の操作できる物を m_interactKind/m_interactEntity
    // (扉なら m_doorTargetEntity/m_doorTargetAnimIdx)に入れる。家の壁・床・天井の奥にある物は対象外
    void UpdateInteractionTarget(const Math::Vector3& playerPos);
    // ゴーストがハント中か。ハント中は照明のスイッチ(壁スイッチ・紐スイッチ・ランプ)を操作できない
    bool IsGhostHunting() const;
    void UpdateInteractPrompt();
    bool GetCrosshairRay(Math::Vector3& outOrigin, Math::Vector3& outDir);

    // 開け閉めできる物(ドア・襖・戸棚など)の一覧。毎フレーム全エンティティの全アニメーションを
    // 走査すると重いので遅延キャッシュする。視線との当たり判定は、アニメーションが動かすノード(扉の板)の
    // メッシュを、その時の(開いていれば開いた)位置の箱として行う。
    struct DoorCandidate { Entity entity; int animIndex; std::vector<int> nodeIndices; };
    mutable std::vector<DoorCandidate> m_doorCandidateCache;
    mutable bool m_doorCandidateCacheBuilt = false;
    // 扉の板のノード名。壁・天井で遮られているかのレイでは、扉そのものは無視する
    mutable std::unordered_set<std::string> m_openableNodeNames;
    void RebuildDoorCandidateCache() const;
    // クロスヘアで捉えている開け閉めできる物。F押下時はこれだけを開け閉めする
    mutable Entity m_doorTargetEntity = INVALID_ENTITY;
    mutable int m_doorTargetAnimIdx = -1;

    // --- アニメーション ---
    // AnimationDataComponentは自分自身ではなく子の"Model"オブジェクトに付いているため、
    // 子階層を探して覚えておく(毎フレーム探索しなくていいようにStart()でキャッシュする)
    Entity m_animEntity = INVALID_ENTITY;
    std::string m_lastAnimClip; // 直近に再生要求したクリップ名(Crouchの再生し直し防止用)

    bool m_isCrouching = false;
    float m_crouchBlend = 0.0f; // 0=立ち,1=完全にしゃがみ。毎フレーム目標値へ滑らかに近づける
    float m_crouchBlendSpeed = 6.0f; // 大きいほど素早く追従する
    // Ctrlを押してからカメラが動き出すまでの遅延(秒)。
    // アニメーション側の動き出しのラグに合わせて調整する。
    float m_crouchCameraDelay = 0.1f;
    float m_crouchCameraDelayTimer = 0.0f;

    // --- しゃがみの体の大きさ(押し入れなど低い所に入るため) ---
    // しゃがむと当たり判定(子の"Collider"のカプセル)の上端をm_crouchColliderTopまで下げ、目線をm_crouchEyeHeightにする。
    // 頭の上に何かある所(押し入れの中段の下など)では立ち上がれない(Ctrlを押してもしゃがんだまま)。
    float m_crouchColliderTop = 1.0f;  // しゃがんだ時の当たり判定の上端(足元から, m)
    float m_crouchEyeHeight = 0.9f;    // しゃがんだ時の目線(足元から, m)
    Entity m_colliderEntity = INVALID_ENTITY;
    // 立っている時のカプセル(シーンに保存されている値)。実行中はここから毎フレームしゃがみ具合に合わせて作り直す
    bool m_standCapsuleValid = false;
    Math::Vector3 m_standCapsuleOffset;
    float m_standCapsuleHeight = 0.0f;
    float m_standCapsuleRadius = 0.0f;
    void CacheStandCapsule();
    void UpdateCrouchCollider();
    bool CanStandUp(const Math::Vector3& playerPos) const;

    // Walk/CrouchWalk系のアニメーション再生速度倍率。1.0で等倍。
    float m_animWalkSpeed = 1.0f;

    // Animation state machine: picks one of Idle/Walk/Crouch/CrouchWalk (or its FlashLight_*/UseItem_* variant
    // when a possessed item is currently equipped/displayed) and plays it by name via
    // AnimationManager::PlayAnimationByName - it only actually restarts the clip when the resolved
    // name changes, so calling this every frame is cheap.
    void UpdateAnimationState(bool isMoving);
    // True while the equipped item is owned and its held-in-hand model is being shown
    // (same condition UpdateHeldItem() uses to decide visibility).
    bool IsHoldingVisibleItem() const;
};

