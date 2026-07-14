#include "../../../../Pch.h"
#include "GameSequence.h"
#include "../../../../Framework/Object/GameObject.h"

REGISTER_COMPONENT(GameSequence);

GameSequence* GameSequence::s_instance = nullptr;

void GameSequence::Awake()
{
    s_instance = this;
}

void GameSequence::Start()
{
    m_currentState = State::Playing;
}

void GameSequence::Update(float deltaTime)
{
    if (m_currentState == State::GameClear) {
        m_clearTimer += deltaTime;
    }
}

void GameSequence::PostUpdate()
{
}

void GameSequence::PreDraw()
{
}

void GameSequence::Draw()
{
    // ƒNƒŠƒAŽž‚ÌUI•`‰æ (ImGui‚Å‰¼ŽÀ‘•)
    if (m_currentState == State::GameClear) {
        ImGui::SetNextWindowPos(ImVec2(1280.0f * 0.5f - 150.0f, 720.0f * 0.5f - 50.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Always);
        
        // ”wŒi‚ð“§–¾‚É‚µ‚ÄƒeƒLƒXƒg‚ð–Ú—§‚½‚¹‚é
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.5f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
        
        ImGui::Begin("Game Clear UI", nullptr, flags);
        ImGui::SetWindowFontScale(3.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "GAME CLEAR");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
        
        ImGui::PopStyleColor();
    }
}

void GameSequence::Serialize(nlohmann::json& out) const
{
}

void GameSequence::Deserialize(const nlohmann::json& in)
{
}

void GameSequence::ImGuiUpdate()
{
    ImGui::Text("Current State: %s", m_currentState == State::Playing ? "Playing" : "GameClear");
    if (ImGui::Button("Debug Clear")) {
        NotifyExorcised();
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
