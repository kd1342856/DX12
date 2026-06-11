import codecs

path = r'C:\GitHub\DX12\Source\Application\Object\Script\Player\Player.cpp'
with codecs.open(path, 'r', encoding='shift_jis') as f:
    text = f.read()

# Replace everything from Update to PostUpdate
import re
pattern = re.compile(r'void Player::Update\(\).*?void Player::PreDraw\(\)', re.DOTALL)

new_code = '''void Player::Update() {
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

    if (input.IsKeyTrigger(VK_SPACE) && m_isGrounded) {
        m_velocityY = 5.0f;
    }

    if (m_useGravity) {
        m_velocityY -= m_gravityStrength * 0.016f;
        cTrans.m_position.y += m_velocityY * 0.016f;
    }
}

void Player::PostUpdate() 
{
    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    // レイキャストの起点と方向
    Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0); // 足元から少し上
    Math::Vector3 dir(0, -1, 0); // 下方向
    float maxDistance = 1000.0f;

    // 衝突フィルタリングを使ってRaycast
    RaycastHit hit = CollisionManager::Instance().Raycast(origin, dir, maxDistance, CollisionTags::StageObject);

    // 0.6f以内でヒットしていれば接地とみなす
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

    // デバッグ用のレイを描画
    uint32_t lineColor = m_isGrounded ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
    float drawDist = hit.hit ? std::min(hit.distance, 1.5f) : 1.5f;
    CollisionManager::Instance().AddDebugLine(origin, origin + dir * drawDist, lineColor);
}

void Player::PreDraw()'''

text = pattern.sub(new_code, text)

with codecs.open(path, 'w', encoding='shift_jis') as f:
    f.write(text)

print("Done")