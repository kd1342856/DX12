#pragma once
#include "../../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../../Graphics/Shader/LitShader/LitShader.h"
#include "../../../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../../../Graphics/Shader/SkyShader/SkyShader.h"
#include "../../../../Graphics/Renderer/ModelRenderer.h"
#include "../../../../Graphics/Renderer/Renderer.h"
#include "../../../../Graphics/Renderer/RenderManager.h"
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../../Graphics/Descriptor/DescriptorHeapManager.h"
#include "../../../Manager/Asset/MeshManager.h"
#include "../../../DirectX/Utility/Logger.h"
#include "../../../DirectX/Utility/Profiler.h"
#include "../../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../../../Graphics/GPUResource/DepthStencil/DepthStencil.h"
#include "../../../Manager/GameManager.h"
#include "../../Components/Data/NativeScript.h"
#include "../../../../Application/Object/Script/System/ReflectionComponent.h"

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

		RenderContext& context = Renderer::GetContext();
		const CBufferData::Light& cbLight = ShaderManager::Instance().GetLightData();

		Math::Vector3 lightDir = cbLight.DL_Dir; // Use the Light's direction
		if (lightDir.LengthSquared() < 0.001f) {
			lightDir = Math::Vector3(0, -1, 1); // fallback
		}
		lightDir.Normalize();

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

			pGraphicsDevice->GetContextManager()->GetGraphicsContext()->TransitionResource(pShadowMap->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
			pGraphicsDevice->GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
			pShadowMap->ClearBuffer();
			auto dsvH = pGraphicsDevice->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(pShadowMap->GetDSVNumber());
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

			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_isVisible && cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
				{
					bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);
					if (isSkinned) {
						skinningShader.BeginShadow(context);
						DrawContext drawCtx;
						const auto& boneMatrices = cModel.m_spModelData->GetBoneMatrices();
						drawCtx.BoneMatrices = &boneMatrices;
						skinningShader.BeginModel(*cModel.m_spModelData, drawCtx);
					} else {
						shadowShader.Begin(context);
					}

					Math::Matrix world = cTransform.m_worldMatrix;
					const auto& nodes = cModel.m_spModelData->GetNodes();
					for (const auto& node : nodes) {
						if (isSkinned) {
							skinningShader.BeginNode(node, world);
						} else {
							// animDeltaTransform is Identity for non-animated nodes
							// so non-door geometry renders exactly as before
							Math::Matrix nodeWorld = node.animDeltaTransform * world;
							shadowShader.BeginNode(node, nodeWorld);
						}

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh) {
								if (isSkinned) skinningShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
								else shadowShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());

								pMesh->DrawInstanced(pMesh->GetInstanceCount());
							}
						}
					}
				}
			}

			// Restore
			context.View = oldView;
			context.Projection = oldProj;

			ShaderManager::Instance().SetDirectionalLightMatrix(mLightVP);

			pGraphicsDevice->GetContextManager()->GetGraphicsContext()->TransitionResource(pShadowMap->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			pGraphicsDevice->GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
		}
	}
	void RenderReflection(Entity cameraEntity)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;

		auto& ecs = GameManager::Instance().GetECS();
		auto& cCamera = ecs.GetComponent<CameraData>(cameraEntity);
		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pRT = Renderer::GetPlanarReflectionRenderTarget();
		if (!pRT) return;

		bool hasReflection = false;
		Math::Vector3 p, n;
		for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
			if (auto* ref = dynamic_cast<class ReflectionComponent*>(scriptData.Instance.get())) {
				if (!ref->IsActive()) continue; // �����Ƀv���C���[�����Ȃ����͖����Ȃ̂ŃX�L�b�v
				p = ref->m_worldPlanePoint;
				n = ref->m_worldPlaneNormal;
				hasReflection = true;
				break; // �A�N�e�B�u�Ȃ��̂̒��ōŏ��Ɍ����������̂��g��(���˃e�N�X�`����1�����������Ȃ�)
			}
		}

		if (!hasReflection) {
			pGraphicsDevice->SetRenderTarget(pRT);
			pRT->Clear(0.0f, 0.0f, 0.0f, 1.0f);
			// �K���ȍ��ɂ��Ă����u���t���[���͗L���Ȕ��˂��Ȃ��v���Ƃ�����
			ShaderManager::Instance().SetReflectionData(Math::Matrix::Identity, false);
			return;
		}

		// �ꎞ�I�ȃR���e�L�X�g�I�[�o�[���C�h
		RenderContext& context = Renderer::GetContext();
		Math::Matrix oldView = context.View;
		Math::Matrix oldProj = context.Projection;

		// �{���̃J�������[���h�s�񂩂�ʒu�ƕ����𒊏o
		Math::Matrix camWorld = oldView.Invert();
		Math::Vector3 camPos = camWorld.Translation();
		// ����: SimpleMath��Matrix::Forward()�̓��[�J��Z-(��)��Ԃ����A
		// ���̃v���W�F�N�g�́u�O���v�̓��[�J��+Z(�e�ӏ���TransformNormal(Vector3(0,0,1), rot)�Ōv�Z)�B
		// Forward()���g���Ǝ��ۂƂ͋t�����̃x�N�g���ɂȂ�A���˃J�������t�����������Ă��܂�
		// (���𐳖ʂ��猩���Ƃ��Ɋ�ł͂Ȃ��㉺���]�������f�����s��̌��ʂ�\��)�B
		// SimpleMath��Backward()(+Z)�����̃v���W�F�N�g�̑O�����ƈ�v����B
		Math::Vector3 camForward = camWorld.Backward();
		Math::Vector3 camUp = camWorld.Up();

		// ���ˍs��̌v�Z
		Math::Plane plane(p, n);
		Math::Matrix reflectionMatrix = Math::Matrix::CreateReflection(plane);

		// �ʒu�AForward�AUp���ׂĂ𔽎˂�����
		Math::Vector3 refCamPos = Math::Vector3::Transform(camPos, reflectionMatrix);
		Math::Vector3 refCamForward = Math::Vector3::TransformNormal(camForward, reflectionMatrix);
		Math::Vector3 refCamUp = Math::Vector3::TransformNormal(camUp, reflectionMatrix);

		Math::Vector3 refCamTarget = refCamPos + refCamForward;

		// ���˂��ꂽ�ʒu�ƕ�������V����View�s����\�z����(��ԑS�̂͗��Ԃ邪�A���_���͔̂��ˑ��̕���������)
		Math::Matrix refView = Math::Matrix::CreateLookAt(refCamPos, refCamTarget, refCamUp);
		context.View = refView;

		// ���˃e�N�X�`���͐����`(1024x1024)�Ȃ̂ŁA�A�X�y�N�g��1:1��Projection��ʓr�g�ށB
		// (������context.Projection���X�V���Ȃ��ƁA�O�t���[����16:9�J�����pProjection��
		//  �c���̂܂܂ɂȂ�A�����`�̃����_�[�^�[�Q�b�g�ɕ`�悷��Ƙc��ł��܂�)
		Math::Matrix refProj = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(cCamera.m_fov), 1.0f, cCamera.m_nearZ, cCamera.m_farZ);
		context.Projection = refProj;

		// ���̒��ł��̃s�N�Z���̃��[���h���W���ē��e���Đ��������˗p��UV�����߂���悤�ɁA
		// ���˃J������View*Proj�s���n���Ă���
		// (���C���J�����̃X�N���[��UV�����̂܂܎g���񂷂ƁA�ʃJ�����ŕ`�����G�Ƃ͑Ή����Ȃ����߂���͑厖)
		ShaderManager::Instance().SetReflectionData(refView * refProj, true);

		// �ʏ��Opaque�p�X�݂̂�`��
		pGraphicsDevice->SetRenderTarget(pRT);
		pRT->Clear(0.0f, 0.0f, 0.0f, 1.0f);
		Renderer::BindViewport(pRT);

		auto& litShader = ShaderLibrary::Instance().Get<LitShader>();
		auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();
		auto& skyShader = ShaderLibrary::Instance().Get<SkyShader>();

		auto drawEntities = [&](bool isBlendPass) {
			for (auto const& entity : m_entities) {
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
				if (cModel.m_isVisible && cModel.m_spModelData && cModel.m_spModelData->IsLoaded()) {
					bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);
					bool isSky = (cModel.m_modelType == ModelType::Sky);
					if (isSky && isBlendPass) continue;

					if (isSkinned) {
						skinningShader.Begin(context);
						DrawContext drawCtx;
						const auto& boneMatrices = cModel.m_spModelData->GetBoneMatrices();
						drawCtx.BoneMatrices = &boneMatrices;
						skinningShader.BeginModel(*cModel.m_spModelData, drawCtx);
					} else if (isSky) {
						skyShader.Begin(context);
						DrawContext drawCtx;
						skyShader.BeginModel(*cModel.m_spModelData, drawCtx);
					} else {
						litShader.Begin(context);
						DrawContext drawCtx;
						litShader.BeginModel(*cModel.m_spModelData, drawCtx);
					}

					for (auto& node : cModel.m_spModelData->GetNodes()) {
						Math::Matrix world = cTransform.m_worldMatrix;
						if (isSkinned) {
							skinningShader.BeginNode(node, world);
						} else if (isSky) {
							skyShader.BeginNode(node, world);
						} else {
							Math::Matrix nodeWorld = node.animDeltaTransform * world;
							litShader.BeginNode(node, nodeWorld);
						}

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh) {
								bool isMeshBlend = (pMesh->GetMaterial().Constants.alphaMode == 2); // 2: Blend
								if (isMeshBlend == isBlendPass) {
									// ���˃p�X�ł͔��]���Ă��邽��Cull Mode���t�ɂ���̂����_�I�����A
									// ����͂��̂܂ܕ`�悵�AShader���őΏ����邩������Ȃ�
									if (isSkinned) skinningShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
									else if (isSky) skyShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
									else litShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
									pMesh->DrawInstanced(pMesh->GetInstanceCount());
								}
							}
						}
					}
				}
			}
		};

		// �[�x�o�b�t�@���N���A���Ďg�p
		auto* pDepthBuffer = pGraphicsDevice->GetDepthStencil();
		auto dsvH = pGraphicsDevice->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(pDepthBuffer->GetDSVNumber());
		auto rtvH = pGraphicsDevice->GetDescriptorHeapManager()->GetRTVAllocator()->GetCPUHandle(pRT->GetRTVIndex());

		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->TransitionResource(pDepthBuffer->GetBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
		pDepthBuffer->ClearBuffer();
		pGraphicsDevice->GetCmdList()->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		drawEntities(false); // Opaque �̂�

		// ����
		context.View = oldView;
		context.Projection = oldProj;

		pGraphicsDevice->TransitionToSRV(pRT);
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

		// Camera-space frustum from the projection, moved into world space by the camera's
		// world transform (inverse of the view matrix) - used below to skip draw commands
		// for anything outside the view instead of recording+drawing everything every frame.
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, cCamera.m_projMatrix);
		frustum.Transform(frustum, cCamera.m_viewMatrix.Invert());

		auto& litShader = ShaderLibrary::Instance().Get<LitShader>();
		auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();

		// �f�o�b�O: �ŏ��̃t���[���̂�ImGui�ɃG���e�B�e�B�̕`�搔���o�͂���
		bool bFirstFrame = (m_debugLogFrameCount == 0);
		if (bFirstFrame)
		{
			Logger::Instance().AddLog(Logger::LogLevel::Info,
				"[RenderSystem] === RenderScene �J�n �G���e�B�e�B��: %d ===", (int)m_entities.size());
		}
		m_debugLogFrameCount++;

		auto drawEntities = [&](bool isBlendPass) {
			for (auto const& entity : m_entities)
			{
				auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
				auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);

				if (cModel.m_isVisible && cModel.m_spModelData && cModel.m_spModelData->IsLoaded())
				{
					bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);
					bool isSky = (cModel.m_modelType == ModelType::Sky);

					if (isSky && isBlendPass) continue; // Sky�� Opaque�p�X�̂�

					// Entity-level frustum cull. Sky is exempt - it's meant to always
					// surround the camera, and its own "bounds" don't mean much culled.
					// Reported to the Profiler once (opaque pass only) to avoid double-
					// counting the same entity when the blend pass runs the same check.
					if (!isSky)
					{
						DirectX::BoundingBox entityBoundsLocal;
						if (cModel.m_spModelData->TryGetLocalBounds(entityBoundsLocal))
						{
							DirectX::BoundingBox entityBoundsWorld;
							entityBoundsLocal.Transform(entityBoundsWorld, cTransform.m_worldMatrix);
							bool culled = !frustum.Intersects(entityBoundsWorld);
							if (!isBlendPass) Profiler::Instance().AddEntityCullResult(culled);
							if (culled) continue;
						}
						// else: bounds not ready yet (assets still streaming in) - draw
						// unconditionally rather than guess wrong and hide something.
					}

					// �f�o�b�O���O: 1�t���[���ڂ̂ݏڍ׏����o�� (Opaque�p�X���̂�)
					if (bFirstFrame && !isBlendPass)
					{
						const auto& boneMatricesDbg = cModel.m_spModelData->GetBoneMatrices();
						const auto& nodesDbg = cModel.m_spModelData->GetNodes();
						int totalMeshes = 0;
						for (const auto& nd : nodesDbg) totalMeshes += (int)nd.meshes.size();

						//Logger::Instance().AddLog(Logger::LogLevel::Info,
						//	"[RenderSystem] Entity=%u Type=%s Bones=%d Nodes=%d TotalMeshes=%d",
						//	(uint32_t)entity,
						//	isSkinned ? "Dynamic(Skinned)" : "Static",
						//	(int)boneMatricesDbg.size(),
						//	(int)nodesDbg.size(),
						//	totalMeshes);

						int meshIdx = 0;
						for (const auto& nd : nodesDbg)
						{
							for (const auto& meshHandle : nd.meshes)
							{
								Mesh* pMeshDbg = MeshManager::Instance().Get(meshHandle);
								if (pMeshDbg)
								{
									const Material& mat = pMeshDbg->GetMaterial();
									Logger::Instance().AddLog(Logger::LogLevel::Info,
										"  Mesh[%d] Node=%s AlbedoValid=%d NormalValid=%d MetalRoughValid=%d",
										meshIdx,
										nd.name.c_str(),
										(int)mat.BaseColor.valid,
										(int)mat.Normal.valid,
										(int)mat.MetallicRoughness.valid);
								}
								meshIdx++;
							}
						}
					}

					if (isSkinned) {
						skinningShader.Begin(context);
						DrawContext drawCtx;
						const auto& boneMatrices = cModel.m_spModelData->GetBoneMatrices();
						drawCtx.BoneMatrices = &boneMatrices;
						skinningShader.BeginModel(*cModel.m_spModelData, drawCtx);
					} else if (isSky) {
						auto& skyShader = ShaderLibrary::Instance().Get<SkyShader>();
						skyShader.Begin(context);
					} else {
						litShader.Begin(context);
					}

					Math::Matrix world = cTransform.m_worldMatrix;
					const auto& nodes = cModel.m_spModelData->GetNodes();
					for (const auto& node : nodes) {
						if (isSkinned) {
							skinningShader.BeginNode(node, world);
						} else if (isSky) {
							auto& skyShader = ShaderLibrary::Instance().Get<SkyShader>();
							skyShader.BeginNode(node, world);
						} else {
							Math::Matrix nodeWorld = node.animDeltaTransform * world;
							litShader.BeginNode(node, nodeWorld);
						}

						// Same nodeWorld transform used above for the static (lit) case -
						// recomputed here since it's local to the branch above.
						Math::Matrix staticNodeWorld = node.animDeltaTransform * world;

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh) {
								bool isMeshBlend = (pMesh->GetMaterial().Constants.alphaMode == 2);
								if (isBlendPass != isMeshBlend) continue;

								// Mesh-level frustum cull - only for the static/lit path.
								// This is what actually matters for something like the house
								// model, where dozens of room/door meshes share one entity:
								// the entity-level box above covers the whole house and can't
								// reject individual rooms, but this can. Skinned/sky meshes
								// are left alone (few of them, and a skinned mesh's bind-pose
								// AABB doesn't reliably bound the animated pose anyway).
								if (!isSkinned && !isSky) {
									DirectX::BoundingBox meshWorldBounds;
									pMesh->GetLocalAABB().Transform(meshWorldBounds, staticNodeWorld);
									bool meshCulled = !frustum.Intersects(meshWorldBounds);
									if (!isBlendPass) Profiler::Instance().AddMeshCullResult(meshCulled);
									if (meshCulled) continue;
								}

								if (isSkinned) skinningShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
								else if (isSky) {
									auto& skyShader = ShaderLibrary::Instance().Get<SkyShader>();
									skyShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
								}
								else litShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());

								pMesh->DrawInstanced(pMesh->GetInstanceCount());
							}
						}
					}
				}
				else if (bFirstFrame && !isBlendPass)
				{
					bool hasData = (cModel.m_spModelData != nullptr);
					bool isLoaded = hasData && cModel.m_spModelData->IsLoaded();
					Logger::Instance().AddLog(Logger::LogLevel::Warning,
						"[RenderSystem] Entity=%u ���f���X�L�b�v HasData=%d IsLoaded=%d",
						(uint32_t)entity, (int)hasData, (int)isLoaded);
				}
			}
		};

		// �p�X1: Opaque & Mask
		drawEntities(false);

		auto* pGraphicsContext = pGraphicsDevice->GetContextManager()->GetGraphicsContext();

		// Opaque�p�X�̌��ʂ� Refraction �p�ɃR�s�[
		if (pRT)
		{
			auto* pDestRT = Renderer::GetSceneOpaqueCopyRenderTarget();
			if (pDestRT)
			{
				auto* pSrcBuffer = pRT->GetResource();
				auto* pDestBuffer = pDestRT->GetResource();

				// ��ԑJ��: Src��COPY_SOURCE��, Dest��COPY_DEST��
				pGraphicsContext->TransitionResource(pSrcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
				pGraphicsContext->TransitionResource(pDestBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
				pGraphicsContext->FlushResourceBarriers();

				pCmdList->CopyResource(pDestBuffer, pSrcBuffer);

				// ��ԑJ��: Src��RENDER_TARGET�ɖ߂�, Dest��SRV(ShaderResource)��
				pGraphicsContext->TransitionResource(pSrcBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				pGraphicsContext->TransitionResource(pDestBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				pGraphicsContext->FlushResourceBarriers();
			}
		}

		// Depth Buffer �� SRV �� Transition
		auto* pDepthBuffer = pGraphicsDevice->GetDepthStencil()->GetBuffer();
		pGraphicsContext->TransitionResource(pDepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		pGraphicsContext->FlushResourceBarriers();

		// �p�X2: Blend & Decal
		drawEntities(true);

		// Depth Buffer �� �������ݗp�ɖ߂�
		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->TransitionResource(pDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
	// �f�o�b�O: 1�t���[���ڂ̂݃��O���o�͂��邽�߂̃J�E���^
	int m_debugLogFrameCount = 0;
};

