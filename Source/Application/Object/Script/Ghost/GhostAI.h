#pragma once
#include "../System/RoomArea.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"

class StimulusSource;

class GhostAI : public NativeScript {
public:
    enum class State
    {
        Idle,
        Wander,      // Room待機: ゴーストルーム(m_targetRoom)の中をうろつく
        HouseWander, // 徘徊: ゴーストルームの外(家の中)をNavMeshで歩き回る
        Hunt,        // Playerを追う(定期的に無条件で開始)
        Stun,
        Dead,
        Lured        // 刺激(StimulusSource)に誘導されて向かう/その場に留まる
    };

    // ゴーストの種類。ゲーム開始時(Start)にランダムで抽選する(m_randomizeType)。
    enum class GhostType
    {
        Jibakurei, // 地縛霊: 刺激に反応しない。香はハント中ならいつでも効く
        Kageonna,  // 影女: 徘徊中は点灯中の光源に誘導される。香はハント中、影女がいる部屋(RoomArea)の部屋の電気(天井照明等。ランプ・フラッシュライトは除く)が点いている時だけ効く。勝手に照明を消す(点けることはしない)
        Yanari,    // 家鳴: 音に誘導される(未実装 - 現状は地縛霊と同じ挙動。抽選対象外)
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

    GhostType GetGhostType() const { return m_ghostType; }
    // ゲーム中に種類を変える(発表用デバッグモード等)。照明のいたずらの間隔も新しい種類に合わせる
    void SetGhostType(GhostType type);

    // 顔のワールド座標。Playerを捕まえた時、カメラをここに向けて固定する
    Math::Vector3 GetFacePosition() const;

