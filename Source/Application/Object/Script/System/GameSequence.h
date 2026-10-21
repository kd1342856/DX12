#pragma once
#include "RoomArea.h"
#include <vector>

class GhostAI;

class GameSequence : public NativeScript {
public:
    enum class State {
        Playing,
        GameClear,
        GameOver
    };

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

    void NotifyExorcised();
    // catcherはPlayerを捕まえたゴースト。捕まった後はカメラがこのゴーストの顔に固定される
    void NotifyGameOver(GhostAI* catcher = nullptr);
    // ゲームオーバーより前に捕まったことだけを通知する(押入れの襖を開けられた瞬間など)。
    // カメラの固定とPlayerの操作停止はここから始まり、ゲームオーバー自体は後でNotifyGameOverで通知する
    void NotifyCaught(GhostAI* catcher) { if (m_currentState == State::Playing && !m_catcher) m_catcher = catcher; }
    bool IsGameOver() const { return m_currentState == State::GameOver; }
    // Playerが捕まっているか(ゲームオーバー中、または襖を開けられてゲームオーバー待ちの間)
    bool IsPlayerCaught() const { return IsGameOver() || m_catcher != nullptr; }
    GhostAI* GetCatcher() const { return m_catcher; }

    // 無敵(発表用デバッグモード): ゴーストに触れても、押入れで見つかってもゲームオーバーにならない
    void SetPlayerInvincible(bool invincible) { m_playerInvincible = invincible; }
    bool IsPlayerInvincible() const { return m_playerInvincible; }
    // GhostAIから、ハントが時間切れで終わった(=プレイヤーが生き延びた)時に呼ばれる。リザルトで表示する。
    void NotifyHuntSurvived() { if (m_currentState == State::Playing) ++m_huntsSurvived; }

    const std::vector<RoomArea*>& GetRooms() const { return m_rooms; }

    static GameSequence* GetInstance() { return s_instance; }

    // シーン切り替え時に呼ぶ。Scene::~Scene()はGameObjectのOnDestroy()を呼ばない
    // (中身のshared_ptr<GameObject>を解放するだけ)ため、s_instanceをそのままにすると
    // 破棄済みのシーンのGameSequenceを指すダングリングポインタが残ってしまう
    // (CollisionManagerの静的オクトツリーと同じ種類の不具合の原因になる)。
    static void ResetInstance() { s_instance = nullptr; }

private:
    static GameSequence* s_instance;
    State m_currentState = State::Playing;
    GhostAI* m_catcher = nullptr;
    bool m_playerInvincible = false;
    float m_clearTimer = 0.0f;
    std::vector<RoomArea*> m_rooms;

    // Total time spent in State::Playing, passed to ResultScene as the "time taken" stat.
    float m_playTimer = 0.0f;
    // ハントを生き延びた回数(リザルトの表示と評価ランクに使う)
    int m_huntsSurvived = 0;
    // How long to hold on the in-scene "GAME CLEAR"/"GAME OVER" overlay before cutting to
    // ResultScene, and whether that cut has already been requested (guards against calling
    // SceneManager::ChangeScene more than once).
    static constexpr float kResultSceneDelay = 1.5f;
    bool m_resultSceneRequested = false;
    void UpdateResultSceneTransition(float deltaTime);
};
