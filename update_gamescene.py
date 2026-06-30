import os

def update_game_scene():
    path = r'c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    include_target = '#include "../../../Framework/ECS/Components/Data/ShaderData.h"\n'
    if '#include "../../../Framework/Manager/NavMeshManager.h"' not in content:
        content = content.replace(include_target, include_target + '#include "../../../Framework/Manager/NavMeshManager.h"\n')

    target = '    JobSystem::Instance().Wait();\n'
    rep = '''    JobSystem::Instance().Wait();

    // NavMeshの構築
    auto& ecs = GameManager::Instance().GetECS();
    for (Entity e = 0; e < 4096; ++e) {
        if (ecs.IsAlive(e) && ecs.HasComponent<ModelRenderData>(e)) {
            auto obj = m_spScene->GetGameObject(e);
            if (obj && obj->GetName() == "Stage") {
                auto& modelData = ecs.GetComponent<ModelRenderData>(e);
                auto& transData = ecs.GetComponent<TransformData>(e);
                auto worldMatrix = Math::Matrix::CreateScale(transData.m_scale) *
                                   Math::Matrix::CreateRotationX(transData.m_rotation.x) *
                                   Math::Matrix::CreateRotationY(transData.m_rotation.y) *
                                   Math::Matrix::CreateRotationZ(transData.m_rotation.z) *
                                   Math::Matrix::CreateTranslation(transData.m_position);
                NavMeshManager::Instance().BuildNavMesh(modelData.m_spModelData, worldMatrix);
                break;
            }
        }
    }
'''
    if 'NavMeshManager::Instance().BuildNavMesh' not in content:
        content = content.replace(target, rep)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    update_game_scene()