    // 香を当てられた時にPlayerから呼ばれる。退治(スタン)できるのはハント中だけで、さらに種類ごとの
    // 条件を満たしていればスタンしてtrueを返す。
    // 条件を満たしていない場合(失敗)、Hunt中ならHuntが続いたまま残り時間が延長される。
    bool TryIncenseStun(float stunTime);

private:
    void UpdateWander(float deltaTime, TransformData& cTrans);
    void UpdateHouseWander(float deltaTime, TransformData& cTrans);
    void UpdateHunt(float deltaTime, TransformData& cTrans);
    // ハント中: Playerが押入れに隠れたかを調べる。隠れるところを見ていれば見つけに行く(m_foundHideSpot)、
    // 見ていなければその辺りを巡回する(m_hidePatrolTimer)。隠れている間は視認・追跡できない
    void UpdateHidingPlayer(float deltaTime);
    void ResetHidingState();
    void PickHuntSearchRoom(const Math::Vector3& currentPos);
    // ハント開始時に玄関ドアを閉めて施錠する(終了時はDoorLock::UnlockAllで解錠)
    void LockEntranceDoor();
    // 近くの点灯中の照明を点滅させる
    void UpdateLightFlicker(float deltaTime, const Math::Vector3& ghostPos);
    // 全ての照明(StimulusSourceのLight)を消す。ハント終了時に呼ぶ
    static void TurnOffAllLights();
    bool IsPlayerHoldingLitFlashLight();
    void UpdateLured(float deltaTime, TransformData& cTrans);
    // HouseWander/Hunt/Luredで共通の、finalTargetへNavMesh(階段区間は直接補間)で1フレーム分進む処理。
    // 位置・向き(m_moveDir)まで更新する。
    void MoveTowardTarget(float deltaTime, TransformData& cTrans, const Math::Vector3& finalTarget, float speed);
    // 影女/家鳴: 対応する刺激を感知したらLuredへ遷移する(Wander/HouseWander中に呼ぶ)
    void CheckStimulus(float deltaTime, const Math::Vector3& currentPos);
    // 影女/家鳴の兆候: 近くで点いている発生源を勝手に消す(影女なら照明を消す)
    void UpdateTell(float deltaTime, const Math::Vector3& currentPos);
    // 次の照明のいたずらまでの時間(種類ごとの間隔から抽選)
    float PickTellInterval() const;
    // Luredを終えてゴーストルームへ歩いて帰る(Hunt終了時と同じ帰り方)
    void ReturnToGhostRoom(const Math::Vector3& currentPos);
    // 影女用: ghostPosが点いている部屋の電気に照らされているか(照らす範囲内で、壁・床・天井に遮られていない)
    // (部屋の電気 = m_isRoomLightの光のStimulusSourceだけ。ランプ・フラッシュライトは含まない)
    bool IsLitByRoomLight(const Math::Vector3& ghostPos) const;
    // 影女の退治条件: ghostPosがいる部屋(RoomArea。ゴーストルームの抽選と同じ箱)の中にある部屋の電気が点いているか。
    // どの部屋の箱にも入っていない場所(廊下の端など)ではIsLitByRoomLightで判定する
    bool IsRoomLightOnAt(const Math::Vector3& ghostPos) const;
    // posが点いている照明の照らす範囲内(スポットライトの円錐+距離、照明側のポイントライトの距離)にあるか。
    // StimulusSource管理の照明は点滅中でも点灯扱い(IsActive)で判定する。ハント中の減速に使う。
    bool IsPositionLit(const Math::Vector3& pos) const;
    // Playerの全身(足元・胴・頭)が視界内・遮蔽なしで見えているか。ハント中の視認加速に使う。
    bool CanSeePlayerFullBody(const TransformData& cTrans);
    // ハント中の実際の移動速度(m/s)。視認加速と照明による減速を反映する。
    float GetHuntSpeed() const;
    // 札で退治された後の除霊演出(発光・閃光・昇天)。終わったらGameSequenceへクリアを通知する
    void UpdateExorciseEffect(float deltaTime, TransformData& cTrans);
    // 除霊の光の粒を1つ出す。burstなら札を貼った瞬間の勢いのある粒にする
    void EmitExorciseParticle(bool burst);
    // 吐息・ハント中のループ音。毎フレーム呼ぶ(除霊・スタン中も呼んでフェードアウトさせる)
    void UpdateSounds(float deltaTime, const Math::Vector3& ghostPos);
    void StopAllSounds();
    void StartHouseWander(const Math::Vector3& currentPos);
    std::vector<RoomArea*> CollectReachableCandidateRooms(const Math::Vector3& currentPos) const;
    void FacePosition(TransformData& cTrans, const Math::Vector3& dir, float deltaTime);
    // Playerに触れていたらゲームオーバーを通知する。衝突イベント(OnCollisionEnter)は壁抜け中(トリガー化)には
    // 発火せず、接触した瞬間の1回しか来ないため取りこぼす。距離で毎フレーム判定する
    void CheckPlayerContact(const Math::Vector3& ghostPos);
    // Playerを捕まえた後(ゲームオーバー中): その場で止まってPlayerの方を向き、姿を見せる。
    // 押入れの襖を開けて見つけた場合は覗き込み(UpdateClosetPeek)をする
    void UpdateCaught(float deltaTime, TransformData& cTrans);
    // 押入れの襖を開けた後: 目線の高さまで沈みながら顔をPlayerの目の前まで近づけ、少ししてゲームオーバー
    void UpdateClosetPeek(float deltaTime, TransformData& cTrans);
    void SetModelVisible(bool visible);
    Entity FindPlayerEntity();
    bool CanSeePlayer(const TransformData& cTrans, Math::Vector3& outPlayerPos);
    void EnsureHouseBounds();
    void EnsureStairsRoom();
    Math::Vector3 ClampToHouseBounds(const Math::Vector3& pos) const;
    // 階段の入口(今いる階側の階段の端の少し手前)。exitLandingYは渡った先の床の高さ。
    Math::Vector3 GetStairsEntryPoint(float exitLandingY) const;
    void TryOpenNearbyDoor(const Math::Vector3& ghostPos, float deltaTime);
    // 引き違いの襖の、開いている側のすき間(gap)を通らせる。経路がこの出入口を通る時だけ
    void TrySteerThroughGap(const Math::Vector3& ghostPos, const Math::Vector3& gapCenter);
    void SetDoorPassThrough(bool enabled);
    // 家具(Propsの静的な箱の当たり判定)。NavMeshは部屋の形だけで家具を知らないので、経路が家具を突っ切る。
    // ぶつかって詰まると壁抜け(当たり判定のトリガー化)が入ってすり抜けてしまうため、移動時に自前で避け、
    // 壁抜け中でも家具の中には入らないよう押し出す
    void EnsurePropBoxes(float deltaTime);
    Math::Vector3 AvoidProps(const Math::Vector3& currentPos, const Math::Vector3& nextPos);
    Math::Vector3 PushOutOfProps(const Math::Vector3& pos) const;
    bool PropOverlapsHeight(const DirectX::BoundingBox& box, float feetY) const;
    Math::Vector3 GetEffectiveMoveTarget(const Math::Vector3& currentPos, const Math::Vector3& finalTarget);
    bool IsInStairsArea(const Math::Vector3& pos, float padOverride = -1.0f) const;
    Math::Vector3 GetStairsCrossingMove(float deltaTime);
    void StartStairsCrossing(const Math::Vector3& currentPos, const Math::Vector3& finalTarget, float speed);

