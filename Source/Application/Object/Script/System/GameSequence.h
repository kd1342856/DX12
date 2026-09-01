#pragma once
#include "RoomArea.h"
#include <vector>

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
    void NotifyGameOver();

    const std::vector<RoomArea*>& GetRooms() const { return m_rooms; }

    static GameSequence* GetInstance() { return s_instance; }

    // �V�[���؂�ւ����ɌĂԁBScene::~Scene()��GameObject��OnDestroy()���Ă΂Ȃ�
    // (���g��shared_ptr<GameObject>�������������邾��)���߁As_instance����u�����
    // �j���ς݂̃V�[����GameSequence���w���_���O�����O�|�C���^���c���Ă��܂�
    // (CollisionManager�̐ÓI�I�N�g�c���[�Ɠ�����ނ̕s��̌���������)�B
    static void ResetInstance() { s_instance = nullptr; }

private:
    static GameSequence* s_instance;
    State m_currentState = State::Playing;
    float m_clearTimer = 0.0f;
    std::vector<RoomArea*> m_rooms;

    // Total time spent in State::Playing, passed to ResultScene as the "time taken" stat.
    float m_playTimer = 0.0f;
    // How long to hold on the in-scene "GAME CLEAR"/"GAME OVER" overlay before cutting to
    // ResultScene, and whether that cut has already been requested (guards against calling
    // SceneManager::ChangeScene more than once).
    static constexpr float kResultSceneDelay = 1.5f;
    bool m_resultSceneRequested = false;
    void UpdateResultSceneTransition(float deltaTime);
};
