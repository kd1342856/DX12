#pragma once
#include "../../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../../Graphics/Shader/LitShader/LitShader.h"
#include "../../../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../../../Graphics/Renderer/ModelRenderer.h"
#include "../../../../Graphics/Renderer/Renderer.h"

// RenderSystem: Draws entities with Transform + ModelRenderData components
class RenderSystem : public SystemBase
{
public:
	Math::Vector3 GetLightDirection() const { return m_lightDirection; }
	void SetLightDirection(const Math::Vector3& dir) { m_lightDirection = dir; }

	Entity GetCameraEntity() const { return m_cameraEntity; }
	void SetCameraEntity(Entity cameraEntity) { m_cameraEntity = cameraEntity; }

	void RenderShadow()
	{
		if (!m_pCoordinator) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();

		Math::Vector3 lightDir = m_lightDirection;
		lightDir.Normalize();

		CBufferData::Light cbLight = {};
		cbLight.AmbientLight = Math::Vector3(0.35f, 0.35f, 0.35f);
		cbLight.DL_Dir = lightDir;
		cbLight.DL_Color = Math::Vector3(0.6f, 0.6f, 0.65f);
		cbLight.SL_Count = 0;

		auto* pShadowMap = pGraphicsDevice->GetShadowMap();
		if (pShadowMap)
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

			Math::Vector3 camPos = Math::Vector3::Zero;
			if (m_cameraEntity != INVALID_ENTITY)
			{
				if (auto* pTrans = m_pCoordinator->TryGetComponent<TransformData>(m_cameraEntity))
				{
					camPos = pTrans->m_position;
				}
			}

			float texelSize = 100.0f / 4096.0f;
			Math::Vector3 snappedCamPos = camPos;
			snappedCamPos.x = std::floor(camPos.x / texelSize) * texelSize;
			snappedCamPos.y = std::floor(camPos.y / texelSize) * texelSize;
			snappedCamPos.z = std::floor(camPos.z / texelSize) * texelSize;

			Math::Vector3 lightPos = snappedCamPos - lightDir * 50.0f;
			Math::Matrix mLightView = Math::Matrix::CreateLookAt(lightPos, snappedCamPos, Math::Vector3::Up);
			Math::Matrix mLightProj = Math::Matrix::CreateOrthographic(100.0f, 100.0f, 0.1f, 100.0f);
			Math::Matrix mLightVP = mLightView * mLightProj;

			RenderContext& context = Renderer::GetContext();
			// Save current cam
			Math::Matrix oldView = context.View;
			Math::Matrix oldProj = context.Projection;

			// Temporarily use light matrix for shadow map rendering
			context.View = mLightView;
			context.Projection = mLightProj;
			
			auto& shadowShader = ShaderLibrary::Instance().Get<ShadowShader>();
			auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();

			bool isSkinningShadowBegun = false;
			bool isLitShadowBegun = false;

			DrawContext drawContext;

			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
				{
					drawContext.SetWorld(cTransform.m_worldMatrix);

					if (cModel.m_modelType == ModelType::Dynamic)
					{
						auto bones = cModel.m_spModelData->GetBoneMatrices();
						drawContext.SetBones(&bones);
						if (!isSkinningShadowBegun)
						{
							skinningShader.BeginShadow(context);
							isSkinningShadowBegun = true;
							isLitShadowBegun = false;
						}
						skinningShader.DrawShadowModel(*cModel.m_spModelData, drawContext);
					}
					else
					{
						drawContext.SetBones(nullptr);
						if (!isLitShadowBegun)
						{
							shadowShader.Begin(context);
							isLitShadowBegun = true;
							isSkinningShadowBegun = false;
						}
						ModelRenderer::Draw(shadowShader, *cModel.m_spModelData, drawContext);
					}
				}
			}

			// Restore
			context.View = oldView;
			context.Projection = oldProj;

			cbLight.DL_ShadowPower = 2.5f;
			cbLight.DL_ShadowBias = 0.001f;
			cbLight.DL_mLightVP[0] = mLightVP;

			pShadowMap->TransitionTo(pCmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		RenderContext& context = Renderer::GetContext();
		context.Light = cbLight;
	}

	void RenderScene(Entity cameraEntity, class RenderTarget* pRT = nullptr)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;
		m_cameraEntity = cameraEntity;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();

		if (pRT)
		{
			pGraphicsDevice->SetRenderTarget(pRT);
			Renderer::BindViewport(pRT);
		}
		else
		{
			pGraphicsDevice->SetBackBuffer();
			// Bind default viewport
			D3D12_VIEWPORT viewport = {};
			viewport.Width = 1280.0f;
			viewport.Height = 720.0f;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			D3D12_RECT scissorRect = { 0, 0, 1280, 720 };
			pCmdList->RSSetViewports(1, &viewport);
			pCmdList->RSSetScissorRects(1, &scissorRect);
		}

		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		RenderContext& context = Renderer::GetContext();
		context.View = cCamera.m_viewMatrix;
		context.Projection = cCamera.m_projMatrix;

		auto& litShader = ShaderLibrary::Instance().Get<LitShader>();
		auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();

		bool isLitBegun = false;
		bool isSkinningBegun = false;

		DrawContext drawContext;

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);

			if (cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
			{
				drawContext.SetWorld(cTransform.m_worldMatrix);

				if (cModel.m_modelType == ModelType::Dynamic)
				{
					auto bones = cModel.m_spModelData->GetBoneMatrices();
						drawContext.SetBones(&bones);
					if (!isSkinningBegun)
					{
						skinningShader.Begin(context);
						isSkinningBegun = true;
						isLitBegun = false;
					}
					ModelRenderer::Draw(skinningShader, *cModel.m_spModelData, drawContext);
				}
				else
				{
					drawContext.SetBones(nullptr);
					if (!isLitBegun)
					{
						litShader.Begin(context);
						isLitBegun = true;
						isSkinningBegun = false;
					}
					ModelRenderer::Draw(litShader, *cModel.m_spModelData, drawContext);
				}
			}
		}
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
};

