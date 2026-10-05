#pragma once
#include "../System/RoomArea.h"

class GhostAI : public NativeScript {
public:
    enum class State
    {
        Idle,
        Wander,      // Room待機: ゴーストルーム(m_targetRoom)の中をうろつく
        HouseWander, // 徘徊: ゴーストルームの外(家の中)をNavMeshで歩き回る
        Hunt,        // Playerを追う(定期的に無条件で開始)
        Stun,
        Dead
    };

    void Awake() override;
    void Start() override;
    void Update(float deltaTime) override;
    void PostUpdate() override;

    void PreDraw() override;
    void Draw() override;
    void OnDestroy() override;

    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;

    void ImGuiUpdate() override;

    void OnCollisionEnter(GameObject* other) override;
    void OnCollisionStay(GameObject* other) override;

    void Exorcise();
    void SetState(State state);
    State GetState() const { return m_currentState; }
    void ApplyStun(float time) { SetState(State::Stun); m_stunTimer = time; }
    void SetTargetRoom(RoomArea* room) { m_targetRoom = room; }
    RoomArea* GetTargetRoom() const { return m_targetRoom; }

    bool IsExorcised() const { return m_isExorcised; }

private:
    void UpdateWander(float deltaTime, TransformData& cTrans);
    void UpdateHouseWander(float deltaTime, TransformData& cTrans);
    void UpdateHunt(float deltaTime, TransformData& cTrans);
    void StartHouseWander(const Math::Vector3& currentPos);
    std::vector<RoomArea*> CollectReachableCandidateRooms(const Math::Vector3& currentPos) const;
    void FacePosition(TransformData& cTrans, const Math::Vector3& dir, float deltaTime);
    void SetModelVisible(bool visible);
    Entity FindPlayerEntity();
    bool CanSeePlayer(const TransformData& cTrans, Math::Vector3& outPlayerPos);
    void EnsureHouseBounds();
    void EnsureStairsRoom();
    Math::Vector3 ClampToHouseBounds(const Math::Vector3& pos) const;
    void TryOpenNearbyDoor(const Math::Vector3& ghostPos, float deltaTime);
    void SetDoorPassThrough(bool enabled);
    Math::Vector3 GetEffectiveMoveTarget(const Math::Vector3& currentPos, const Math::Vector3& finalTarget);
    bool IsInStairsArea(const Math::Vector3& pos, float padOverride = -1.0f) const;
    Math::Vector3 GetStairsCrossingMove(float deltaTime);
    void StartStairsCrossing(const Math::Vector3& currentPos, const Math::Vector3& finalTarget, float speed);

    float m_moveSpeed = 1.0f;
    float m_changeDirTimer = 0.0f;
    Math::Vector3 m_moveDir = { 0, 0, 0 };
    bool m_isExorcised = false;

    State m_currentState = State::Idle;

    // --- 家の範囲(HouseWander/Huntの移動先をこの範囲内に制限する) ---
    // 全RoomAreaのAABBを合成し、部屋同士の境界(廊下など)も含まれるように少し余裕を持たせたもの。
    bool m_houseBoundsValid = false;
    Math::Vector3 m_houseBoundsMin = { 0, 0, 0 };
    Math::Vector3 m_houseBoundsMax = { 0, 0, 0 };
    float m_houseBoundsMargin = 0.5f; //部屋AABBから外側に広げる余裕(m)

