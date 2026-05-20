#pragma once
#include "../../../../Graphics/Shader/ShaderManager.h"

// =============================================
// RenderSystem
// Transform + Model + Shader を持つEntityを描画
// =============================================
class RenderSystem : public SystemBase
{
public:
	// カメラEntity設宁E
	void SetCameraEntity(Entity cameraEntity)
	{
		m_cameraEntity = cameraEntity;
	}

	// 描画更新
	void Update() override
	{
		if (!m_pCoordinator) return;

		// カメラの行�EをShaderManagerにセチE��
		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(m_cameraEntity);
		ShaderManager::Instance().SetCameraMatrix(cCamera.m_viewMatrix, cCamera.m_projMatrix);

		// 描画対象EntityルーチE
		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);

			// StandardShaderで描画
			if (cModel.m_spModelData) {
				ShaderManager::Instance().m_litShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
			}
		}
	}

private:
	Entity m_cameraEntity = 0;
};