    // 速度はエディタではkm/hで扱う(内部の移動計算では/3.6してm/sにする)
    static constexpr float kKmhToMs = 1.0f / 3.6f;
    float m_moveSpeedKmh = 3.6f;           // 通常(待機・徘徊・誘導)の移動速度(km/h)
    float m_changeDirTimer = 0.0f;
    Math::Vector3 m_moveDir = { 0, 0, 0 };
    bool m_isExorcised = false;

    // --- 除霊演出(札で退治された時) ---
    // 札を貼った瞬間に強く光り(閃光)、その後はその場で足元から光の粒になって消えていく
    // (SkinningShaderの足元からのディゾルブ + ParticleManagerの光の粒)。
    // 演出が終わるまでGameSequenceへのクリア通知を遅らせる(結果画面に切り替わる前に見せるため)。
    float m_exorciseDuration    = 3.0f;   // 演出全体の長さ(秒)
    float m_exorciseModelHeight = 2.0f;   // 消えていく範囲(足元からこの高さまで)(m)
    float m_exorciseEdgeWidth   = 0.15f;  // 消える境界の光る帯の幅(m)
    float m_exorciseEdgeGlow    = 12.0f;  // 消える境界の光の強さ(HDR)
    float m_exorciseGlowStrength = 6.0f;  // 発光(Bloomに乗るHDRの強さ)
    float m_exorciseLightIntensity = 8.0f;// 閃光の点光源の最大強度
    float m_exorciseLightRange  = 8.0f;   // 閃光の点光源の届く距離(m)
    float m_exorciseSoundVolume = 0.9f;   // 除霊SE(札の音+りん+消えていく音)の音量
    Math::Vector3 m_exorciseColor = { 1.0f, 0.85f, 0.55f }; // 浄化の光の色(淡い金色)
    // 消える境界から湧き上がって消えていく光の粒(ParticleManager)
    float m_exorciseParticleRate   = 90.0f; // 1秒あたりに出す数
    float m_exorciseParticleRadius = 0.45f; // 消える境界のこの半径の円内から出す(m)
    int   m_exorciseParticleBurst  = 50;    // 札を貼った瞬間にまとめて出す数
    float m_exorciseParticleAccum  = 0.0f;
    float m_exorciseTimer = 0.0f;
    bool  m_exorciseNotified = false;
    Math::Vector3 m_exorciseStartPos = { 0, 0, 0 };
    float m_exorciseCutY = 0.0f;           // 今消えている境界の高さ(ワールドY)。光の粒はここから出す
    std::shared_ptr<GameObject> m_exorciseLightObj; // 演出中だけ生成する点光源

    State m_currentState = State::Idle;

    // --- 種類 ---
    GhostType m_ghostType = GhostType::Jibakurei;
    bool m_randomizeType = true; // trueならStart()で種類を抽選する(falseならm_ghostTypeをそのまま使う)

    // --- 誘導(Lured) ---
    // Wander/HouseWander中、m_lureCheckIntervalごとに対応する刺激を探し、見つかればそこへ向かう。
    // 到着後はm_lureDwellDurationだけ留まり、時間切れか発生源が
    // 無効になったらゴーストルームへ帰る。帰った直後に同じ発生源へ即座に戻らないよう
    // m_lureCooldownの間は再感知しない。
    StimulusSource* m_lureSource = nullptr;
    bool  m_lureArrived = false;
    float m_lureCheckTimer = 0.0f;
    float m_lureCheckInterval = 0.5f;
    float m_lureArriveThreshold = 1.5f;  // 発生源にこの距離(XZ)まで近づいたら到着とみなす
    float m_lureDwellTimer = 0.0f;
    float m_lureDwellDuration = 20.0f;   // 到着後に留まる時間(秒)
    float m_lureCooldownTimer = 0.0f;
    float m_lureCooldown = 10.0f;        // 誘導を終えてから再感知するまでの時間(秒)

