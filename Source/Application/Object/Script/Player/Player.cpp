#include "Player.h"
#include "../../../../Framework/Manager/GameManager.h"
#include "../../../../Framework/DirectX/Utility/Input.h"
#include "../../../../Framework/ImGuiEditor/ImGui/imgui.h"
#include "../../../../Framework/Object/GameObject.h"

REGISTER_COMPONENT(Player);

void Player::Awake() 
{
}

void Player::Start() 
{
}

void Player::Update() {
    auto& input = Input::Instance();
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    Math::Matrix playerRot = Math::Matrix::CreateRotationY(cTrans.m_rotation.y);
    Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), playerRot);
    Math::Vector3 right   = Math::Vector3::TransformNormal(Math::Vector3(1, 0, 0), playerRot);

    Math::Vector3 moveDir(0, 0, 0);
    if (input.IsKeyHold('W')) moveDir += forward;
    if (input.IsKeyHold('S')) moveDir -= forward;
    if (input.IsKeyHold('A')) moveDir -= right;
    if (input.IsKeyHold('D')) moveDir += right;

    if (moveDir.LengthSquared() > 0.0f) {
        moveDir.Normalize();
    }
    cTrans.m_position += moveDir * m_moveSpeed * 0.016f;

    if (m_useGravity) {
        m_velocityY -= m_gravityStrength * 0.016f;
        cTrans.m_position.y += m_velocityY * 0.016f;
    }

    if (input.IsKeyTrigger(VK_SPACE) && m_isGrounded) {
        m_velocityY = 5.0f;
    }
    m_isGrounded = false;
}

void Player::PostUpdate() 
{
}

void Player::PreDraw() 
{
}

void Player::Draw() 
{
}

void Player::Serialize(nlohmann::json& out) const {
    out["moveSpeed"] = m_moveSpeed;
    out["useGravity"] = m_useGravity;
    out["gravityStrength"] = m_gravityStrength;
}

void Player::Deserialize(const nlohmann::json& in) {
    if (in.contains("moveSpeed")) m_moveSpeed = in["moveSpeed"];
    if (in.contains("useGravity")) m_useGravity = in["useGravity"];
    if (in.contains("gravityStrength")) m_gravityStrength = in["gravityStrength"];
}

void Player::ImGuiUpdate() {
    ImGui::DragFloat("Move Speed", &m_moveSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::Checkbox("Use Gravity", &m_useGravity);
    if (m_useGravity) {
        ImGui::DragFloat("Gravity Strength", &m_gravityStrength, 0.1f, 0.0f, 100.0f);
    }
}

void Player::OnCollisionEnter(GameObject* other) {
    if (m_velocityY <= 0.0f) {
        m_isGrounded = true;
        m_velocityY = 0.0f;
    }
}

void Player::OnCollisionStay(GameObject* other) {
    if (m_velocityY <= 0.0f) {
        m_isGrounded = true;
    }
}