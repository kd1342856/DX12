#pragma once
#include "../../../Object/GameObject.h"

class TransformSystem : public SystemBase {
public:
    void Update(float deltaTime) override {}
public:
    void Update(const std::vector<std::shared_ptr<GameObject>>& gameObjects)
    {
        for (auto& obj : gameObjects) {
            UpdateRecursive(obj, Math::Matrix::Identity);
        }
    }

private:
    void UpdateRecursive(const std::shared_ptr<GameObject>& obj, const Math::Matrix& parentWorld)
    {
        if (!obj->IsActive()) return;

        Math::Matrix currentWorld = parentWorld;
        auto entity = obj->GetEntityID();
        
        if (m_pCoordinator->IsAlive(entity))
        {
            if (auto* pTrans = m_pCoordinator->TryGetComponent<TransformData>(entity))
            {
                auto& trans = *pTrans;
            
            Math::Matrix scale = Math::Matrix::CreateScale(trans.m_scale);
            Math::Matrix rot = Math::Matrix::CreateFromYawPitchRoll(
                trans.m_rotation.y,
                trans.m_rotation.x,
                trans.m_rotation.z
            );
            Math::Matrix transMat = Math::Matrix::CreateTranslation(trans.m_position);
            
            trans.m_worldMatrix = scale * rot * transMat * parentWorld;
            currentWorld = trans.m_worldMatrix;
            }
        }

        for (const auto& child : obj->GetChildren()) {
            UpdateRecursive(child, currentWorld);
        }
    }
};