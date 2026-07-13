#include "../../../../Pch.h"
#include "GhostAI.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"
#include "../../../../Framework/Object/GameObject.h"

REGISTER_COMPONENT(GhostAI);

void GhostAI::Awake()
{
}

void GhostAI::Start()
{
    m_changeDirTimer = 0.0f;
    SetState(GhostAI::State::Wander);
}

void GhostAI::SetState(GhostAI::State state)
{
    m_currentState = state;
    switch (state) {
        case GhostAI::State::Idle:
            AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animIdle, true);
            break;
        case GhostAI::State::Wander:
            AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animWander, true);
            break;
        case GhostAI::State::Hunt:
            AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animHunt, true, 1.5f);
            break;
        case GhostAI::State::Dead:
            AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animDead, false);
            break;
    }
}

void GhostAI::Update(float deltaTime)
{
    if (m_isExorcised) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    if (m_currentState == GhostAI::State::Wander) {
        m_changeDirTimer -= GameTimer::Instance().DeltaTime();
        if (m_changeDirTimer <= 0.0f) {
            float randX = Random::Instance().Range(-1.0f, 1.0f);
            float randZ = Random::Instance().Range(-1.0f, 1.0f);
            m_moveDir = Math::Vector3(randX, 0.0f, randZ);
            if (m_moveDir.LengthSquared() > 0.0f) 
            {
                m_moveDir.Normalize();
            }
            m_changeDirTimer = Random::Instance().Range(1.0f, 3.0f);
        }

        cTrans.m_position += m_moveDir * m_moveSpeed * GameTimer::Instance().DeltaTime();

        if (m_moveDir.LengthSquared() > 0.001f) {
            float angle = atan2f(m_moveDir.x, m_moveDir.z);
            cTrans.m_rotation.y = angle;
        }
    }
}

void GhostAI::PostUpdate()
{
}

void GhostAI::PreDraw()
{
}

void GhostAI::Draw()
{
}

void GhostAI::Serialize(nlohmann::json& out) const
{
    out["moveSpeed"] = m_moveSpeed;
    out["animIdle"] = m_animIdle;
    out["animWander"] = m_animWander;
    out["animHunt"] = m_animHunt;
    out["animDead"] = m_animDead;
}

void GhostAI::Deserialize(const nlohmann::json& in)
{
    if (in.contains("moveSpeed")) m_moveSpeed = in["moveSpeed"];
    if (in.contains("animIdle")) m_animIdle = in["animIdle"];
    if (in.contains("animWander")) m_animWander = in["animWander"];
    if (in.contains("animHunt")) m_animHunt = in["animHunt"];
    if (in.contains("animDead")) m_animDead = in["animDead"];
}

void GhostAI::ImGuiUpdate()
{
    ImGui::DragFloat("Move Speed", &m_moveSpeed, 0.1f, 0.1f, 10.0f);
}

void GhostAI::OnCollisionEnter(GameObject* other)
{
}

void GhostAI::OnCollisionStay(GameObject* other)
{
}

void GhostAI::Exorcise()
{
    if (m_isExorcised) return;

    m_isExorcised = true;
    SetState(GhostAI::State::Dead);
    // TODO: Delete object after death animation finishes
}
