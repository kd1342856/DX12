#pragma once
#include "../../../../Graphics/Shader/ShaderManager.h"

// =============================================
// RenderSystem
// Transform + Model + Shader 繧呈戟縺?E?Entity繧呈緒逕ｻ
// =============================================
class RenderSystem : public SystemBase
{
public:
	// 繧?E?繝｡繝ｩEntity險?E?螳・
	Entity GetCameraEntity() const { return m_cameraEntity; }
	void SetCameraEntity(Entity cameraEntity)
	{
		m_cameraEntity = cameraEntity;
	}

	// 謠冗判譖ｴ譁E??
	// 描画更新?E?デフォルトカメラを使用?E?E
	void Update() override
	{
		Update(m_cameraEntity, nullptr);
	}

	// 描画更新?E?指定したカメラを使用?E?E
	void Update(Entity cameraEntity, class RenderTarget* pRT = nullptr)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;
		m_cameraEntity = cameraEntity;

		// --- 1. シャドウパス (ShadowShader) ---
		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();
		auto* pShadowMap = pGraphicsDevice->GetShadowMap();

		if (pShadowMap && ShaderManager::Instance().m_shadowShader.IsCreated())
		{
			pShadowMap->ClearBuffer();
			auto dsvH = pGraphicsDevice->GetDSVHeap()->GetCPUHandle(pShadowMap->GetDSVNumber());
			pCmdList->OMSetRenderTargets(0, nullptr, false, &dsvH);

			// ライト?E行?E計?E
			Math::Vector3 lightDir = Math::Vector3(0.5f, -1.0f, 0.5f);
			lightDir.Normalize();
			Math::Vector3 lightPos = Math::Vector3(0, 0, 0) - lightDir * 50.0f;
			Math::Matrix mLightView = Math::Matrix::CreateLookAt(lightPos, Math::Vector3(0, 0, 0), Math::Vector3::Up);
			Math::Matrix mLightProj = Math::Matrix::CreateOrthographic(100.0f, 100.0f, 0.1f, 100.0f);
			Math::Matrix mLightVP = mLightView * mLightProj;

			// シャドウシェーダーのセチE??アチE?E?E?EitShader用とSkinningShader用?E?E
			bool isSkinningShadowBegun = false;
			bool isLitShadowBegun = false;

			CBufferData::Camera cbLightCam = {};
			cbLightCam.mView = mLightView;
			cbLightCam.mProj = mLightProj;
			cbLightCam.mVP = mLightVP;

			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_spModelData) {
					if (cModel.m_modelType == ModelType::Dynamic)
					{
						if (!isSkinningShadowBegun)
						{
							ShaderManager::Instance().m_skinningShader.BeginShadow();
							GDF::Instance().BindCBuffer(0, cbLightCam); // パイプライン?E??替えで再バインチE
							isSkinningShadowBegun = true;
							isLitShadowBegun = false;
						}
						ShaderManager::Instance().m_skinningShader.DrawShadowModel(*cModel.m_spModelData, cTransform.m_worldMatrix, cModel.m_spModelData->GetBoneMatrices());
					}
					else
					{
						if (!isLitShadowBegun)
						{
							ShaderManager::Instance().m_shadowShader.Begin();
							GDF::Instance().BindCBuffer(0, cbLightCam); // パイプライン?E??替えで再バインチE
							isLitShadowBegun = true;
							isSkinningShadowBegun = false;
						}
						ShaderManager::Instance().m_shadowShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
					}
				}
			}

			// ライト?E行?EをLitShaderに渡す準備 (メインパス用)
			CBufferData::Light cbLight = {};
			cbLight.AmbientLight = Math::Vector3(0.3f, 0.3f, 0.3f);
			cbLight.DL_Dir = lightDir;
			cbLight.DL_Color = Math::Vector3(1.0f, 1.0f, 1.0f);
			cbLight.SL_Count = 0;
			cbLight.DL_ShadowPower = 0.5f; // 影の濁E??
			cbLight.DL_ShadowBias = 0.005f;
			cbLight.DL_mLightVP[0] = mLightVP;
			
			// メインパス用にRenderTargetを?Eに戻ぁE
			if (pRT)
			{
				pGraphicsDevice->SetRenderTarget(pRT);
			}
			else
			{
				pGraphicsDevice->SetBackBuffer();
			}

			// cbLightのセチE??にはLitShaderのルートシグネチャが?E??なので先にBeginを呼ぶ
			ShaderManager::Instance().SetLightData(cbLight);
		}

		// --- 2. メインパス (LitShader) ---
		// カメラの行?EをShaderManagerにセチE??
		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		ShaderManager::Instance().SetCameraMatrix(cCamera.m_viewMatrix, cCamera.m_projMatrix);

		// 描画対象EntityルーチE
		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);

			if (cModel.m_spModelData) {
				if (cModel.m_modelType == ModelType::Dynamic) {
					ShaderManager::Instance().m_skinningShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix, cModel.m_spModelData->GetBoneMatrices());
				}
				else {
					ShaderManager::Instance().m_litShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
				}
			}
		}
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
};