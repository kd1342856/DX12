import os

def update_ghost_ai_cpp():
    path = r'c:\GitHub\DX12\Source\Application\Script\Character\Ghost\GhostAI.cpp'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # Add includes
    include_str = '''#include "../../../../Framework/Manager/NavMeshManager.h"
#include "../../../../Framework/Manager/SceneManager.h"
#include "../../../../Framework/Manager/Scene.h"
#include "../../../Scene/TitleScene/TitleScene.h"
'''
    if 'NavMeshManager.h' not in content:
        content = content.replace('#include "Pch.h"\n', '#include "Pch.h"\n' + include_str)

    # Replace Update
    target_update = '''void GhostAI::Update()
{
    if (m_isExorcised) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    if (m_currentState == GhostAI::State::Wander) {
        m_changeDirTimer -= GameTimer::Instance().DeltaTime();
        if (m_changeDirTimer <= 0.0f) {
            float randX = Random::Instance().Range(-1.0f, 1.0f);
            float randZ = Random::Instance().Range(-1.0f, 1.0f);
            m_moveDir = DirectX::SimpleMath::Vector3(randX, 0.0f, randZ);
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
}'''

    new_update = '''void GhostAI::Update()
{
    if (m_isExorcised) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto& cTrans = ecs.GetComponent<TransformData>(GetGameObject()->GetEntityID());

    Math::Vector3 playerPos;
    bool playerFound = false;
    auto currentScene = SceneManager::Instance().GetCurrentScene();
    std::shared_ptr<Scene> scene = std::dynamic_pointer_cast<Scene>(currentScene);
    if (scene) {
        for (auto& obj : scene->GetGameObjects()) {
            if (obj->GetName() == "Player") {
                playerPos = ecs.GetComponent<TransformData>(obj->GetEntityID()).m_position;
                playerFound = true;
                break;
            }
        }
    }

    float distToPlayer = 9999.0f;
    if (playerFound) {
        distToPlayer = Math::Vector3::Distance(cTrans.m_position, playerPos);
    }

    // State Transitions
    if (m_currentState == GhostAI::State::Wander) {
        if (playerFound && distToPlayer <= m_detectionRadius) {
            SetState(GhostAI::State::Hunt);
            m_pathUpdateTimer = 0.0f;
        }
    } else if (m_currentState == GhostAI::State::Hunt) {
        if (!playerFound || distToPlayer > m_loseRadius) {
            SetState(GhostAI::State::Wander);
            m_currentPath.clear();
        }
    }

    // Movement
    if (m_currentState == GhostAI::State::Wander) {
        m_changeDirTimer -= GameTimer::Instance().DeltaTime();
        if (m_changeDirTimer <= 0.0f) {
            float randX = Random::Instance().Range(-1.0f, 1.0f);
            float randZ = Random::Instance().Range(-1.0f, 1.0f);
            m_moveDir = DirectX::SimpleMath::Vector3(randX, 0.0f, randZ);
            if (m_moveDir.LengthSquared() > 0.0f) m_moveDir.Normalize();
            m_changeDirTimer = Random::Instance().Range(1.0f, 3.0f);
        }

        cTrans.m_position += m_moveDir * m_moveSpeed * GameTimer::Instance().DeltaTime();

        if (m_moveDir.LengthSquared() > 0.001f) {
            cTrans.m_rotation.y = atan2f(m_moveDir.x, m_moveDir.z);
        }
    } 
    else if (m_currentState == GhostAI::State::Hunt) {
        m_pathUpdateTimer -= GameTimer::Instance().DeltaTime();
        if (m_pathUpdateTimer <= 0.0f) {
            NavMeshManager::Instance().FindPath(cTrans.m_position, playerPos, m_currentPath);
            m_pathUpdateTimer = 0.5f; // Update path every 0.5 seconds
        }

        if (!m_currentPath.empty()) {
            Math::Vector3 targetNode = m_currentPath[0];
            // Ignore Y for distance
            Math::Vector3 posXZ(cTrans.m_position.x, 0, cTrans.m_position.z);
            Math::Vector3 targetXZ(targetNode.x, 0, targetNode.z);
            
            if (Math::Vector3::Distance(posXZ, targetXZ) < 0.5f) {
                m_currentPath.erase(m_currentPath.begin());
            }

            if (!m_currentPath.empty()) {
                targetNode = m_currentPath[0];
                targetXZ = Math::Vector3(targetNode.x, 0, targetNode.z);
                Math::Vector3 moveDir = targetXZ - posXZ;
                if (moveDir.LengthSquared() > 0.0f) {
                    moveDir.Normalize();
                    cTrans.m_position += moveDir * m_huntSpeed * GameTimer::Instance().DeltaTime();
                    cTrans.m_rotation.y = atan2f(moveDir.x, moveDir.z);
                }
            }
        } else {
            // Direct line if path is empty (or NavMesh not ready)
            Math::Vector3 moveDir = playerPos - cTrans.m_position;
            moveDir.y = 0;
            if (moveDir.LengthSquared() > 0.0f) {
                moveDir.Normalize();
                cTrans.m_position += moveDir * m_huntSpeed * GameTimer::Instance().DeltaTime();
                cTrans.m_rotation.y = atan2f(moveDir.x, moveDir.z);
            }
        }
    }
}'''
    if 'NavMeshManager::Instance()' not in content:
        content = content.replace(target_update, new_update)

    # Add collision logic
    target_col = '''void GhostAI::OnCollisionEnter(GameObject* other)
{
}'''
    new_col = '''void GhostAI::OnCollisionEnter(GameObject* other)
{
    if (m_currentState == GhostAI::State::Dead) return;
    if (other && other->GetName() == "Player") {
        SceneManager::Instance().ChangeScene(std::make_shared<TitleScene>());
    }
}'''
    if 'ChangeScene' not in content:
        content = content.replace(target_col, new_col)

    # Add serialize
    target_ser = '''    out["animDead"] = m_animDead;
}'''
    new_ser = '''    out["animDead"] = m_animDead;
    out["detectionRadius"] = m_detectionRadius;
    out["loseRadius"] = m_loseRadius;
    out["huntSpeed"] = m_huntSpeed;
}'''
    if 'detectionRadius' not in content:
        content = content.replace(target_ser, new_ser)

    target_deser = '''    if (in.contains("animDead")) m_animDead = in["animDead"];
}'''
    new_deser = '''    if (in.contains("animDead")) m_animDead = in["animDead"];
    if (in.contains("detectionRadius")) m_detectionRadius = in["detectionRadius"];
    if (in.contains("loseRadius")) m_loseRadius = in["loseRadius"];
    if (in.contains("huntSpeed")) m_huntSpeed = in["huntSpeed"];
}'''
    if 'm_detectionRadius =' not in content:
        content = content.replace(target_deser, new_deser)

    # Imgui
    target_imgui = '''    ImGui::DragFloat("Move Speed", &m_moveSpeed, 0.1f, 0.1f, 10.0f);'''
    new_imgui = '''    ImGui::DragFloat("Wander Speed", &m_moveSpeed, 0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("Hunt Speed", &m_huntSpeed, 0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("Detection Radius", &m_detectionRadius, 0.1f, 1.0f, 50.0f);
    ImGui::DragFloat("Lose Radius", &m_loseRadius, 0.1f, 1.0f, 50.0f);'''
    if 'Wander Speed' not in content:
        content = content.replace(target_imgui, new_imgui)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    update_ghost_ai_cpp()
