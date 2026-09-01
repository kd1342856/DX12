#pragma once
#include "../System/RoomArea.h"

class GhostAI : public NativeScript {
public:
    enum class State
    {
        Idle,
        Wander,      // Room�ҋ@: �S�[�X�g���[��(m_targetRoom)�̒���������
        HouseWander, // �p�j: �S�[�X�g���[���̊O(�Ƃ̒�)��NavMesh�ŕ������
        Hunt,        // Player��ǂ�(�����I�ɖ������ŊJ�n)
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

    // --- �Ƃ͈̔�(HouseWander/Hunt�̈ړ������͈͓̔��ɐ�������) ---
    // �SRoomArea��AABB���������A�������m�̌���(�L���Ȃ�)��������悤�ɏ����]�T�������������́B
    bool m_houseBoundsValid = false;
    Math::Vector3 m_houseBoundsMin = { 0, 0, 0 };
    Math::Vector3 m_houseBoundsMax = { 0, 0, 0 };
    float m_houseBoundsMargin = 0.5f; //����AABB����O���ɍL����]�T(m)

    // --- �Ƃ̒��̜p�j(Room�ҋ@ <-> HouseWander) ---
    // Wander(Room�ҋ@)����m_houseWanderTimer�����Z���A0�ɂȂ����瑼�̕�����ڎw����HouseWander�֑J�ڂ���B
    // HouseWander����m_houseWanderDurationTimer���؂��܂ŕ�����n������A�؂ꂽ��S�[�X�g���[���֖߂�n�߂�
    // (m_houseWanderReturning=true)�A����������Wander(Room�ҋ@)�֖߂�B
    // �ړ���̕�����NavMesh::IsReachable()�Ŏ��ۂɓ��B�\�Ȃ��̂�����I��
    // (�Ƃ����G�Ȍ`�Ȃǂ�NavMesh���q�����Ă��Ȃ�������I�ԂƗ����������Ă��܂�����)�B
    // ����ł��i�߂Ȃ��Ȃ����ꍇ�̕ی��Ƃ���m_houseWanderStuckTimer�ŗ������������m����B
    float m_houseWanderTimer         = 0.0f;  // ���ɜp�j���n�߂�܂ł̎c�莞��(Room�ҋ@��)
    float m_houseWanderIntervalMin   = 15.0f; // �p�j���n�߂�܂ł̊Ԋu�̍ŏ��l(�b)
    float m_houseWanderIntervalMax   = 30.0f; // �p�j���n�߂�܂ł̊Ԋu�̍ő�l(�b)
    float m_houseWanderDurationTimer = 0.0f;  // �p�j�I��(�A�ҊJ�n)�܂ł̎c�莞��(HouseWander��)
    float m_houseWanderDurationMin   = 10.0f; // �p�j�𑱂��鎞�Ԃ̍ŏ��l(�b)
    float m_houseWanderDurationMax   = 20.0f; // �p�j�𑱂��鎞�Ԃ̍ő�l(�b)
    bool  m_houseWanderReturning     = false; // true�̊Ԃ̓S�[�X�g���[���ւ̋A�Ғ�
    Math::Vector3 m_houseWanderTargetPos = { 0, 0, 0 };
    float m_roomArriveThreshold = 1.5f;       // �ړI�n�ɂ��̋����܂ŋ߂Â����瓞���Ƃ݂Ȃ�
    Math::Vector3 m_houseWanderLastPos = { 0, 0, 0 }; // �����������m�p�̒��߈ʒu
    float m_houseWanderStuckTimer = 0.0f;     // �i��ł��Ȃ��܂܌o�߂�������
    float m_houseWanderStuckTimeout = 5.0f;   // ���ꂾ���i��ł��Ȃ���ΖړI�n����߂�(�b)

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