    // --- 香の失敗ペナルティ ---
    float m_failHuntExtension = 5.0f;    // 条件外で香を使われた時、Hunt中ならこの秒数だけHuntを延長する

    // --- 兆候(影女: 勝手に照明を消す) ---
    float m_tellTimer = 0.0f;
    float m_tellIntervalMin = 20.0f;
    float m_tellIntervalMax = 40.0f;
    // 影女は明かりを消して回るのが兆候なので、他の種類より頻繁に消す
    float m_kageonnaTellIntervalMin = 8.0f;
    float m_kageonnaTellIntervalMax = 15.0f;
    float m_tellRange = 8.0f;            // この距離以内で点いている発生源を勝手に消す

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

    // 詰まった時の脱出(UpdateHouseWander/UpdateHunt/UpdateLuredの立ち往生検知で使う)。
    // 以前は詰まると壁抜け(当たり判定のトリガー化)を数秒許可していたが、T字の角などで引っかかるたびに
    // 壁をすり抜けて追ってくるようになっていたので廃止した。代わりに経路を計算し直し、少し後ろ斜めへ
    // 下がってから進み直す(当たり判定は入ったままなので壁には入らない)。下がる向きは毎回左右を入れ替える。
    float m_unstickDelay    = 1.0f;   // これだけ進めなければ脱出する(秒)
    float m_unstickDuration = 0.6f;   // 後ろ斜めへ下がる時間(秒)
    float m_unstickCooldown = 1.5f;   // 次に脱出するまでの間隔(秒)
    float m_unstickTimer = 0.0f;
    // 同じ行き先へ向かう間に下がり直した回数。下がる動き自体が「進んだ」と数えられて立ち往生の時間が溜まらず、
    // たどり着けない行き先(部屋の中心が食卓の位置にある等)をいつまでも諦めなかったので、回数でも諦める
    int   m_unstickAttempts = 0;
    int   m_unstickMaxAttempts = 3;

    // 引き違いの襖のすき間を通る(TrySteerThroughGap)。NavMeshは出入口全体を通れる扱いなので、
    // そのままだと閉じている側の板を(当たり判定は無視するので)すり抜けて見える
    Math::Vector3 m_gapPos = { 0, 0, 0 };
    float m_gapTimer = 0.0f;              // >0の間はすき間の真ん中へ真っ直ぐ向かう
    Math::Vector3 m_lastGapPos = { 0, 0, 0 };
    float m_gapCooldownTimer = 0.0f;      // 同じすき間へ何度も向かい直さないよう、通った後しばらくは対象外
    float m_unstickCooldownTimer = 0.0f;
    float m_unstickSide = 1.0f;
    Math::Vector3 m_unstickDir = { 0, 0, 0 };
    // 詰まっていたら脱出を始める(stuckTimerは各ステートの立ち往生時間)
    void TryUnstick(float stuckTimer, const Math::Vector3& currentPos);
    Math::Vector3 m_huntStuckLastPos = { 0, 0, 0 };
    float m_huntStuckTimer = 0.0f;
    float m_huntStuckCheckTimer = 0.0f;

