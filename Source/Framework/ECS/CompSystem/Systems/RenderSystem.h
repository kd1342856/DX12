#pragma once
#include "../../../../Graphics/Shader/ShaderManager.h"

// RenderSystem: Draws entities with Transform + ModelRenderData components
class RenderSystem : public SystemBase
{
public:
	Math::Vector3 GetLightDirection() const { return m_lightDirection; }
	void SetLightDirection(const Math::Vector3& dir) { m_lightDirection = dir; }

	Entity GetCameraEntity() const { return m_cameraEntity; }
	void SetCameraEntity(Entity cameraEntity) { m_cameraEntity = cameraEntity; }

	void Update() override
	{
		Update(m_cameraEntity, nullptr);
	}

	void Update(Entity cameraEntity, class RenderTarget* pRT = nullptr)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;
		m_cameraEntity = cameraEntity;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();

		// Normalize light direction
		Math::Vector3 lightDir = m_lightDirection;
		lightDir.Normalize();

		// Initialize light data (always set regardless of shadow pass)
		CBufferData::Light cbLight = {};
		cbLight.AmbientLight = Math::Vector3(0.35f, 0.35f, 0.35f); // Dark ambient for horror
		cbLight.DL_Dir = lightDir;
		cbLight.DL_Color = Math::Vector3(0.6f, 0.6f, 0.65f); // Dark directional (moonlight)
		cbLight.SL_Count = 0;
        
		for (size_t i = 0; i < m_spotLights.size() && cbLight.SL_Count < 10; ++i) {
			cbLight.SL[cbLight.SL_Count] = m_spotLights[i];
			cbLight.SL_Count++;
		}