    // --- �n���g(Hunt) ---
    // �ȑO�͎��F(CanSeePlayer)�ɂ���Ă̂݊J�n���Ă������A���͈ʒu�Ɋւ�炸�����I�ɖ������ŊJ�n����
    // �^�C�}�[�ɕύX(m_huntTriggerTimer)�BHunt����Player�̌��݈ʒu�𒼐ڒǐՂ���̂ŁA
    // CanSeePlayer/m_hasLastKnownPlayerPos�͂����ł͎g�킸(�֐����͎̂c���Ă���)�B
    float m_huntTimer = 0.0f;              // �n���g�I���܂ł̎c�莞��(Hunt��)
    float m_huntDurationMin = 4.0f;        // �n���g�p�����Ԃ̍ŏ��l(�b)
    float m_huntDurationMax = 8.0f;        // �n���g�p�����Ԃ̍ő�l(�b)
    float m_huntSpeedMultiplier = 1.8f;    // �n���g���̈ړ����x�{��
    float m_huntTriggerTimer = 0.0f;       // ���Ƀn���g���J�n����܂ł̎c�莞��(Room�ҋ@/�p�j��)
    float m_huntTriggerIntervalMin = 20.0f;// �n���g�J�n�܂ł̊Ԋu�̍ŏ��l(�b)
    float m_huntTriggerIntervalMax = 40.0f;// �n���g�J�n�܂ł̊Ԋu�̍ő�l(�b)
    Entity m_playerEntity = INVALID_ENTITY;

    // --- �f�o�b�O�\��: �s�����_/�o�H�̉��� ---
    bool m_showPathDebug = true;
    Math::Vector3 m_debugFinalTarget = { 0, 0, 0 }; // ���ۂɖڎw���Ă���ŏI�ړI�n(Room���S��Player�ʒu)
    Math::Vector3 m_debugMoveTarget  = { 0, 0, 0 }; // GetEffectiveMoveTarget��̎��ۂ�NavMesh�ړ��ڕW

    // --- ���F�E�o�H�T�� (���݂�Hunt�J�n�ɂ͖��g�p�B�֐��E�t�B�[���h�͎c���Ă���) ---
    float m_visionRange = 15.0f;           // ���F�ł���ő勗��
    float m_visionAngle = 100.0f;          // ����p(�x)
    float m_eyeHeight = 1.5f;              // �������C�̔��ˍ���(��������̍���)
    float m_searchGiveUpDistance = 1.0f;   // �ŏI�ڌ��n�_�ɂ��̋����܂ŋ߂Â��Ă�������Ȃ���Α{���I��
    Math::Vector3 m_lastKnownPlayerPos = { 0, 0, 0 };
    bool m_hasLastKnownPlayerPos = false;

    // --- �h�A�J��(HouseWander/Hunt�̈ړ����ɋ߂Â����܂��Ă���h�A�������I�ɊJ����) ---
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
    float m_doorCheckInterval = 0.25f;     // �h�A�̋ߐڔ���Ԋu(�b) - ���t���[���S�G���e�B�e�B�������Ȃ��悤�Ԉ���
    float m_doorOpenRange = 2.0f;          // ���̋����ȓ��̕܂��Ă���h�A�͎����I�ɊJ����

    // Ghost.gltf�̃A�j���[�V������ 0:Ghost_Death, 1:Ghost_Move ��2�����Ȃ��B
    // ��p��Idle/Hunt���[�V�������Ȃ��̂ŁA�Ƃ肠����Move���g��
    // (Hunt�͊����R�[�h��Speed=1.5�{�ɂ��đ����Ă���悤�Ɍ����Ă���)�B
    int m_animIdle = 1;
    int m_animWander = 1;
    int m_animHunt = 1;
    int m_animDead = 0;

    RoomArea* m_targetRoom = nullptr;
    float m_stunTimer = 0.0f;

    // AnimationDataComponent�͎������g�ł͂Ȃ��q��"Model"�I�u�W�F�N�g�ɕt���Ă��邽�߁A
    // �q�K�w��T���Ċo���Ă���(���t���[���T�����Ȃ��Ă����悤Start()�ŃL���b�V������)
    Entity m_animEntity = INVALID_ENTITY;

    // --- �d�� ---
    float m_velocityY       = 0.0f;   // �������x
    float m_gravityStrength = 9.8f;   // �d�͋��x
    bool  m_isGrounded      = false;  // �ڒn���Ă��邩
    float m_groundOffset    = 0.0f;   // ground-height Y offset (tune if the model origin isn't at the feet)
    // Set for one frame by UpdateHouseWander/UpdateHunt right after GetStairsCrossingMove computes
    // height directly from Z-progress along the stairs - tells the gravity/ground-raycast block to
    // skip itself so it doesn't immediately override that with a raycast against the stair treads.
    bool  m_skipGravityThisFrame = false;
};
