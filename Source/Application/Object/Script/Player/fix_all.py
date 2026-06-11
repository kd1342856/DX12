import codecs
import re

path = r'C:\GitHub\DX12\Source\Framework\Manager\CollisionManager.cpp'
with codecs.open(path, 'r', encoding='shift_jis') as f:
    text = f.read()

# Fix the incorrect replacement in DrawDebugWires
bad_draw_loop = '''for (const auto& shape : colData.m_shapes) {
            if ((shape->m_tags & rayInfo.collisionMask) == 0) continue;'''
good_draw_loop = '''for (const auto& shape : colData.m_shapes) {'''
text = text.replace(bad_draw_loop, good_draw_loop)

# Fix Raycast hit logic properly
bad_ray_loop = '''for (const auto& shape : colData.m_shapes) {
            if ((shape->m_tags & rayInfo.collisionMask) == 0) continue;
            RayResult out;
            if (shape->RayCast(rayInfo, transData.m_worldMatrix, out)) {'''
# Actually the replacement in Raycast is correct! 
# Let's verify we didn't break Raycast itself. 
# It should still have the check.

with codecs.open(path, 'w', encoding='shift_jis') as f:
    f.write(text)

# Also fix Player.cpp correctly with the new collision manager raycast
path_p = r'C:\GitHub\DX12\Source\Application\Object\Script\Player\Player.cpp'
with codecs.open(path_p, 'r', encoding='shift_jis') as f:
    text_p = f.read()

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

    Math::Vector3 origin = cTrans.m_position + Math::Vector3(0, 0.5f, 0);
    Math::Vector3 dir(0, -1, 0);
    float maxDistance = 1000.0f;

    RaycastHit hit = CollisionManager::Instance().Raycast(origin, dir, maxDistance, CollisionTags::StageObject);

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

    uint32_t lineColor = m_isGrounded ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
    float drawDist = hit.hit ? std::min(hit.distance, 1.5f) : 1.5f;
    CollisionManager::Instance().AddDebugLine(origin, origin + dir * drawDist, lineColor);
}

void Player::PreDraw()'''

text_p = pattern.sub(new_code, text_p)

with codecs.open(path_p, 'w', encoding='shift_jis') as f:
    f.write(text_p)

print("Done Player and Collision")