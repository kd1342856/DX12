#pragma once
#include <algorithm>
#include "../../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../../Graphics/Shader/LitShader/LitShader.h"
#include "../../../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../../../Graphics/Shader/SkyShader/SkyShader.h"
#include "../../../../Graphics/Shader/NormalPrepassShader/NormalPrepassShader.h"
#include "../../../../Graphics/Renderer/ModelRenderer.h"
#include "../../../../Graphics/Renderer/Renderer.h"
#include "../../../../Graphics/Renderer/RenderManager.h"
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../../Graphics/Descriptor/DescriptorHeapManager.h"
#include "../../../Manager/Asset/MeshManager.h"
#include "../../../DirectX/Utility/Logger.h"
#include "../../../DirectX/Utility/Profiler.h"
#include "../../../Manager/NavMesh/RoomVisibilityManager.h"
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

	// RendererPanel用の実行時トグル - 描画の不具合がカリング（フラスタム/ポータルルーム）に
	// 起因するのか、それとは無関係なのかをA/Bテストするためにオフにする。
	static inline bool s_enableFrustumCulling = true;
	static inline bool s_enableRoomCulling = true;

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

	// 最も近いポイントライト1灯だけの簡易シャドウ(単一パースペクティブ、真のキューブシャドウではない)。
	// lightPos: 光源のワールド座標。aimAt: 影を落としたい方向の目標点(通常は視点/プレイヤー位置)。
	// range: ライトの減衰距離(そのままシャドウ投影のFar面にする)。
	void RenderPointLightShadow(const Math::Vector3& lightPos, const Math::Vector3& aimAt, float range)
	{
		if (!m_pCoordinator) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();

		auto* pShadowMap = pGraphicsDevice->GetPointLightShadowMap();
		if (!pShadowMap) return;

		Math::Vector3 aimDir = aimAt - lightPos;
		if (aimDir.LengthSquared() < 0.0001f) aimDir = Math::Vector3(0, -1, 0);
		aimDir.Normalize();
		Math::Vector3 up = (std::abs(aimDir.y) > 0.95f) ? Math::Vector3(1, 0, 0) : Math::Vector3(0, 1, 0);

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

		// 広めのFOV(120度)で単一方向をカバーする近似。真のキューブシャドウではないので、
		// このFOVの外(=光源からほぼ真後ろ)は影が付かない。
		Math::Matrix mLightView = Math::Matrix::CreateLookAt(lightPos, lightPos + aimDir, up);
		Math::Matrix mLightProj = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(120.0f), 1.0f, 0.05f, std::max(range, 1.0f));
		Math::Matrix mLightVP = mLightView * mLightProj;

		RenderContext& context = Renderer::GetContext();
		Math::Matrix oldView = context.View;
		Math::Matrix oldProj = context.Projection;
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

		context.View = oldView;
		context.Projection = oldProj;

		// LitShader_PS.hlsl側でNdotLに応じてこの値を最大8倍まで広げる(スロープスケールバイアス)ので、
		// ここはまっすぐ光を受ける面での最小値だけ決めればよい。
		ShaderManager::Instance().SetPointLightShadowData(mLightVP, 0.0015f, true);

		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->TransitionResource(pShadowMap->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		pGraphicsDevice->GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
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

		// Debug: numeric dump of everything the reflection math uses, throttled to ~1/sec so it's
		// readable in the editor's Console window instead of eyeballing 3D debug lines.
		{
			static int s_dbgFrame = 0;
			if ((s_dbgFrame++ % 60) == 0)
			{
				Logger::Instance().AddLog(Logger::LogLevel::Info,
					"[Reflection] plane p=(%.2f,%.2f,%.2f) n=(%.2f,%.2f,%.2f)", p.x, p.y, p.z, n.x, n.y, n.z);
				Logger::Instance().AddLog(Logger::LogLevel::Info,
					"[Reflection] camPos=(%.2f,%.2f,%.2f) camForward=(%.2f,%.2f,%.2f)", camPos.x, camPos.y, camPos.z, camForward.x, camForward.y, camForward.z);
				Logger::Instance().AddLog(Logger::LogLevel::Info,
					"[Reflection] refCamPos=(%.2f,%.2f,%.2f) refCamForward=(%.2f,%.2f,%.2f)", refCamPos.x, refCamPos.y, refCamPos.z, refCamForward.x, refCamForward.y, refCamForward.z);

				// Also log the Player body mesh's own world "forward" (project convention: +Z / Backward()),
				// so we can tell whether the mesh's front actually points the same way the camera looks.
				for (auto const& e : m_entities)
				{
					auto* cM = m_pCoordinator->TryGetComponent<ModelRenderData>(e);
					if (!cM || cM->m_filePath.find("Player.gltf") == std::string::npos) continue;
					auto* cT = m_pCoordinator->TryGetComponent<TransformData>(e);
					if (!cT) continue;
					Math::Vector3 modelForward = cT->m_worldMatrix.Backward();
					Math::Vector3 modelPos = cT->m_worldMatrix.Translation();
					float dot = modelForward.Dot(camForward);
					Logger::Instance().AddLog(Logger::LogLevel::Info,
						"[Reflection] PlayerModel pos=(%.2f,%.2f,%.2f) forward=(%.2f,%.2f,%.2f) dot(vs camForward)=%.2f (%s)",
						modelPos.x, modelPos.y, modelPos.z, modelForward.x, modelForward.y, modelForward.z, dot,
						dot > 0 ? "SAME direction as camera" : "OPPOSITE direction from camera");
					break;
				}
			}
		}

		Math::Vector3 refCamTarget = refCamPos + refCamForward;

		// Debug visualization: real camera direction(yellow), reflected camera position+direction(magenta),
		// line connecting both(white, should cross the mirror plane). Visible in editor free-cam(F5) debug draw.
		CollisionManager::Instance().AddDebugLine(camPos, camPos + camForward * 2.0f, IM_COL32(255, 255, 0, 255));
		CollisionManager::Instance().AddDebugLine(refCamPos, refCamTarget, IM_COL32(255, 0, 255, 255));
		CollisionManager::Instance().AddDebugLine(camPos, refCamPos, IM_COL32(255, 255, 255, 255));
		{
			float s = 0.15f;
			CollisionManager::Instance().AddDebugLine(refCamPos - Math::Vector3(s, 0, 0), refCamPos + Math::Vector3(s, 0, 0), IM_COL32(0, 255, 255, 255));
			CollisionManager::Instance().AddDebugLine(refCamPos - Math::Vector3(0, s, 0), refCamPos + Math::Vector3(0, s, 0), IM_COL32(0, 255, 255, 255));
			CollisionManager::Instance().AddDebugLine(refCamPos - Math::Vector3(0, 0, s), refCamPos + Math::Vector3(0, 0, s), IM_COL32(0, 255, 255, 255));
		}

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

		// Note: SetRenderTarget(pRT) above already bound pRT's own private RTV+DSV, and
		// pRT->Clear() already cleared both. This used to re-bind here with a *different*,
		// wrongly-sized shared depth buffer (GraphicsDevice::GetDepthStencil(), sized for the
		// main window, not this 1024x1024 reflection target) - depth testing against a
		// mismatched buffer meant geometry drawn here could silently fail the depth test,
		// leaving the reflection render target essentially black. Removed; pRT's own
		// depth buffer (already bound/cleared above) is what should be used.

		drawEntities(false); // Opaque のみ

		// ����
		context.View = oldView;
		context.Projection = oldProj;

		pGraphicsDevice->TransitionToSRV(pRT);
	}

	// SSAO/SSR用のビュー空間法線プリパス。RenderScene直前に呼ぶ。
	// スキニングメッシュ/半透明/Skyは対象外(近似用途のため単純化 - Phase3の既知の制限)。
	void RenderNormalPrepass(Entity cameraEntity, class RenderTarget* pNormalRT)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY || !pNormalRT) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();

		pGraphicsDevice->SetRenderTarget(pNormalRT);
		Renderer::BindViewport(pNormalRT);
		pNormalRT->Clear(0.5f, 0.5f, 1.0f, 1.0f); // (0,0,1)相当(カメラ正面向き)でクリア

		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		RenderContext& context = Renderer::GetContext();
		context.View = cCamera.m_viewMatrix;
		context.Projection = cCamera.m_projMatrix;

		auto& normalShader = ShaderLibrary::Instance().Get<NormalPrepassShader>();

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!cModel.m_isVisible || !cModel.m_spModelData || !cModel.m_spModelData->IsLoaded()) continue;
			if (cModel.m_modelType == ModelType::Dynamic) continue; // スキニングは今回対象外
			if (cModel.m_modelType == ModelType::Sky) continue;

			normalShader.Begin(context);
			Math::Matrix world = cTransform.m_worldMatrix;
			for (const auto& node : cModel.m_spModelData->GetNodes())
			{
				Math::Matrix nodeWorld = node.animDeltaTransform * world;
				normalShader.BeginNode(node, nodeWorld);
				for (const auto& meshHandle : node.meshes)
				{
					Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
					if (pMesh)
					{
						bool isMeshBlend = (pMesh->GetMaterial().Constants.alphaMode == 2);
						if (isMeshBlend) continue; // 半透明はAO/SSRの元データに含めない
						pMesh->DrawInstanced(pMesh->GetInstanceCount());
					}
				}
			}
		}

		pGraphicsDevice->TransitionToSRV(pNormalRT);
		pGraphicsDevice->TransitionDepthToSRV(pNormalRT);
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

		// 投影行列から作ったカメラ空間のフラスタムを、カメラのワールド変換(View行列の逆)で
		// ワールド空間に移す - 毎フレーム全部記録+描画するのではなく、視界外のものの
		// 描画コマンドを以下でスキップするために使う。
		Math::Matrix camWorld = cCamera.m_viewMatrix.Invert();
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, cCamera.m_projMatrix);
		frustum.Transform(frustum, camWorld);

		// フラスタム判定の上にさらにポータル/ルームカリングを重ねる - フラスタムカリング
		// だけでは足りない理由(部屋が視錐台の中にあっても壁の裏に隠れていることがある)は
		// RoomVisibilityManagerを参照。
		RoomVisibilityManager::Instance().UpdateVisibleRooms(camWorld.Translation());

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

					// エンティティ単位のフラスタムカリング。Skyは常にカメラを取り囲む
					// ものなので対象外にしている（そもそも「境界」の意味があまり無い）。
					// Profilerへの報告はOpaqueパスのみ1回だけ行い、Blendパスで同じ判定を
					// もう一度走らせた時に同じエンティティを二重カウントしないようにしている。
					if (!isSky && s_enableFrustumCulling)
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
						// else: バウンズがまだ準備できていない(アセットがまだストリーミング中)
						// - 誤って何かを隠すよりは、無条件に描画する。
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

						// 上のstatic(lit)ケースで使ったのと同じnodeWorld変換 -
						// 上のブランチ内のローカル変数なので、ここで計算し直している。
						Math::Matrix staticNodeWorld = node.animDeltaTransform * world;

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh) {
								bool isMeshBlend = (pMesh->GetMaterial().Constants.alphaMode == 2);
								if (isBlendPass != isMeshBlend) continue;

								// メッシュ単位のフラスタムカリング - static/litパスのみ。
								// 家のモデルのように、1エンティティに部屋やドアのメッシュが
								// 何十個も紐づくケースで実際に効いてくるのはこれ:
								// 上のエンティティ単位のボックスは家全体を覆ってしまい
								// 個々の部屋を除外できないが、これならできる。Skinned/sky
								// メッシュは対象外にしている(数が少ないし、Skinnedメッシュの
								// バインドポーズAABBはアニメーション後のポーズを正しく
								// 包含できるとは限らないため)。
								if (!isSkinned && !isSky) {
									DirectX::BoundingBox meshWorldBounds;
									pMesh->GetLocalAABB().Transform(meshWorldBounds, staticNodeWorld);
									bool meshCulled = s_enableFrustumCulling && !frustum.Intersects(meshWorldBounds);
									// ルームカリングは初回問い合わせ時にメッシュの部屋割り当てを
									// キャッシュして二度と評価し直さない - ドアの開閉のように
									// 実際に動くものには不適切なので、動くノードは対象外にして
									// フラスタム判定のみに頼る。小物(メッシュ数が少ない)は
									// ルームカリングの恩恵がほとんど無い割に、その境界ケースの
									// 影響を受けやすい(例: 出入り口付近に置かれたピックアップ
									// アイテム) - 実質的に意味のあるメッシュ数を持つモデルにだけ適用する。
									constexpr int kRoomCullingMinMeshCount = 6;
									if (!meshCulled && s_enableRoomCulling
										&& !cModel.m_spModelData->IsNodeAnimated(node.name)
										&& cModel.m_spModelData->GetTotalMeshCount() >= kRoomCullingMinMeshCount) {
										meshCulled = !RoomVisibilityManager::Instance().IsMeshInVisibleRoom(pMesh, staticNodeWorld);
									}
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

		// Depth Buffer を SRV へ Transition
		// (pRTが実際に持つ専用深度バッファ - g_opaqueDepth/DOF/SSAO/SSRはここを読む。
		//  以前はGraphicsDevice::GetDepthStencil()という別の未使用気味なバッファを
		//  遷移させていて、血痕デカール等が実際には書き込まれていない深度を見てしまっていた)
		if (pRT)
		{
			pGraphicsDevice->TransitionDepthToSRV(pRT);
		}

		// パス2: Blend & Decal
		drawEntities(true);

		// Depth Buffer を書き込み用に戻す
		if (pRT)
		{
			pGraphicsDevice->TransitionDepthToWrite(pRT);
		}
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
	// �f�o�b�O: 1�t���[���ڂ̂݃��O���o�͂��邽�߂̃J�E���^
	int m_debugLogFrameCount = 0;
};

