#pragma once
#include "../../ComponentManager.h"
#include "../../ECSCoordinator.h"
#include "../System.h"
#include "../../Components/Data/CameraData.h"
#include "../../Components/Data/TransformData.h"

class CameraSystem : public SystemBase
{
public:
    void Update()
    {
        for (auto const& entity : m_entities)
        {
            auto& trans = m_pCoordinator->GetComponent<TransformData>(entity);
            auto& camera = m_pCoordinator->GetComponent<CameraData>(entity);

            // View Matrix (Inverse of World Matrix)
            camera.m_viewMatrix = trans.m_worldMatrix.Invert();

            // Projection Matrix
            camera.m_projMatrix = DirectX::XMMatrixPerspectiveFovLH(
                DirectX::XMConvertToRadians(camera.m_fov),
                1280.0f / 720.0f,
                camera.m_nearZ,
                camera.m_farZ
            );
        }
    }
};