    // --- ハント(Hunt) ---
    // 開始は位置に関わらず定期的なタイマー(m_huntTriggerTimer)で無条件に行う。
    // Hunt中の追跡は視認(CanSeePlayer)で行う: 見えている間はPlayerの現在位置を追い、
    // 見失ったら最終目撃地点(m_lastKnownPlayerPos)へ向かい、そこでも見つからなければ部屋を巡回して探す。
    float m_huntTimer = 0.0f;              // ハント終了までの残り時間(Hunt中)
    float m_huntDurationMin = 4.0f;        // ハント継続時間の最小値(秒)
    float m_huntDurationMax = 8.0f;        // ハント継続時間の最大値(秒)
    float m_huntSpeedKmh = 6.5f;           // ハント中の基本速度(km/h)。Playerを視認していない時はこの速度
    // --- 視認加速(ハント中) ---
    // Playerの全身を見続けている間、m_losTimerがm_losTimeToMax秒かけて溜まり、速度が
    // m_huntSpeedKmhからm_huntMaxSpeedKmhまで徐々に上がる。見失うとm_losDecayTime秒かけて戻る。
    float m_huntMaxSpeedKmh = 10.0f;       // 視認加速の最高速度(km/h)
    float m_losTimeToMax = 6.0f;           // 最高速度に達するまで全身を見続ける時間(秒)
    float m_losDecayTime = 2.0f;           // 見失ってから基本速度に戻るまでの時間(秒)
    float m_losTimer = 0.0f;               // 溜まった視認時間(0..m_losTimeToMax)
    bool  m_isSeeingFullBody = false;      // デバッグ表示用
    // --- 照明による減速(ハント中、全種類) ---
    float m_litSpeedMultiplier = 0.6f;     // 照らされている間の速度倍率
    bool  m_isInLight = false;             // デバッグ表示用
    float m_huntTriggerTimer = 0.0f;       // 次にハントを開始するまでの残り時間(Room待機/徘徊中)
    float m_huntTriggerIntervalMin = 20.0f;// ハント開始までの間隔の最小値(秒)
    float m_huntTriggerIntervalMax = 40.0f;// ハント開始までの間隔の最大値(秒)
    Entity m_playerEntity = INVALID_ENTITY;

    // --- デバッグ表示: 行き先/経路の可視化 ---
    bool m_showPathDebug = true;
    Math::Vector3 m_debugFinalTarget = { 0, 0, 0 }; // 実際に目指している最終目的地(Room中心かPlayer位置)
    Math::Vector3 m_debugMoveTarget  = { 0, 0, 0 }; // GetEffectiveMoveTarget後の実際のNavMesh移動目標

    // --- 視認・捜索 (Hunt中の追跡に使用) ---
    float m_visionRange = 15.0f;           // 視認できる最大距離
    float m_visionAngle = 100.0f;          // 視野角(度)
    float m_proximitySenseDistance = 1.5f; // この距離以内なら視野角外(背後)でも気付く(壁越しは不可)
    // --- 追跡(ハント中) ---
    // 一度視認したら、壁に隠れてもPlayerの現在位置をずっと追い続ける。視認加速が溜まらない
    // (全身が見えていない)状態がm_trackGiveUpTime秒続いたら追跡をやめ、最後に分かっていた位置からの捜索に移る。
    bool  m_isTrackingPlayer = false;
    float m_trackGiveUpTime = 5.0f;        // 視認加速が入らないまま、この秒数経ったら追跡停止
    float m_trackGiveUpTimer = 0.0f;
    std::string m_entranceDoorName = "Entrance_SlidingDoor"; // 玄関の引き戸(左右2枚とも名前に含む) // ハント中に施錠する玄関ドア(ドアアニメーション名に含まれる文字列)
    float m_lightFlickerRange = 4.0f;          // この水平距離以内の点灯中の照明を点滅させる
    float m_lightFlickerVerticalRange = 4.2f;  // 照明がゴーストの足元より上、この高さ以内(=同じ階の天井まで)なら対象
    float m_wanderFlickerChance = 0.2f;        // ハント以外で近くの照明が点滅し始める確率(1秒あたり)。ハント中は常に点滅
    float m_flashLightFlickerRange = 6.0f;     // この距離以内の点いたフラッシュライト(手持ち・床)も点滅させる。消しはしない
    float m_lightFlickerCheckTimer = 0.0f;
    bool m_isSensingLight = false;         // 点灯したフラッシュライトを持つPlayerに向かっているか(デバッグ表示用)
    float m_eyeHeight = 1.5f;              // 視線レイの発射高さ(足元からの高さ)
    float m_searchGiveUpDistance = 1.0f;   // 最終目撃地点にこの距離まで近づいても見つからなければ捜索終了
    Math::Vector3 m_lastKnownPlayerPos = { 0, 0, 0 };
    bool m_hasLastKnownPlayerPos = false;
    bool m_isSeeingPlayer = false;         // 直近のフレームでPlayerを視認していたか(デバッグ表示用)