    // --- 家の中の徘徊(Room待機 <-> HouseWander) ---
    // Wander(Room待機)中はm_houseWanderTimerを減算し、0になったら他の部屋を目指してHouseWanderへ遷移する。
    // HouseWander中はm_houseWanderDurationTimerが切れるまで部屋を渡り歩き、切れたらゴーストルームへ戻り始め
    // (m_houseWanderReturning=true)、到着したらWander(Room待機)へ戻る。
    // 移動先の部屋はNavMesh::IsReachable()で実際に到達可能なものだけ選ぶ
    // (家が複雑な形などでNavMeshが繋がっていない部屋を選ぶと立ち往生してしまうため)。
    // それでも進めなくなった場合の保険としてm_houseWanderStuckTimerで立ち往生を検知する。
    float m_houseWanderTimer         = 0.0f;  // 次に徘徊を始めるまでの残り時間(Room待機中)
    float m_houseWanderIntervalMin   = 15.0f; // 徘徊を始めるまでの間隔の最小値(秒)
    float m_houseWanderIntervalMax   = 30.0f; // 徘徊を始めるまでの間隔の最大値(秒)
    float m_houseWanderDurationTimer = 0.0f;  // 徘徊終了(帰還開始)までの残り時間(HouseWander中)
    float m_houseWanderDurationMin   = 10.0f; // 徘徊を続ける時間の最小値(秒)
    float m_houseWanderDurationMax   = 20.0f; // 徘徊を続ける時間の最大値(秒)
    bool  m_houseWanderReturning     = false; // trueの間はゴーストルームへの帰還中
    Math::Vector3 m_houseWanderTargetPos = { 0, 0, 0 };
    float m_roomArriveThreshold = 1.5f;       // 目的地にこの距離まで近づいたら到着とみなす
    Math::Vector3 m_houseWanderLastPos = { 0, 0, 0 }; // 立ち往生検知用の直近位置
    float m_houseWanderStuckTimer = 0.0f;     // 進んでいないまま経過した時間
    float m_houseWanderStuckTimeout = 5.0f;   // これだけ進んでいなければ目的地を諦める(秒)

    // Progress is measured over a window instead of frame-to-frame: re-anchoring every single
    // frame only catches a full stop, not rapid back-and-forth oscillation (e.g. flapping between
    // two slightly different recomputed paths at a tight pinch point), since each individual frame
    // can show >10cm movement even though net progress over a second is zero. Comparing position
    // only once per window catches that case too.
    float m_stuckCheckWindow = 1.0f;
    float m_stuckCheckMinProgress = 0.5f;
    float m_houseWanderStuckCheckTimer = 0.0f;

    // Shared "quick recovery" delay: used by both UpdateHouseWander and UpdateHunt's stuck-checks
    // to grant a brief pass-through window (reusing m_doorPassThroughTimer/Duration) well before
    // the full give-up timeout, so minor collision snags don't need a full destination change to
    // resolve.
    float m_quickStuckPassThroughDelay = 1.0f;
    Math::Vector3 m_huntStuckLastPos = { 0, 0, 0 };
    float m_huntStuckTimer = 0.0f;
    float m_huntStuckCheckTimer = 0.0f;

    // --- ハント(Hunt) ---
    // 以前は視認(CanSeePlayer)によってのみ開始していたが、今は位置に関わらず定期的に無条件で開始する
    // タイマーに変更(m_huntTriggerTimer)。Hunt中はPlayerの現在位置を直接追跡するので、
    // CanSeePlayer/m_hasLastKnownPlayerPosはここでは使わず(関数自体は残している)。
    float m_huntTimer = 0.0f;              // ハント終了までの残り時間(Hunt中)
    float m_huntDurationMin = 4.0f;        // ハント継続時間の最小値(秒)
    float m_huntDurationMax = 8.0f;        // ハント継続時間の最大値(秒)
    float m_huntSpeedMultiplier = 1.8f;    // ハント中の移動速度倍率
    float m_huntTriggerTimer = 0.0f;       // 次にハントを開始するまでの残り時間(Room待機/徘徊中)
    float m_huntTriggerIntervalMin = 20.0f;// ハント開始までの間隔の最小値(秒)
    float m_huntTriggerIntervalMax = 40.0f;// ハント開始までの間隔の最大値(秒)
    Entity m_playerEntity = INVALID_ENTITY;

    // --- デバッグ表示: 行き先/経路の可視化 ---
    bool m_showPathDebug = true;
    Math::Vector3 m_debugFinalTarget = { 0, 0, 0 }; // 実際に目指している最終目的地(Room中心かPlayer位置)
    Math::Vector3 m_debugMoveTarget  = { 0, 0, 0 }; // GetEffectiveMoveTarget後の実際のNavMesh移動目標

    // --- 視認・経路探索 (現在はHunt開始には未使用。関数・フィールドは残している) ---
    float m_visionRange = 15.0f;           // 視認できる最大距離
    float m_visionAngle = 100.0f;          // 視野角(度)
    float m_eyeHeight = 1.5f;              // 視線レイの発射高さ(足元からの高さ)
    float m_searchGiveUpDistance = 1.0f;   // 最終目撃地点にこの距離まで近づいても見つからなければ捜索終了
    Math::Vector3 m_lastKnownPlayerPos = { 0, 0, 0 };
    bool m_hasLastKnownPlayerPos = false;

