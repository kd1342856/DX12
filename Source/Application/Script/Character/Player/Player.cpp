#include "../../../../Pch.h"
#include "Player.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"

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
    cTrans.m_position += moveDir * m_moveSpeed * GameTimer::Instance().DeltaTime();

    if (input.IsKeyTrigger(VK_SPACE) && m_isGrounded) {
        m_velocityY = 5.0f;
    }

    if (m_useGravity) {
        m_velocityY -= m_gravityStrength * GameTimer::Instance().DeltaTime();
        cTrans.m_position.y += m_velocityY * GameTimer::Instance().DeltaTime();
    }

    // 重力処理後に接地判定を行う（描画・カメラとのガタつきを防ぐためUpdate内で処理）
    Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0);
    Math::Vector3 dir(0, -1, 0);
    RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(origin, dir, 1000.0f, "Stage");

    if (hit.hit && hit.distance <= 0.6f) {
        m_isGrounded = true;
        if (m_velocityY < 0.0f) {
            m_velocityY = 0.0f;
            cTrans.m_position.y = origin.y - hit.distance;
        }
    }
    else {
        m_isGrounded = false;
    }

    // デバッグ用ラインの描画（接地していれば緑、していなければ赤）
    uint32_t lineColor = m_isGrounded ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
    float drawDist = hit.hit ? std::min(hit.distance, 1.5f) : 1.5f;
    CollisionManager::Instance().AddDebugLine(origin, origin + dir * drawDist, lineColor);
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
}

void Player::OnCollisionStay(GameObject* other) {
}