    // --- 押入れに隠れたPlayer(ハント中) ---
    // 隠れる直前m_hideSpottedGrace秒以内に見ていたら、押入れの前まで行って襖を開け、ゲームオーバーにする。
    // 見ていなかったら(居場所の見当がついている時だけ)押入れの前を中心にしばらく巡回し、その後は普通の捜索に戻る。
    float m_hideSpottedGrace   = 1.0f;     // 隠れる直前この秒数以内に視認していたら「隠れるところを見た」
    float m_hideRevealDelay    = 0.8f;     // 顔を近づけ終わってからゲームオーバーまで(秒)
    // 襖を開けた後の覗き込み: しゃがみアニメーションでしゃがみ、顔がPlayerの目線の高さに来るよう高さを合わせて
    // 押入れの中へ顔を差し込み、目の前まで近づける(顔ドアップ)
    float m_peekStartDelay     = 0.3f;     // 襖を開けてから覗き込み始めるまで(秒)
    float m_peekMoveTime       = 0.6f;     // 顔を目の前まで近づけるのにかける時間(秒)
    float m_peekDistance       = 0.6f;     // 近づけ終わった時の、Playerの目からゴーストの顔までの距離(m)
    float m_crouchFaceHeight   = 1.1f;     // しゃがみアニメーション中の、足元から顔までの高さ(m)
    bool  m_peekStarted = false;
    Math::Vector3 m_peekStartPos;          // 覗き込み開始時のゴーストの位置
    Math::Vector3 m_peekDir;               // Playerの目から見たゴーストの方向(水平, 覗き込み開始時に固定)
    float m_catchRadius        = 1.0f;     // Playerとの水平距離がこれ以下で「触れた」としてゲームオーバー(m)
    float m_faceHeight         = 1.9f;     // 足元から顔までの高さ。捕まった時のカメラの注視点(m)
    float m_hidePatrolDuration = 10.0f;    // 見られずに隠れた時、押入れの辺りを巡回する時間(秒)
    float m_hidePatrolRadius   = 2.5f;     // 巡回する範囲(押入れの前からの半径, m)
    float m_hidePatrolRepick   = 3.0f;     // 巡回の行き先を選び直す間隔(秒)
    float m_timeSinceSawPlayer = 999.0f;
    int   m_lastHideSpot = -1;             // 前フレームでPlayerが隠れていた押入れ(-1=隠れていない)
    int   m_foundHideSpot = -1;            // 隠れるところを見た押入れ(-1=なし)
    float m_revealTimer = -1.0f;           // 襖を開けてからの経過(<0=まだ開けていない)
    float m_hidePatrolTimer = 0.0f;
    float m_hidePatrolRepickTimer = 0.0f;
    Math::Vector3 m_hidePatrolCenter = { 0, 0, 0 };
    Math::Vector3 m_hidePatrolTarget = { 0, 0, 0 };
    RoomArea* m_huntSearchRoom = nullptr;  // 見失った後に捜索で向かっている部屋

    // --- ドア開閉(HouseWander/Huntの移動中に近づいた閉まっているドアを自動的に開ける) ---
    float m_doorCheckTimer = 0.0f;

