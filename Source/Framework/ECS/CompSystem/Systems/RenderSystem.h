#pragma once
#include "../../../../Graphics/Shader/ShaderManager.h"

// =============================================
// RenderSystem
// Transform + Model + Shader 繧呈戟縺､Entity繧呈緒逕ｻ
// =============================================
class RenderSystem : public SystemBase
{
public:
	// 繧ｫ繝｡繝ｩEntity險ｭ螳・
	void SetCameraEntity(Entity cameraEntity)
	{
		m_cameraEntity = cameraEntity;
	}

	// 謠冗判譖ｴ譁ｰ
		// 描画更新（デフォルトカメラを使用）
	void Update() override
	{
		Update(m_cameraEntity);
	}

	// 描画更新（指定したカメラを使用）
	void Update(Entity cameraEntity)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;

		// カメラの行列をShaderManagerにセット
		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		ShaderManager::Instance().SetCameraMatrix(cCamera.m_viewMatrix, cCamera.m_projMatrix);

		// 描画対象Entityループ
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
	Entity m_cameraEntity = INVALID_ENTITY;
};