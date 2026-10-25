#pragma once

class CameraSystem : public SystemBase
{
public:
    void Update(float deltaTime) override
    {
        for (auto const& entity : m_entities)
        {
            auto& trans = m_pCoordinator->GetComponent<TransformData>(entity);
            auto& camera = m_pCoordinator->GetComponent<CameraData>(entity);

            // View Matrix (Inverse of World Matrix)
            camera.m_viewMatrix = trans.m_worldMatrix.Invert();

            // Projection Matrix
            // アスペクトは描画解像度から求める(描画解像度はウィンドウ解像度×レンダースケールなので比は同じ)
            const auto& device = GraphicsDevice::Instance();
            const float aspect = (device.GetBackBufferHeight() > 0)
                ? static_cast<float>(device.GetBackBufferWidth()) / static_cast<float>(device.GetBackBufferHeight())
                : 16.0f / 9.0f;
            camera.m_projMatrix = DirectX::XMMatrixPerspectiveFovLH(
                DirectX::XMConvertToRadians(camera.m_fov),
                aspect,
                camera.m_nearZ,
                camera.m_farZ
            );
        }
    }
};