    // --- ドア開閉(HouseWander/Huntの移動中に近づいた閉まっているドアを自動的に開ける) ---
    float m_doorCheckTimer = 0.0f;
    float m_doorPassThroughTimer = 0.0f;
    float m_doorPassThroughDuration = 3.5f;
    // Safety-net interval for NavMeshManager::MoveToward: it now mainly recomputes the path when
    // the target has actually moved (see its own comment), so this only matters as a rare fallback
    // in case something else changed underneath without the target moving. Kept long so it doesn't
    // become the primary re-trigger again - a short value here caused the same "commits to a
    // waypoint then immediately reverses" flapping this was meant to fix, just on a slower cycle.
    float m_pathUpdateInterval = 4.0f;
    // How close (in a straight line) the ghost gets to a NavMesh waypoint before advancing to the
    // next one. A large value lets it "cut the corner" between two waypoints in a straight line
    // that isn't clipped to the walkable polygon at all, which can cross outside the room's inset
    // boundary right at a tight turn - looking like scraping along the wall/corner. This is kept
    // small (well under the room's own 0.5m wall inset minus the ghost's own collider radius) so it
    // follows the path closely enough to stay inside that inset boundary through turns.
    float m_pathNodeReachThreshold = 0.35f;

    // The RoomArea marked m_isStairs, cached at Start(). Used to explicitly route through the
    // stairs' own position whenever the intended target is on a different floor - see
    // GetEffectiveMoveTarget's comment for why this exists instead of just trusting NavMesh's own
    // long-range pathfinding across floors.
    RoomArea* m_stairsRoom = nullptr;
    float m_floorHeightThreshold = 1.5f; // Y gap beyond which two positions are treated as different floors
    // Hysteresis state for the floor-crossing decision above: without this, a Y gap sitting right
    // at m_floorHeightThreshold (e.g. from gravity/ground-raycast jitter while on the stairs, or
    // just walking near that height difference on flat ground) could flip the effective target
    // between the real destination and the stairs waypoint every single recompute, looking like
    // repeatedly starting forward and immediately reversing.
    bool m_isCrossingFloors = false;
    // Snapshot of the exit landing height (stairs' own min.y or max.y), taken the instant
    // m_isCrossingFloors becomes true. During Hunt, finalTarget is the player's live position and
    // can itself change floor while the ghost is mid-crossing; re-deriving the exit landing fresh
    // from that moving target every frame let it flip mid-flight, so the exit check would compare
    // against a DIFFERENT landing than the one the crossing actually started toward - misreading
    // "still en route" as "just arrived" and releasing early, which immediately re-triggered a
    // crossing back the other way from nearly the same spot (visibly: repeatedly climbing partway
    // then reversing). Freezing it for the whole crossing removes that live dependency.
    float m_frozenExitLandingY = 0.0f;
    // Set the moment m_isCrossingFloors releases (real height reached the exit landing), cleared once
    // position actually leaves the stairs' padded area. Without this, the release comparison (against
    // the stairs' own real landing height) and the re-entry comparison (against the wander target's
    // Y, which is a room's volumetric GetCenter() - typically ~2m above its actual floor, not the
    // floor height itself) don't share a reference point: right after releasing near the true landing,
    // the ghost's real height can still be more than m_floorHeightThreshold below the target's
    // approximate center height, immediately satisfying the enter condition again and re-latching the
    // very next frame - repeating indefinitely while still standing right at the bottom of the stairs.
    bool m_recentlyExitedStairs = false;
    // Real stairs are built from discrete stepped risers in the collision mesh (not a smooth ramp),
    // and the ghost's capsule collider has no step-up logic - it simply bumps into each riser's
    // vertical face and gets pushed back, unable to climb at all. Rather than build a general step-
    // offset system, keep pass-through active for the whole time the ghost's XZ position is inside
    // the stairs' footprint (padded a bit), on top of the door-crossing case; see Update().
    bool m_wasPassThroughActive = false;
    float m_stairsPassThroughPadding = 1.5f;
    // Separate padding used only to decide when to STOP driving movement via GetStairsCrossingMove
    // and hand back off to normal NavMesh pathing (see UpdateHouseWander/UpdateHunt). Originally kept
    // small (0.3) on the assumption that a bigger value would walk past real NavMesh/floor coverage,
    // but with height now driven by GetStairsCrossingMove's own Z-progress interpolation rather than
    // gravity/raycast, the real safety cutoff is m_isCrossingFloors (it goes false once actual height
    // matches the destination floor, well before this padding distance matters) - a small pad instead
    // caused the opposite problem: handing off to MoveToward right at the exit edge, which snapped
    // the ghost back toward the stairs' own polygon before it could make any real progress into the
    // next room, so the two systems fought over control at that boundary indefinitely. Larger pad/
    // overshoot gives MoveToward a position solidly inside the neighboring room's own polygon instead.
    //
    // 1.0 still wasn't enough: at that landing point, findNearestPoly could snap to either the
    // stairs' own polygon or the neighboring room's depending on tiny positional noise between path
    // recomputes, so MoveToward's cached path kept resolving to two different first waypoints - one
    // back toward the stairs, one onward toward the real target - and never finished either, looking
    // like standing still. Pushing the landing further into the room avoids that boundary entirely -
    // capped at 1.3 (not pushed further) because the south/bottom landing has less real clearance:
    // the actual floor there ends only ~1.6m past the stairs' own bounds (measured directly in
    // Blender), so too large a value would walk it into the house's exterior void on that side.
    float m_stairsNavExitPadding = 1.3f;
    // Latches true the moment GetStairsCrossingMove starts driving movement, and only clears once
    // m_isCrossingFloors goes false (real height has reached the exit landing). Re-testing the padded
    // position bounds every frame (the original design) let control flip briefly back to MoveToward
    // whenever position wobbled across that padded boundary mid-crossing, and MoveToward routing back
    // through the stairs' synthetic bridge portal each time undid the crossing-move's own progress -
    // an oscillation neither system could win. Latching removes position from the decision entirely
    // once crossing has begun: height alone decides when it's over.
    bool m_stairsCrossingLatched = false;
    // Fixed start/end points and elapsed time for the current stairs crossing, set once when the
    // latch engages. Position-derived steering (recomputing direction from cTrans.m_position each
    // call) kept reversing direction mid-crossing for reasons that didn't trace back to collision or
    // MoveToward interference (both were ruled out and it still happened) - time-based interpolation
    // between two fixed points removes cTrans.m_position from the calculation entirely, so nothing
    // can make it un-progress once started.
    Math::Vector3 m_stairsCrossStart = { 0, 0, 0 };
    Math::Vector3 m_stairsCrossEnd = { 0, 0, 0 };
    float m_stairsCrossElapsed = 0.0f;
    float m_stairsCrossDuration = 1.0f;
    float m_doorCheckInterval = 0.25f;     // ドアの近接判定間隔(秒) - 毎フレーム全エンティティを走査しないよう間引く
    float m_doorOpenRange = 2.0f;          // この距離以内の閉まっているドアは自動的に開ける

