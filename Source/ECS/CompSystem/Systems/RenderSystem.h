#pragma once

// =============================================
// RenderSystem
// Transform + Model + Shader を持つEntityを描画
// GDFファサード経由でGraphicsDeviceにアクセス
// =============================================
class RenderSystem : public SystemBase
{
public:
	// 描画幅・高さの設定
	void SetScreenSize(int width, int height)
	{
		m_screenWidth = width;
		m_screenHeight = height;
	}

	// カメラEntityを設定
	void SetCameraEntity(Entity cameraEntity)
	{
		m_cameraEntity = cameraEntity;
	}

	// 描画更新
	void Update() override
	{
		if (!m_pCoordinator) return;

		// カメラ情報の取得とバインド
		auto& cCamera = m_pCoordinator->GetComponent<CameraComponent>(m_cameraEntity);
		CBufferData::Camera cbCamera;
		cbCamera.mView = cCamera.m_viewMatrix;
		cbCamera.mProj = cCamera.m_projMatrix;

		// 描画対象Entityをループ
		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformComponent>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelComponent>(entity);
			auto& cShader = m_pCoordinator->GetComponent<ShaderComponent>(entity);

			// シェーダー描画開始
			cShader.m_spShader->Begin(m_screenWidth, m_screenHeight);

			// カメラ定数バッファバインド
			GDF::Instance().BindCBuffer(0, cbCamera);

			// ワールド行列バインド
			GDF::Instance().BindCBuffer(1, cTransform.m_worldMatrix);

			// モデル描画
			cShader.m_spShader->DrawModel(*cModel.m_spModelData);
		}
	}

private:
	int m_screenWidth = 1280;
	int m_screenHeight = 720;
	Entity m_cameraEntity = 0;
};