    // --- 家具の回避 ---
    // ゴーストの体の半径(家具の箱をこの分だけ太らせて判定する, m)。実際の当たり判定(カプセル)の半径+余裕で上書きする。
    // カプセルより小さいと、避けたつもりでもカプセルが家具の角にかかり、物理の押し戻しで家具の上へ押し上げられる
    float m_propAvoidRadius = 0.35f;
    float m_propLookAhead   = 0.8f;       // 進路上この距離以内に家具があれば回り込み始める(m)
    // これより低い家具(本・新聞など)は障害物にしない(m)。布団(0.24m)は当たり判定があり、上を通ろうとすると
    // 押し上げられて進めなくなるので避ける
    float m_propMinObstacleHeight = 0.2f;
    std::vector<DirectX::BoundingBox> m_propBoxes;
    bool  m_propBoxesBuilt = false;
    float m_propBoxRetryTimer = 0.0f;
    int   m_avoidPropIndex = -1;          // 今回り込んでいる家具(左右を毎フレーム切り替えてガタつかないよう覚えておく)
    float m_avoidPropSide = 0.0f;         // 回り込む向き(+1/-1)
    float m_avoidHoldTimer = 0.0f;        // 家具を避け終わってもしばらく回り込む向きを保つ(ぐるぐる回る時に左右が入れ替わらないように)
    float m_avoidCircleDir = 1.0f;        // 家具のまわりを回る向き(+1=反時計回り, -1=時計回り)。進む向きに対する左右ではなくこちらで覚える
    Math::Vector3 m_avoidGoal = { 0, 0, 0 }; // 最終的な行き先(回り込む左右を、行き先に近い回り方で選ぶのに使う)
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
    // 階段の昇り降りは「入口 -> 階段の端 -> 反対側の端 -> 出口」の折れ線に沿って進む
    // (以前は今いる場所から出口まで斜め一直線だったので、階段の横から入ると天井や壁をすり抜けていた)
    std::vector<Math::Vector3> m_stairsCrossPoints;
    float m_stairsCrossSpeed = 1.0f;
    float m_stairsCrossElapsed = 0.0f;
    float m_stairsCrossDuration = 1.0f;
    float m_stairsEntryOffset = 0.6f;  // 入口(階段の端からこれだけ手前)の位置
    float m_stairsEntryReach = 1.2f;   // 入口にこの水平距離まで近づいたら昇り降りを始める
    float m_doorCheckInterval = 0.25f;     // ドアの近接判定間隔(秒) - 毎フレーム全エンティティを走査しないよう間引く
    float m_doorOpenRange = 2.0f;          // この距離以内の閉まっているドアは自動的に開ける
    float m_doorSoundVolume = 0.9f;        // ゴーストがドアを開けた時の音量(プレイヤーとの距離でさらに減衰する)

    // --- 音(吐息・ハント) ---
    // 吐息: 姿が見えない間も時々近くで聞こえる(3D: 距離で減衰・左右にパン)。ハント中は同じ音をループで鳴らし続ける
    float m_breathVolume          = 0.7f;
    float m_breathRange           = 7.0f;   // この距離(m)以上離れていると聞こえない
    float m_breathIntervalMin     = 40.0f;  // 通常時の吐息の間隔(秒)。鳴り終わってから数える
    float m_breathIntervalMax     = 90.0f;
    float m_breathChance          = 0.5f;   // 間隔が来た時に吐息が鳴る確率(0〜1)。外れたら鳴らさずにもう一度間隔を待つ
    float m_huntBreathVolume      = 0.9f;
    float m_huntBreathRange       = 12.0f;
    float m_breathTimer           = 5.0f;
    // 別の階にいるPlayerには吐息を聞こえなくする(距離だけで減衰すると、真上/真下の階から聞こえてしまう)
    float m_breathOtherFloorRatio = 0.0f;   // 別の階にいる時の吐息の音量の割合
    float m_breathFloorFade       = 1.0f;   // 階の違いによる音量(階段の昇り降りで急に変わらないよう滑らかにする)
    // ハント中のループ(2D、ゴーストが近いほど大きくなる)
    float m_huntLoopVolume        = 0.6f;
    float m_huntLoopMinRatio      = 0.35f;  // 遠く(Hunt Loop Near Range以上)にいる時のループ音量の割合
    float m_huntLoopNearRange     = 15.0f;  // この距離から近づくほどループ音量が上がる(m)
    float m_huntLoopFadeTime      = 1.5f;   // ハント開始・終了時のフェード時間(秒)
    float m_huntLoopFade          = 0.0f;   // 0~1
    std::unique_ptr<DirectX::SoundEffectInstance> m_huntLoopSound;
    std::unique_ptr<DirectX::SoundEffectInstance> m_breathSound;     // 通常時の吐息(1回ずつ)
    std::unique_ptr<DirectX::SoundEffectInstance> m_huntBreathSound; // ハント中の吐息(ループ)

    // Ghost.gltfのアニメーションは 0:Ghost_Death, 1:Ghost_Move の2つしかない。
    // 専用のIdle/Huntモーションがないので、とりあえずMoveを使う
    // (Huntは既存コードでSpeed=1.5倍にして走っているように見せている)。
    int m_animIdle = 1;
    int m_animWander = 1;
    int m_animHunt = 1;
    int m_animDead = 0;
    int m_animCrouch = 1;                  // 押入れを覗き込む時のしゃがみ(ループ)

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
