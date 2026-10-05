#include "../../../../Pch.h"
#include "GameSequence.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../Ghost/GhostAI.h"
#include "../../../Scene/ResultScene/ResultScene.h"

REGISTER_COMPONENT(GameSequence);

GameSequence* GameSequence::s_instance = nullptr;

void GameSequence::Awake()
{
    s_instance = this;
}

void GameSequence::Start()
{
    m_currentState = State::Playing;
    m_playTimer = 0.0f;
    m_clearTimer = 0.0f;
    m_resultSceneRequested = false;
    m_rooms.clear();

    auto& ecs = GameManager::Instance().GetECS();
    GhostAI* ghost = nullptr;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) 
    {
        if (auto* room = dynamic_cast<RoomArea*>(scriptData.Instance.get())) 
        {
            m_rooms.push_back(room);
        }
        if (!ghost)
        {
            if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) 
            {
                ghost = g;
            }
        }
    }

    if (ghost) 
    {
        std::vector<RoomArea*> candidates;
        for (RoomArea* room : m_rooms) 
        {
            if (room->m_isGhostRoomCandidate) 
            {
                candidates.push_back(room);
            }
        }

        if (!candidates.empty()) {
            // rand() with no srand() call anywhere in the project always starts from the same
            // default seed, so this picked the exact same candidate every single run - Random
            // seeds itself from std::random_device once at process start, giving real variety.
            int r = Random::Instance().Range(0, (int)candidates.size() - 1);
            RoomArea* targetRoom = candidates[r];
            ghost->SetTargetRoom(targetRoom);
            
            // ゴーストを選ばれた部屋の中心に配置
            auto& gTrans = ecs.GetComponent<TransformData>(ghost->GetGameObject()->GetEntityID());
            gTrans.m_position = targetRoom->GetCenter();
            // ※ y = 0.0f の強制リセットは廃止。2階ルームが選ばれた場合もルームのY座標を使う

            // ログに現在指定されたルームを出す
            std::string roomName = targetRoom->GetGameObject()->GetName();
            Logger::Instance().AddLog(Logger::LogLevel::Info, "Ghost Room Selected: %s", roomName.c_str());
        } else {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Ghost Room Selection Failed: No Candidate Rooms found! Did you check 'Ghost Room Candidate'?");
        }
    } else {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "Ghost Room Selection Failed: GhostAI not found in scene!");
    }
}

void GameSequence::Update(float deltaTime)
{
    if (m_currentState == State::Playing) {
        m_playTimer += deltaTime;
    }
    if (m_currentState == State::GameClear || m_currentState == State::GameOver) {
        m_clearTimer += deltaTime;
    }
    UpdateResultSceneTransition(deltaTime);
}

void GameSequence::UpdateResultSceneTransition(float deltaTime)
{
    if (m_currentState == State::Playing || m_resultSceneRequested) return;

    // Let the brief in-scene "GAME CLEAR"/"GAME OVER" overlay (see Draw()) show for a moment
    // before cutting away, so the transition doesn't feel instantaneous/jarring.
    if (m_clearTimer < kResultSceneDelay) return;

    m_resultSceneRequested = true;
    bool isClear = (m_currentState == State::GameClear);
    // TODO: wire up a real ghost name/reward once that data exists on GhostAI; placeholders for now.
    SceneManager::Instance().ChangeScene(
        std::make_unique<ResultScene>(isClear, m_playTimer, "Onryo", isClear ? 1000 : 0),
        1.0f);
}

void GameSequence::PostUpdate()
{
}

void GameSequence::PreDraw()
{
}

void GameSequence::Draw()
{
    // ImGui側のクリア/オーバーテキストオーバーレイは廃止。
    // ResultSceneの画像(bg_clear.jpg/Death.png)で結果を描画するので、
    // その前にゲーム画面中央にデバッグ的な文字が出ている必要がない。
}

void GameSequence::Serialize(nlohmann::json& out) const
{
}

void GameSequence::Deserialize(const nlohmann::json& in)
{
}

void GameSequence::ImGuiUpdate()
{
    ImGui::Text("Current State: %s", m_currentState == State::Playing ? "Playing" : 
                                     (m_currentState == State::GameClear ? "GameClear" : "GameOver"));
    if (ImGui::Button("Debug Clear")) {
        NotifyExorcised();
    }
    ImGui::SameLine();
    if (ImGui::Button("Debug Over")) {
        NotifyGameOver();
    }
}

void GameSequence::OnCollisionEnter(GameObject* other)
{
}

void GameSequence::OnCollisionStay(GameObject* other)
{
}

void GameSequence::NotifyExorcised()
{
    if (m_currentState == State::Playing) {
        m_currentState = State::GameClear;
        m_clearTimer = 0.0f;
    }
}

void GameSequence::NotifyGameOver()
{
    if (m_currentState == State::Playing) {
        m_currentState = State::GameOver;
        m_clearTimer = 0.0f;
    }
}