    // Ghost.gltfのアニメーションは 0:Ghost_Death, 1:Ghost_Move の2つしかない。
    // 専用のIdle/Huntモーションがないので、とりあえずMoveを使う
    // (Huntは既存コードでSpeed=1.5倍にして走っているように見せている)。
    int m_animIdle = 1;
    int m_animWander = 1;
    int m_animHunt = 1;
    int m_animDead = 0;

    RoomArea* m_targetRoom = nullptr;
    float m_stunTimer = 0.0f;

    // AnimationDataComponentは自分自身ではなく子の"Model"オブジェクトに付いているため、
    // 子階層を探して覚えておく(毎フレーム探索しなくていいようStart()でキャッシュする)
    Entity m_animEntity = INVALID_ENTITY;

    // --- 重力 ---
    float m_velocityY       = 0.0f;   // 落下速度
    float m_gravityStrength = 9.8f;   // 重力強度
    bool  m_isGrounded      = false;  // 接地しているか
    float m_groundOffset    = 0.0f;   // ground-height Y offset (tune if the model origin isn't at the feet)
    // Set for one frame by UpdateHouseWander/UpdateHunt right after GetStairsCrossingMove computes
    // height directly from Z-progress along the stairs - tells the gravity/ground-raycast block to
    // skip itself so it doesn't immediately override that with a raycast against the stair treads.
    bool  m_skipGravityThisFrame = false;
};