		// =============================================
		// Pass 1: Shadow Pass
		// =============================================
		auto* pShadowMap = pGraphicsDevice->GetShadowMap();
		if (pShadowMap && ShaderManager::Instance().m_shadowShader.IsCreated())
		{
			auto desc = pShadowMap->GetBuffer()->GetDesc();

			D3D12_VIEWPORT shadowViewport = {};

			shadowViewport.Width = (float)desc.Width;
			shadowViewport.Height = (float)desc.Height;

			shadowViewport.MinDepth = 0.0f;
			shadowViewport.MaxDepth = 1.0f;

			D3D12_RECT shadowScissor = { 0, 0, (LONG)desc.Width, (LONG)desc.Height };

			pCmdList->RSSetViewports(1, &shadowViewport);
			pCmdList->RSSetScissorRects(1, &shadowScissor);

			pShadowMap->TransitionTo(pCmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pShadowMap->ClearBuffer();
			auto dsvH = pGraphicsDevice->GetDSVHeap()->GetCPUHandle(pShadowMap->GetDSVNumber());
			pCmdList->OMSetRenderTargets(0, nullptr, false, &dsvH);

			Math::Vector3 lightPos = Math::Vector3(0, 0, 0) - lightDir * 50.0f;
			Math::Matrix mLightView = Math::Matrix::CreateLookAt(lightPos, Math::Vector3(0, 0, 0), Math::Vector3::Up);
			Math::Matrix mLightProj = Math::Matrix::CreateOrthographic(100.0f, 100.0f, 0.1f, 100.0f);
			Math::Matrix mLightVP = mLightView * mLightProj;

			CBufferData::Camera cbLightCam = {};
			cbLightCam.mView = mLightView;
			cbLightCam.mProj = mLightProj;
			cbLightCam.mVP = mLightVP;

			bool isSkinningShadowBegun = false;
			bool isLitShadowBegun = false;

			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
				{
					if (cModel.m_modelType == ModelType::Dynamic)
					{
						if (!isSkinningShadowBegun)
						{
							ShaderManager::Instance().m_skinningShader.BeginShadow();
							GDF::Instance().BindCBuffer(0, cbLightCam);
							isSkinningShadowBegun = true;
							isLitShadowBegun = false;
						}
						ShaderManager::Instance().m_skinningShader.DrawShadowModel(
							*cModel.m_spModelData, cTransform.m_worldMatrix, cModel.m_spModelData->GetBoneMatrices());
					}
					else
					{
						if (!isLitShadowBegun)
						{
							ShaderManager::Instance().m_shadowShader.Begin();
							GDF::Instance().BindCBuffer(0, cbLightCam);
							isLitShadowBegun = true;
							isSkinningShadowBegun = false;
						}
						ShaderManager::Instance().m_shadowShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
					}
				}
			}

			// Add shadow-specific light data
			cbLight.DL_ShadowPower = 2.5f;
			cbLight.DL_ShadowBias = 0.005f;
			cbLight.DL_mLightVP[0] = mLightVP;

			pShadowMap->TransitionTo(pCmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// =============================================
		// Pass 1-B: Spot Light Shadow Pass (フラッシュライトのみ)
		// =============================================
		auto* pSpotShadowMap = pGraphicsDevice->GetSpotShadowMap();
		if (pSpotShadowMap && cbLight.SL_Count > 0 && ShaderManager::Instance().m_shadowShader.IsCreated())
		{
			auto sdesc = pSpotShadowMap->GetBuffer()->GetDesc();

			D3D12_VIEWPORT spotViewport = {};
			spotViewport.Width = (float)sdesc.Width;
			spotViewport.Height = (float)sdesc.Height;
			spotViewport.MinDepth = 0.0f;
			spotViewport.MaxDepth = 1.0f;
			D3D12_RECT spotScissor = { 0, 0, (LONG)sdesc.Width, (LONG)sdesc.Height };
			pCmdList->RSSetViewports(1, &spotViewport);
			pCmdList->RSSetScissorRects(1, &spotScissor);

			pSpotShadowMap->TransitionTo(pCmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pSpotShadowMap->ClearBuffer();
			auto spotDsvH = pGraphicsDevice->GetDSVHeap()->GetCPUHandle(pSpotShadowMap->GetDSVNumber());
			pCmdList->OMSetRenderTargets(0, nullptr, false, &spotDsvH);

			// フラッシュライトの視点でView/Proj行列を作成
			auto& sl = cbLight.SL[0];
			Math::Vector3 upVec = Math::Vector3::Up;
			if (fabsf(sl.Dir.Dot(upVec)) > 0.99f) upVec = Math::Vector3::Right;

			float outerAngle = acosf(sl.OuterCorn);
			float fov = std::min(outerAngle * 2.0f + DirectX::XMConvertToRadians(4.0f), DirectX::XMConvertToRadians(170.0f));

			Math::Matrix mSpotView = Math::Matrix::CreateLookAt(sl.Pos, sl.Pos + sl.Dir, upVec);
			Math::Matrix mSpotProj = Math::Matrix::CreatePerspectiveFieldOfView(fov, 1.0f, 0.1f, sl.Range);
			Math::Matrix mSpotVP = mSpotView * mSpotProj;

			CBufferData::Camera cbSpotCam = {};
			cbSpotCam.mView = mSpotView;
			cbSpotCam.mProj = mSpotProj;
			cbSpotCam.mVP = mSpotVP;

			bool isSkinningSpotBegun = false;
			bool isLitSpotBegun = false;

			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
				{
					if (cModel.m_modelType == ModelType::Dynamic)
					{
						if (!isSkinningSpotBegun)
						{
							ShaderManager::Instance().m_skinningShader.BeginShadow();
							GDF::Instance().BindCBuffer(0, cbSpotCam);
							isSkinningSpotBegun = true;
							isLitSpotBegun = false;
						}
						ShaderManager::Instance().m_skinningShader.DrawShadowModel(
							*cModel.m_spModelData, cTransform.m_worldMatrix, cModel.m_spModelData->GetBoneMatrices());
					}
					else
					{
						if (!isLitSpotBegun)
						{
							ShaderManager::Instance().m_shadowShader.Begin();
							GDF::Instance().BindCBuffer(0, cbSpotCam);
							isLitSpotBegun = true;
							isSkinningSpotBegun = false;
						}
						ShaderManager::Instance().m_shadowShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
					}
				}
			}

			// 影用パラメータをSpotLightに書き戻す
			cbLight.SL[0].EnableShadow = 1.0f;
			cbLight.SL[0].ShadowBias = 0.0015f;
			cbLight.SL[0].mLightVP = mSpotVP;

			pSpotShadowMap->TransitionTo(pCmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// Upload light data to ShaderManager
		ShaderManager::Instance().SetLightData(cbLight);

		// =============================================
		// Pass 2: Set render target (always execute, independent of shadow pass)
		// =============================================
		if (pRT)
		{
			pGraphicsDevice->SetRenderTarget(pRT);
		}
		else
		{
			pGraphicsDevice->SetBackBuffer();
		}

		D3D12_VIEWPORT viewport = {};
		viewport.Width = 1280.0f;
		viewport.Height = 720.0f;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		D3D12_RECT scissorRect = { 0, 0, 1280, 720 };
		pCmdList->RSSetViewports(1, &viewport);
		pCmdList->RSSetScissorRects(1, &scissorRect);

		// =============================================
		// Pass 3: Main color pass (Lit / Skinning)
		// =============================================
		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		ShaderManager::Instance().SetCameraMatrix(cCamera.m_viewMatrix, cCamera.m_projMatrix);

		bool isLitBegun = false;
		bool isSkinningBegun = false;

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);

			if (cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
			{
				if (cModel.m_modelType == ModelType::Dynamic)
				{
					if (!isSkinningBegun)
					{
						ShaderManager::Instance().m_skinningShader.Begin();
						ShaderManager::Instance().BindCameraMatrix(0);
						ShaderManager::Instance().BindLightData(1);
						isSkinningBegun = true;
						isLitBegun = false;
					}
					ShaderManager::Instance().m_skinningShader.DrawModel(
						*cModel.m_spModelData, cTransform.m_worldMatrix, cModel.m_spModelData->GetBoneMatrices());
				}
				else
				{
					if (!isLitBegun)
					{
						ShaderManager::Instance().m_litShader.Begin();
						ShaderManager::Instance().BindCameraMatrix(0);
						ShaderManager::Instance().BindLightData(1);
						isLitBegun = true;
						isSkinningBegun = false;
					}
					ShaderManager::Instance().m_litShader.DrawModel(*cModel.m_spModelData, cTransform.m_worldMatrix);
				}
			}
		}
	}

public:
	void AddSpotLight(const CBufferData::SpotLight& sl) {
		m_spotLights.push_back(sl);
	}

	// フレームの最後にスポットライトをクリアする（Render完了後に呼ぶ）
	void ClearSpotLights() {
		m_spotLights.clear();
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
	std::vector<CBufferData::SpotLight> m_spotLights;
};