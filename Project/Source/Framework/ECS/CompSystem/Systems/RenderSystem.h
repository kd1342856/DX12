#pragma once
#include <algorithm>
#include "../../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../../Graphics/Shader/LitShader/LitShader.h"
#include "../../../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../../../Graphics/Shader/SkyShader/SkyShader.h"
#include "../../../../Graphics/Shader/NormalPrepassShader/NormalPrepassShader.h"
#include "../../../../Graphics/Shader/VelocityShader/VelocityShader.h"
#include "../../../../Graphics/Renderer/MotionVectors.h"
#include "../../../../Graphics/Renderer/TemporalAA.h"
#include "../../../../Graphics/Renderer/RaytracingScene.h"
#include "../../../../Graphics/Renderer/ModelRenderer.h"
#include "../../../../Graphics/Renderer/Renderer.h"
#include "../../../../Graphics/Renderer/RenderManager.h"
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../../Graphics/Shader/ShaderCompiler/ShaderCompiler.h"
#include "../../../../Graphics/Shadow/LocalShadowManager.h"
#include "../../../../Graphics/Renderer/QualitySettings.h"
#include "../../../../Graphics/Descriptor/DescriptorHeapManager.h"
#include "../../../Manager/Asset/MeshManager.h"
#include "../../../DirectX/Utility/Logger.h"
#include "../../../DirectX/Utility/Profiler.h"
#include "../../../Manager/NavMesh/RoomVisibilityManager.h"
#include "../../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../../../Graphics/GPUResource/DepthStencil/DepthStencil.h"
#include "../../../Manager/GameManager.h"
#include "../../../Manager/Particle/ParticleManager.h"
#include "../../Components/Data/NativeScript.h"
#include "../../../../Application/Object/Script/System/ReflectionComponent.h"
#include "../../../Object/GameObject.h"

// RenderSystem: Draws entities with Transform + ModelRenderData components
class RenderSystem : public SystemBase
{
public:
	Math::Vector3 GetLightDirection() const { return m_lightDirection; }
	void SetLightDirection(const Math::Vector3& dir) { m_lightDirection = dir; }

	Entity GetCameraEntity() const { return m_cameraEntity; }
	void SetCameraEntity(Entity cameraEntity) { m_cameraEntity = cameraEntity; }

	// TAAのジッター(投影のずれ、NDC)。深度プリパス・メインの描画・速度のパスの投影に足す。
	// メインのカメラの描画の間だけ設定し、終わったら0に戻す(反射やプレビューには掛けない)
	void SetProjectionJitter(const Math::Vector2& jitterNdc) { m_projectionJitter = jitterNdc; }

	// RendererPanel用の実行時トグル - 描画の不具合がカリング（フラスタム/ポータルルーム）に
	// 起因するのか、それとは無関係なのかをA/Bテストするためにオフにする。
	static inline bool s_enableFrustumCulling = true;
	static inline bool s_enableRoomCulling = true;

	// ModelRenderData.m_glowColorが設定されていれば(照明の点灯中など)、Emissiveを上書きした
	// マテリアルのコピーをscratchに作って返す。共有マテリアル自体は書き換えない
	// (同じモデルを使う消灯中の照明まで光ってしまうため)。対象外ならそのまま返す。
	static const Material& ResolveGlowMaterial(const ModelRenderData& model, const Material& mat, Material& scratch)
	{
		bool glow = model.m_glowColor.LengthSquared() > 0.0f &&
			(model.m_glowMaterial.empty() || model.m_glowMaterial == mat.Name);
		// 反射で光る面(吊り下げ照明のかさの内側など): 裏面だけを弱めに光らせる
		const bool reflectGlow = !glow && model.m_glowColor.LengthSquared() > 0.0f && model.m_glowReflectScale > 0.0f &&
			!model.m_glowReflectMaterial.empty() && model.m_glowReflectMaterial == mat.Name;
		if (!glow && !reflectGlow && !model.m_dissolveEnabled) return mat;
		scratch = mat;
		if (reflectGlow) {
			scratch.Constants.emissiveFactorX = model.m_glowColor.x * model.m_glowReflectScale;
			scratch.Constants.emissiveFactorY = model.m_glowColor.y * model.m_glowReflectScale;
			scratch.Constants.emissiveFactorZ = model.m_glowColor.z * model.m_glowReflectScale;
			scratch.Constants.emissiveStrength = 1.0f;
			scratch.Constants.emissiveFrontScale = 0.0f;
			scratch.Constants.emissiveBackScale = 1.0f;
		}
		if (glow) {
			// 発光マップがあればそれをマスクとして使う(例: シーリングライトはガラス部分だけ白いマップ)。
			// 無ければ白テクスチャ扱いになり、メッシュ全体がFactorの色で光る。
			scratch.Constants.emissiveFactorX = model.m_glowColor.x;
			scratch.Constants.emissiveFactorY = model.m_glowColor.y;
			scratch.Constants.emissiveFactorZ = model.m_glowColor.z;
			scratch.Constants.emissiveStrength = 1.0f;
		}
		if (model.m_dissolveEnabled) {
			// 足元からのディゾルブ(SkinningShader_PS.hlslのproceduralType==5)
			scratch.Constants.proceduralType = 5;
			scratch.Constants.dissolveHeight = model.m_dissolveHeight;
			scratch.Constants.dissolveEdgeWidth = model.m_dissolveEdgeWidth;
			scratch.Constants.dissolveEdgeColor = model.m_dissolveEdgeColor;
		}
		return scratch;
	}

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

			// テクセル単位でカメラの位置を丸めて、カメラが動いても影の縁がちらつかないようにする(解像度は画質設定で変わる)
			float texelSize = 100.0f / static_cast<float>(desc.Width);
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
							Math::Matrix nodeWorld = cModel.GetNodeAnimDelta(&node - cModel.m_spModelData->GetNodes().data(), node) * world;
							shadowShader.BeginNode(node, nodeWorld);
						}

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh && pMesh->IsReady()) {
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

	// 局所光源(スポットライト・点光源)の影。LocalShadowManagerが割り当てた枠のうち、
	// 中身が古いもの(ライトが動いた・範囲内で影を落とす物が動いた・まだ描いていない)だけを描き直す。
	// 動かないライトの影はこれでキャッシュされ、範囲内で何も動かなければ描かない
	void RenderLocalLightShadows()
	{
		if (!m_pCoordinator) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();
		auto* pGraphicsContext = pGraphicsDevice->GetContextManager()->GetGraphicsContext();
		PROFILE_CPU_SCOPE("LocalLightShadows");
		PROFILE_GPU_SCOPE(pCmdList, "LocalLightShadows");

		UpdateShadowCasterChanges();

		RenderContext& context = Renderer::GetContext();
		const Math::Matrix oldView = context.View;
		const Math::Matrix oldProjection = context.Projection;

		auto renderSlots = [&](std::vector<LocalShadowManager::Slot>& slots, ShadowMapArray* pShadowMaps)
		{
			if (!pShadowMaps)
			{
				return;
			}
			const UINT faceCount = pShadowMaps->IsCube() ? 6 : 1;
			bool isWriting = false;
			for (size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
			{
				LocalShadowManager::Slot& slot = slots[slotIndex];
				if (!slot.isRequested)
				{
					continue;
				}
				if (slot.isValid && !IsShadowCasterChangedNear(slot.position, slot.range))
				{
					continue;
				}
				if (!isWriting)
				{
					pGraphicsContext->TransitionResource(pShadowMaps->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
					pGraphicsContext->FlushResourceBarriers();
					isWriting = true;
				}
				for (UINT faceIndex = 0; faceIndex < faceCount; ++faceIndex)
				{
					RenderShadowFace(pShadowMaps, static_cast<UINT>(slotIndex) * faceCount + faceIndex, slot.view[faceIndex], slot.projection);
				}
				slot.isValid = true;
			}
			// 描き直さなかったフレームも、シェーダーから読める状態にしておく
			pGraphicsContext->TransitionResource(pShadowMaps->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			pGraphicsContext->FlushResourceBarriers();
		};
		renderSlots(LocalShadowManager::Instance().GetSpotSlots(), pGraphicsDevice->GetSpotShadowMaps());
		renderSlots(LocalShadowManager::Instance().GetCubeSlots(), pGraphicsDevice->GetCubeShadowMaps());

		context.View = oldView;
		context.Projection = oldProjection;
	}

	// 最大3枚(3窓)まで同時にアクティブな平面反射をサポートする。各アクティブな
	// ReflectionComponentに専用の反射カメラ+RT(スロット0〜2)を割り当て、そのメッシュの
	// Material.reflectionSlotにスロット番号を書き込む。ガラスのピクセルシェーダーは
	// 自分のreflectionSlotに対応するg_planarReflectionMapN/g_mReflectionVP[N]を使う。
	// 4枚目以降が同時にアクティブになった場合は割り当てられず(見つかった順の先着3枚まで)、
	// SSRフォールバックのまま表示される - 既知の制限。
	void RenderReflection(Entity cameraEntity)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;

		auto& ecs = GameManager::Instance().GetECS();
		auto& cCamera = ecs.GetComponent<CameraData>(cameraEntity);
		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();

		// 前フレームの割り当てが残らないよう、まず全メッシュのreflectionSlotをリセットする。
		// これをやらないと、鏡が非アクティブになった窓が古いスロットのVP/テクスチャを
		// 使い続けてしまう(古い絵がそのまま映り続けるバグの元)。
		for (auto const& entity : m_entities) {
			auto* cModel = m_pCoordinator->TryGetComponent<ModelRenderData>(entity);
			if (!cModel || !cModel->m_spModelData) continue;
			for (auto& node : cModel->m_spModelData->GetNodes()) {
				for (auto const& meshHandle : node.meshes) {
					Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
					if (pMesh) pMesh->GetMaterialRef().Constants.reflectionSlot = -1;
				}
			}
		}
		ShaderManager::Instance().ClearAllReflectionData();

		// アクティブなReflectionComponentを先着3個まで集める
		constexpr int kMaxReflections = 3;
		class ReflectionComponent* activeComps[kMaxReflections] = {};
		Math::Vector3 activeP[kMaxReflections];
		Math::Vector3 activeN[kMaxReflections];
		int activeCount = 0;
		for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
			if (auto* ref = dynamic_cast<class ReflectionComponent*>(scriptData.Instance.get())) {
				if (!ref->IsActive()) continue;
				// 同時に描く数は画質設定で決める(描かない窓はSSRで代用される)
				if (activeCount >= (std::min)(kMaxReflections, QualitySettings::GetCurrent().maxPlanarReflections)) break;
				activeP[activeCount] = ref->m_worldPlanePoint;
				activeN[activeCount] = ref->m_worldPlaneNormal;
				activeComps[activeCount] = ref;
				activeCount++;
			}
		}

		// 使わないスロットのRTは黒クリアしておく(前回の絵が残ると変なものが映る)
		auto clearSlot = [&](int slot) {
			auto* pRT = Renderer::GetPlanarReflectionRenderTarget(slot);
			if (!pRT) return;
			pGraphicsDevice->SetRenderTarget(pRT);
			pRT->Clear(0.0f, 0.0f, 0.0f, 1.0f);
			pGraphicsDevice->TransitionToSRV(pRT);
		};

		if (activeCount == 0) {
			for (int slot = 0; slot < kMaxReflections; ++slot) clearSlot(slot);
			return;
		}
		for (int slot = activeCount; slot < kMaxReflections; ++slot) clearSlot(slot);

		// 一時的なコンテキストオーバーライド
		RenderContext& context = Renderer::GetContext();
		Math::Matrix oldView = context.View;
		Math::Matrix oldProj = context.Projection;

		// 本物のカメラのワールド行列から位置と方向を抽出
		// 注意: SimpleMathのMatrix::Forward()はローカルZ-(奥)を返すが、
		// このプロジェクトの「前」はローカル+Z(手持ちアイテムもTransformNormal(Vector3(0,0,1), rot)で計算)。
		// Forward()を使うと実際とは逆方向のベクトルになり、反射カメラも逆方向を向いてしまう。
		// SimpleMathのBackward()(+Z)がこのプロジェクトの前方向と一致する。
		Math::Matrix camWorld = oldView.Invert();
		Math::Vector3 camPos = camWorld.Translation();
		Math::Vector3 camForward = camWorld.Backward();
		Math::Vector3 camUp = camWorld.Up();

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
							Math::Matrix nodeWorld = cModel.GetNodeAnimDelta(&node - cModel.m_spModelData->GetNodes().data(), node) * world;
							litShader.BeginNode(node, nodeWorld);
						}

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh && pMesh->IsReady()) {
								bool isMeshBlend = (pMesh->GetMaterial().Constants.alphaMode == 2); // 2: Blend
								if (isMeshBlend == isBlendPass) {
									Material glowScratch;
									const Material& drawMat = ResolveGlowMaterial(cModel, pMesh->GetMaterial(), glowScratch);
									if (isSkinned) skinningShader.BeforeDrawMesh(*pMesh, drawMat);
									else if (isSky) skyShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
									else litShader.BeforeDrawMesh(*pMesh, drawMat);
									pMesh->DrawInstanced(pMesh->GetInstanceCount());
								}
							}
						}
					}
				}
			}
		};

		for (int slot = 0; slot < activeCount; ++slot)
		{
			auto* pRT = Renderer::GetPlanarReflectionRenderTarget(slot);
			if (!pRT) continue;

			// このスロットを使うメッシュにreflectionSlotを割り当てる
			if (auto* pGO = activeComps[slot]->GetGameObject()) {
				if (auto* cModel = m_pCoordinator->TryGetComponent<ModelRenderData>(pGO->GetEntityID())) {
					if (cModel->m_spModelData) {
						for (auto& node : cModel->m_spModelData->GetNodes()) {
							for (auto const& meshHandle : node.meshes) {
								Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
								if (pMesh) pMesh->GetMaterialRef().Constants.reflectionSlot = slot;
							}
						}
					}
				}
			}

			Math::Matrix refView;
			Math::Matrix refProj;

			if (activeComps[slot]->m_useFixedCameraSource)
			{
				// 「反射」の数学は一切行わず、Inspectorでドラッグ&ドロップにより明示的に
				// 指定したGameObject(ReflectionComponent::m_fixedCameraSourceUUID)の
				// CameraDataをそのままこの鏡に映す(監視カメラ/モニター的な使い方)。
				// 「フラグが立っているものを検索」という曖昧な方式だと、削除し損ねた孤児
				// コンポーネントが誤って優先される事故が起きたため、明示指定に変更した。
				Entity fixedCamEntity = INVALID_ENTITY;
				if (auto* pFixedCamObj = activeComps[slot]->ResolveFixedCameraSource())
				{
					if (ecs.TryGetComponent<CameraData>(pFixedCamObj->GetEntityID()))
					{
						fixedCamEntity = pFixedCamObj->GetEntityID();
					}
				}

				{
					static int s_dbgFrameFixed = 0;
					if ((s_dbgFrameFixed++ % 60) == 0)
					{
						Logger::Instance().AddLog(Logger::LogLevel::Info,
							"[Reflection][FixedCam] slot=%d fixedCamEntity=%u", slot, (uint32_t)fixedCamEntity);
					}
				}

				if (fixedCamEntity != INVALID_ENTITY)
				{
					auto& fixedCamData = ecs.GetComponent<CameraData>(fixedCamEntity);
					refView = fixedCamData.m_viewMatrix;
					// 反射テクスチャは正方形(1024x1024)なので、アスペクト1:1のProjectionを組み直す。
					refProj = DirectX::XMMatrixPerspectiveFovLH(
						DirectX::XMConvertToRadians(fixedCamData.m_fov), 1.0f, fixedCamData.m_nearZ, fixedCamData.m_farZ);

					static int s_dbgFrameFixed2 = 0;
					if ((s_dbgFrameFixed2++ % 60) == 0)
					{
						Math::Vector3 camWorldPos = fixedCamData.m_viewMatrix.Invert().Translation();
						Logger::Instance().AddLog(Logger::LogLevel::Info,
							"[Reflection][FixedCam] using camPos=(%.2f,%.2f,%.2f) fov=%.1f near=%.3f far=%.1f",
							camWorldPos.x, camWorldPos.y, camWorldPos.z, fixedCamData.m_fov, fixedCamData.m_nearZ, fixedCamData.m_farZ);
					}
				}
				else
				{
					// 固定カメラが見つからない(Debug Preview Cameraが立っていない)場合は
					// 通常の自動反射にフォールバックする。
					Math::Plane fallbackPlane(activeP[slot], activeN[slot]);
					Math::Matrix fallbackReflect = Math::Matrix::CreateReflection(fallbackPlane);
					Math::Vector3 fallbackPos = Math::Vector3::Transform(camPos, fallbackReflect);
					Math::Vector3 fallbackForward = Math::Vector3::TransformNormal(camForward, fallbackReflect);
					Math::Vector3 fallbackUp = Math::Vector3::TransformNormal(camUp, fallbackReflect);
					refView = Math::Matrix::CreateLookAt(fallbackPos, fallbackPos + fallbackForward, fallbackUp);
					refProj = DirectX::XMMatrixPerspectiveFovLH(
						DirectX::XMConvertToRadians(cCamera.m_fov), 1.0f, cCamera.m_nearZ, cCamera.m_farZ);
				}
			}
			else
			{
				// 反射行列の計算
				Math::Plane plane(activeP[slot], activeN[slot]);
				Math::Matrix reflectionMatrix = Math::Matrix::CreateReflection(plane);

				// 位置、Forward、Upすべてを反射させる
				Math::Vector3 refCamPos = Math::Vector3::Transform(camPos, reflectionMatrix);
				Math::Vector3 refCamForward = Math::Vector3::TransformNormal(camForward, reflectionMatrix);
				Math::Vector3 refCamUp = Math::Vector3::TransformNormal(camUp, reflectionMatrix);
				Math::Vector3 refCamTarget = refCamPos + refCamForward;

				// Debug: スロット0だけ、数値ダンプ+デバッグ線を出す(全スロット出すと見づらいため)。
				if (slot == 0)
				{
					static int s_dbgFrame = 0;
					if ((s_dbgFrame++ % 60) == 0)
					{
						Logger::Instance().AddLog(Logger::LogLevel::Info,
							"[Reflection] plane p=(%.2f,%.2f,%.2f) n=(%.2f,%.2f,%.2f)", activeP[slot].x, activeP[slot].y, activeP[slot].z, activeN[slot].x, activeN[slot].y, activeN[slot].z);
						Logger::Instance().AddLog(Logger::LogLevel::Info,
							"[Reflection] camPos=(%.2f,%.2f,%.2f) camForward=(%.2f,%.2f,%.2f)", camPos.x, camPos.y, camPos.z, camForward.x, camForward.y, camForward.z);
						Logger::Instance().AddLog(Logger::LogLevel::Info,
							"[Reflection] refCamPos=(%.2f,%.2f,%.2f) refCamForward=(%.2f,%.2f,%.2f) activeCount=%d", refCamPos.x, refCamPos.y, refCamPos.z, refCamForward.x, refCamForward.y, refCamForward.z, activeCount);
					}

					CollisionManager::Instance().AddDebugLine(camPos, camPos + camForward * 2.0f, IM_COL32(255, 255, 0, 255));
					CollisionManager::Instance().AddDebugLine(refCamPos, refCamTarget, IM_COL32(255, 0, 255, 255));
					CollisionManager::Instance().AddDebugLine(camPos, refCamPos, IM_COL32(255, 255, 255, 255));
				}

				// 反射された位置と方向から新しいView行列を構築する(空間全体は歪むが、視点側は反射側の見た目だけ正しい)
				refView = Math::Matrix::CreateLookAt(refCamPos, refCamTarget, refCamUp);

				// 反射テクスチャは正方形(1024x1024)なので、アスペクト1:1のProjectionを別途組む。
				refProj = DirectX::XMMatrixPerspectiveFovLH(
					DirectX::XMConvertToRadians(cCamera.m_fov), 1.0f, cCamera.m_nearZ, cCamera.m_farZ);
			}

			context.View = refView;
			context.Projection = refProj;
			context.IsMainView = false;

			// このガラスのピクセルシェーダーがワールド座標を再投影して正しい反射用のUVを
			// 求められるように、反射カメラのView*Proj行列をスロット別に渡しておく
			ShaderManager::Instance().SetReflectionData(slot, refView * refProj, true);

			// 通常のOpaqueパスのみ描画
			pGraphicsDevice->SetRenderTarget(pRT);
			pRT->Clear(0.0f, 0.0f, 0.0f, 1.0f);
			Renderer::BindViewport(pRT);

			drawEntities(false); // Opaque のみ

			pGraphicsDevice->TransitionToSRV(pRT);
		}

		// 復元
		context.View = oldView;
		context.Projection = oldProj;
		context.IsMainView = true;
	}

	// SSAO/SSR用のビュー空間法線プリパス。RenderScene直前に呼ぶ。
	// スキニングメッシュ/半透明/Skyは対象外(近似用途のため単純化 - Phase3の既知の制限)。
	// 深度プリパス。シーンの深度(pDepthRTの深度)と、SSAO・SSR用のビュー空間の法線(pNormalRT)を描く。
	// メインの描画(RenderSceneのuseDepthPrepass)は、ここで描いたメッシュをEQUALで描くので、
	// 見えているピクセルだけに重いライティングが走る。描く条件はLitShader::IsDepthPrepassMaterialと合わせること
	void RenderNormalPrepass(Entity cameraEntity, class RenderTarget* pNormalRT, class RenderTarget* pDepthRT)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY || !pNormalRT || !pDepthRT) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();
		auto* pGraphicsContext = pGraphicsDevice->GetContextManager()->GetGraphicsContext();
		auto* pHeapManager = pGraphicsDevice->GetDescriptorHeapManager();
		PROFILE_CPU_SCOPE("DepthPrepass");
		PROFILE_GPU_SCOPE(pCmdList, "DepthPrepass");

		// 色は法線のRT、深度はシーンのRTの深度に描く
		pGraphicsContext->TransitionResource(pNormalRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		pGraphicsContext->TransitionResource(pDepthRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pGraphicsContext->FlushResourceBarriers();
		auto rtvHandle = pHeapManager->GetRTVAllocator()->GetCPUHandle(pNormalRT->GetRTVIndex());
		auto dsvHandle = pHeapManager->GetDSVAllocator()->GetCPUHandle(pDepthRT->GetDSVIndex());
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
		Renderer::BindViewport(pNormalRT);
		const float clearNormal[] = { 0.5f, 0.5f, 1.0f, 1.0f }; // (0,0,1)相当(カメラ正面向き)
		pCmdList->ClearRenderTargetView(rtvHandle, clearNormal, 0, nullptr);
		pCmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		RenderContext& context = Renderer::GetContext();
		context.View = cCamera.m_viewMatrix;
		context.Projection = TemporalAA::ApplyJitter(cCamera.m_projMatrix, m_projectionJitter);

		// メインの描画と同じフラスタムで、エンティティ単位とメッシュ単位のカリングをする。
		// メインはこれに加えてルームカリングもするので、メインで描くメッシュは必ずここでも描かれる
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, cCamera.m_projMatrix);
		frustum.Transform(frustum, cCamera.m_viewMatrix.Invert());

		auto& normalShader = ShaderLibrary::Instance().Get<NormalPrepassShader>();

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!cModel.m_isVisible || !cModel.m_spModelData || !cModel.m_spModelData->IsLoaded()) continue;
			if (cModel.m_modelType == ModelType::Dynamic) continue; // スキニングはメインの描画で深度も書く
			if (cModel.m_modelType == ModelType::Sky) continue;

			if (s_enableFrustumCulling)
			{
				DirectX::BoundingBox entityBoundsLocal;
				if (cModel.m_spModelData->TryGetLocalBounds(entityBoundsLocal))
				{
					DirectX::BoundingBox entityBoundsWorld;
					entityBoundsLocal.Transform(entityBoundsWorld, cTransform.m_worldMatrix);
					if (!frustum.Intersects(entityBoundsWorld)) continue;
				}
			}

			normalShader.Begin(context);
			Math::Matrix world = cTransform.m_worldMatrix;
			for (const auto& node : cModel.m_spModelData->GetNodes())
			{
				Math::Matrix nodeWorld = cModel.GetNodeAnimDelta(&node - cModel.m_spModelData->GetNodes().data(), node) * world;
				normalShader.BeginNode(node, nodeWorld);
				for (const auto& meshHandle : node.meshes)
				{
					Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
					if (!pMesh || !pMesh->IsReady())
					{
						continue;
					}
					// 半透明・アルファテスト・プロシージャルは、メインの描画で深度を書く(EQUALにしない)
					if (!LitShader::IsDepthPrepassMaterial(pMesh->GetMaterial()))
					{
						continue;
					}
					if (s_enableFrustumCulling)
					{
						DirectX::BoundingBox meshWorldBounds;
						pMesh->GetLocalAABB().Transform(meshWorldBounds, nodeWorld);
						if (!frustum.Intersects(meshWorldBounds)) continue;
					}
					normalShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
					pMesh->DrawInstanced(pMesh->GetInstanceCount());
				}
			}
		}

		// SSAO・SSRは法線のRTの深度を読むので、シーンの深度をそこへ写す
		pGraphicsContext->TransitionResource(pDepthRT->GetDepthResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		pGraphicsContext->TransitionResource(pNormalRT->GetDepthResource(), D3D12_RESOURCE_STATE_COPY_DEST);
		pGraphicsContext->FlushResourceBarriers();
		pCmdList->CopyResource(pNormalRT->GetDepthResource(), pDepthRT->GetDepthResource());
		pGraphicsContext->TransitionResource(pDepthRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		pGraphicsContext->TransitionResource(pNormalRT->GetDepthResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		pGraphicsContext->TransitionResource(pNormalRT->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		pGraphicsContext->FlushResourceBarriers();
	}

	// useDepthPrepass: 直前にRenderNormalPrepassでpRTの深度を描いたならtrue。プリパスで描いた不透明のメッシュをEQUALで描く
	void RenderScene(Entity cameraEntity, class RenderTarget* pRT = nullptr, bool useDepthPrepass = false)
	{
		if (!m_pCoordinator) return;
		if (cameraEntity == INVALID_ENTITY) return;
		m_cameraEntity = cameraEntity;

		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		SceneView sceneView;
		sceneView.view = cCamera.m_viewMatrix;
		sceneView.projection = TemporalAA::ApplyJitter(cCamera.m_projMatrix, m_projectionJitter);
		sceneView.cullingProjection = cCamera.m_projMatrix;
		sceneView.useDepthPrepass = useDepthPrepass;
		RenderSceneView(sceneView, pRT);
	}

	// シーンを描く視点と、描き方
	struct SceneView
	{
		Math::Matrix view;
		Math::Matrix projection;        // 描画に使う投影(ジッターを含んでよい)
		Math::Matrix cullingProjection; // カリングに使う投影(ジッターを含まない)
		bool useDepthPrepass = false;
		// 反射プローブの撮影。ルームカリング・半透明・パーティクル・屈折用のコピーを省く
		// (半透明とガラスは、メインの視点の画面のテクスチャ(深度・屈折・SSR)を読むため正しく描けない)
		bool isProbeCapture = false;
	};

	// 行列を指定してシーンを描く(メインのカメラ・プレビュー・反射プローブの撮影で共通)
	void RenderSceneView(const SceneView& sceneView, class RenderTarget* pRT)
	{
		const bool useDepthPrepass = sceneView.useDepthPrepass;
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
			Renderer::BindDefaultViewport();
		}

		RenderContext& context = Renderer::GetContext();
		context.View = sceneView.view;
		context.Projection = sceneView.projection;
		context.IsMainView = !sceneView.isProbeCapture && pRT == Renderer::GetSceneHDRRenderTarget();

		// 投影行列から作ったカメラ空間のフラスタムを、カメラのワールド変換(View行列の逆)で
		// ワールド空間に移す - 毎フレーム全部記録+描画するのではなく、視界外のものの
		// 描画コマンドを以下でスキップするために使う。
		Math::Matrix camWorld = sceneView.view.Invert();
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, sceneView.cullingProjection);
		frustum.Transform(frustum, camWorld);

		// フラスタム判定の上にさらにポータル/ルームカリングを重ねる - フラスタムカリング
		// だけでは足りない理由(部屋が視錐台の中にあっても壁の裏に隠れていることがある)は
		// RoomVisibilityManagerを参照。
		// 反射プローブは部屋の中から全方向を撮るので、ルームカリングはしない
		const bool useRoomCulling = s_enableRoomCulling && !sceneView.isProbeCapture;
		if (useRoomCulling)
		{
			RoomVisibilityManager::Instance().UpdateVisibleRooms(camWorld.Translation());
		}

		auto& litShader = ShaderLibrary::Instance().Get<LitShader>();
		auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();

		// デバッグ: 最初のフレームのみImGuiにエンティティの描画数を出力する
		bool bFirstFrame = (m_debugLogFrameCount == 0);
		if (bFirstFrame)
		{
			Logger::Instance().AddLog(Logger::LogLevel::Info,
				u8"[RenderSystem] === RenderScene 開始 エンティティ数: %d ===", (int)m_entities.size());
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

					if (isSky && isBlendPass) continue; // SkyはOpaqueパスのみ

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

					// デバッグログ: 1フレーム目のみ詳細情報を出力 (Opaqueパス時のみ)
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
							Math::Matrix nodeWorld = cModel.GetNodeAnimDelta(&node - cModel.m_spModelData->GetNodes().data(), node) * world;
							litShader.BeginNode(node, nodeWorld);
						}

						// 上のstatic(lit)ケースで使ったのと同じnodeWorld変換 -
						// 上のブランチ内のローカル変数なので、ここで計算し直している。
						Math::Matrix staticNodeWorld = cModel.GetNodeAnimDelta(&node - cModel.m_spModelData->GetNodes().data(), node) * world;

						for (const auto& meshHandle : node.meshes) {
							Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
							if (pMesh && pMesh->IsReady()) {
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
									if (!meshCulled && useRoomCulling
										&& !cModel.m_spModelData->IsNodeAnimated(node.name)
										&& cModel.m_spModelData->GetTotalMeshCount() >= kRoomCullingMinMeshCount) {
										meshCulled = !RoomVisibilityManager::Instance().IsMeshInVisibleRoom(pMesh, staticNodeWorld);
									}
									if (!isBlendPass) Profiler::Instance().AddMeshCullResult(meshCulled);
									if (meshCulled) continue;
								}

								Material glowScratch;
								const Material& drawMat = ResolveGlowMaterial(cModel, pMesh->GetMaterial(), glowScratch);
								if (isSkinned) skinningShader.BeforeDrawMesh(*pMesh, drawMat);
								else if (isSky) {
									auto& skyShader = ShaderLibrary::Instance().Get<SkyShader>();
									skyShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
								}
								else
								{
									// プリパスで深度を描いたメッシュはEQUALで描く。元のマテリアルと描くマテリアルの両方が条件を満たすときだけ
									litShader.SetDepthEqual(useDepthPrepass && !isBlendPass &&
										LitShader::IsDepthPrepassMaterial(pMesh->GetMaterial()) && LitShader::IsDepthPrepassMaterial(drawMat));
									litShader.BeforeDrawMesh(*pMesh, drawMat);
								}

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
						u8"[RenderSystem] Entity=%u モデルスキップ HasData=%d IsLoaded=%d",
						(uint32_t)entity, (int)hasData, (int)isLoaded);
				}
			}
		};

		// パス1: Opaque & Mask
		drawEntities(false);

		auto* pGraphicsContext = pGraphicsDevice->GetContextManager()->GetGraphicsContext();

		// 反射プローブの撮影では、不透明とアルファテストだけを描く
		if (sceneView.isProbeCapture)
		{
			litShader.SetDepthEqual(false);
			context.IsMainView = true;
			return;
		}

		// Opaqueパスの結果を Refraction 用にコピー
		if (pRT)
		{
			auto* pDestRT = Renderer::GetSceneOpaqueCopyRenderTarget();
			if (pDestRT)
			{
				auto* pSrcBuffer = pRT->GetResource();
				auto* pDestBuffer = pDestRT->GetResource();

				// 状態遷移: SrcはCOPY_SOURCEへ, DestはCOPY_DESTへ
				pGraphicsContext->TransitionResource(pSrcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
				pGraphicsContext->TransitionResource(pDestBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
				pGraphicsContext->FlushResourceBarriers();

				pCmdList->CopyResource(pDestBuffer, pSrcBuffer);

				// 状態遷移: SrcはRENDER_TARGETに戻す, DestはSRV(ShaderResource)へ
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
		litShader.SetDepthEqual(false);

		// Depth Buffer を書き込み用に戻す
		if (pRT)
		{
			pGraphicsDevice->TransitionDepthToWrite(pRT);
		}

		// パス3: パーティクル(加算合成。PSOがHDRターゲット用なのでpRTがある時だけ)
		if (pRT)
		{
			ParticleManager::Instance().Render(context);
		}
	}

	// レイトレース(DDGI)の加速構造に、動かないモデルの不透明とアルファテストのメッシュを並べる。
	// 照明の点灯(m_glowColor)による発光も、当たった所の明るさとして入れる
	void BuildRaytracingScene()
	{
		if (!m_pCoordinator) return;
		auto& scene = RaytracingScene::Instance();
		if (!scene.IsSupported()) return;
		PROFILE_CPU_SCOPE("BuildRaytracingScene");

		scene.BeginFrame();
		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!cModel.m_isVisible || !cModel.m_spModelData || !cModel.m_spModelData->IsLoaded()) continue;
			if (cModel.m_modelType == ModelType::Dynamic || cModel.m_modelType == ModelType::Sky) continue;

			const Math::Matrix& world = cTransform.m_worldMatrix;
			const auto& nodes = cModel.m_spModelData->GetNodes();
			for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
			{
				const auto& node = nodes[nodeIndex];
				const Math::Matrix nodeWorld = cModel.GetNodeAnimDelta(nodeIndex, node) * world;
				for (const auto& meshHandle : node.meshes)
				{
					Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
					if (!pMesh || !pMesh->IsReady()) continue;
					// 半透明(ガラスなど)は光を通すものとして入れない
					if (pMesh->GetMaterial().Constants.alphaMode == 2) continue;
					Material glowScratch;
					const Material& material = ResolveGlowMaterial(cModel, pMesh->GetMaterial(), glowScratch);
					scene.AddInstance(*pMesh, material, nodeWorld);
				}
			}
		}
		scene.Build();
	}

	// 速度(モーションベクトル)を描く。メインの描画(RenderScene)の後に呼ぶ。
	// カメラの動きによる速度を全画面に書いてから、前のフレームから動いた物だけを描き直して上書きする
	void RenderVelocity(Entity cameraEntity, class RenderTarget* pSceneRT)
	{
		if (!m_pCoordinator) return;
		auto& motionVectors = MotionVectors::Instance();
		if (cameraEntity == INVALID_ENTITY || !pSceneRT || !motionVectors.IsValid()) return;

		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();
		auto* pGraphicsContext = pGraphicsDevice->GetContextManager()->GetGraphicsContext();
		auto* pHeapManager = pGraphicsDevice->GetDescriptorHeapManager();
		PROFILE_CPU_SCOPE("Velocity");
		PROFILE_GPU_SCOPE(pCmdList, "Velocity");

		motionVectors.RenderCameraVelocity(pSceneRT);

		GPUTexture* pVelocity = motionVectors.GetVelocityTexture();
		auto rtvHandle = pHeapManager->GetRTVAllocator()->GetCPUHandle(pVelocity->GetRTVIndex());
		auto dsvHandle = pHeapManager->GetDSVAllocator()->GetCPUHandle(pSceneRT->GetDSVIndex());
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
		Renderer::BindViewport(pSceneRT);

		auto& cCamera = m_pCoordinator->GetComponent<CameraData>(cameraEntity);
		RenderContext& context = Renderer::GetContext();
		context.View = cCamera.m_viewMatrix;
		context.Projection = TemporalAA::ApplyJitter(cCamera.m_projMatrix, m_projectionJitter);

		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, cCamera.m_projMatrix);
		frustum.Transform(frustum, cCamera.m_viewMatrix.Invert());

		VelocityShader::FrameConstants frame;
		frame.currentViewProjection = motionVectors.GetCurrentViewProjection();
		frame.previousViewProjection = motionVectors.GetPreviousViewProjection();
		auto& velocityShader = ShaderLibrary::Instance().Get<VelocityShader>();

		++m_motionFrame;
		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!cModel.m_isVisible || !cModel.m_spModelData || !cModel.m_spModelData->IsLoaded()) continue;
			if (cModel.m_modelType == ModelType::Sky) continue;

			const bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);
			const auto& nodes = cModel.m_spModelData->GetNodes();

			// 今のフレームの位置(ワールド行列・ノードのアニメーション・骨)を集める。
			// 毎フレームのメモリの確保を避けるため、作業用の入れ物を使い回す(前のフレームの物と入れ替えていく)
			MotionState& current = m_scratchMotionState;
			current.world = cTransform.m_worldMatrix;
			current.lastSeenFrame = m_motionFrame;
			if (isSkinned)
			{
				const auto& bones = cModel.m_spModelData->GetBoneMatrices();
				current.bones.assign(bones.begin(), bones.end());
				current.nodeDeltas.clear();
			}
			else
			{
				current.bones.clear();
				current.nodeDeltas.resize(nodes.size());
				for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
				{
					current.nodeDeltas[nodeIndex] = cModel.GetNodeAnimDelta(nodeIndex, nodes[nodeIndex]);
				}
			}

			// 前のフレームと比べる。初めて見た物は動いていないものとして扱う
			auto it = m_motionStates.find(entity);
			if (it == m_motionStates.end())
			{
				m_motionStates.emplace(entity, current);
				continue;
			}
			MotionState& previous = it->second;
			const bool isMoving = !IsSameMatrices(&current.world, &previous.world, 1) ||
				current.bones.size() != previous.bones.size() || !IsSameMatrices(current.bones.data(), previous.bones.data(), current.bones.size()) ||
				current.nodeDeltas.size() != previous.nodeDeltas.size() || !IsSameMatrices(current.nodeDeltas.data(), previous.nodeDeltas.data(), current.nodeDeltas.size());

			bool isVisible = true;
			DirectX::BoundingBox entityBoundsLocal;
			if (isMoving && cModel.m_spModelData->TryGetLocalBounds(entityBoundsLocal))
			{
				DirectX::BoundingBox entityBoundsWorld;
				entityBoundsLocal.Transform(entityBoundsWorld, cTransform.m_worldMatrix);
				isVisible = frustum.Intersects(entityBoundsWorld);
			}

			if (isMoving && isVisible)
			{
				if (isSkinned)
				{
					velocityShader.BeginSkinned(context, frame);
					velocityShader.SetBones(current.bones, previous.bones);
					// スキニングは骨の行列がノードの変換を含むので、エンティティのワールド行列だけを使う(SkinningShaderと同じ)
					velocityShader.SetWorld(current.world, previous.world);
				}
				else
				{
					velocityShader.BeginStatic(context, frame);
				}
				for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
				{
					const auto& node = nodes[nodeIndex];
					if (!isSkinned)
					{
						velocityShader.SetWorld(current.nodeDeltas[nodeIndex] * current.world, previous.nodeDeltas[nodeIndex] * previous.world);
					}
					for (const auto& meshHandle : node.meshes)
					{
						Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
						if (!pMesh || !pMesh->IsReady()) continue;
						// 半透明は深度を書かないので、速度も書かない
						if (pMesh->GetMaterial().Constants.alphaMode == 2) continue;
						velocityShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
						pMesh->DrawInstanced(pMesh->GetInstanceCount());
					}
				}
			}
			// 今の位置を次のフレームの「前の位置」にする(古い方は作業用の入れ物として次の物で使う)
			std::swap(previous, current);
		}

		// このフレームで見つからなかった物(非表示・削除)は忘れる。次に現れたとき前の位置から飛んで見えないように
		for (auto it = m_motionStates.begin(); it != m_motionStates.end();)
		{
			if (it->second.lastSeenFrame != m_motionFrame)
			{
				it = m_motionStates.erase(it);
			}
			else
			{
				++it;
			}
		}

		pGraphicsContext->TransitionResource(pVelocity->GetResource(), GPUTexture::kInitialState);
		pGraphicsContext->FlushResourceBarriers();
	}

private:
	// 影の1面(2D配列の1枚、またはキューブの1面)を描く
	void RenderShadowFace(ShadowMapArray* pShadowMaps, UINT faceIndex, const Math::Matrix& view, const Math::Matrix& projection)
	{
		auto* pGraphicsDevice = &GDF::Instance().GetGraphicsDevice();
		auto* pCmdList = pGraphicsDevice->GetCmdList();

		const float resolution = static_cast<float>(pShadowMaps->GetResolution());
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, resolution, resolution, 0.0f, 1.0f };
		D3D12_RECT scissor = { 0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution) };
		pCmdList->RSSetViewports(1, &viewport);
		pCmdList->RSSetScissorRects(1, &scissor);

		pShadowMaps->ClearFace(faceIndex);
		auto dsvHandle = pGraphicsDevice->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(pShadowMaps->GetDSVNumber(faceIndex));
		pCmdList->OMSetRenderTargets(0, nullptr, false, &dsvHandle);

		RenderContext& context = Renderer::GetContext();
		context.View = view;
		context.Projection = projection;

		// この面の視錐台に入る物だけを描く
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, projection);
		frustum.Transform(frustum, view.Invert());
		DrawShadowCasters(frustum);
	}

	// 影を落とす物を、今のcontextのView/Projectionで深度だけ描く
	void DrawShadowCasters(const DirectX::BoundingFrustum& frustum)
	{
		RenderContext& context = Renderer::GetContext();
		auto& shadowShader = ShaderLibrary::Instance().Get<ShadowShader>();
		auto& skinningShader = ShaderLibrary::Instance().Get<SkinningShader>();

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!IsShadowCaster(cModel))
			{
				continue;
			}
			DirectX::BoundingBox localBounds;
			if (cModel.m_spModelData->TryGetLocalBounds(localBounds))
			{
				DirectX::BoundingBox worldBounds;
				localBounds.Transform(worldBounds, cTransform.m_worldMatrix);
				if (!frustum.Intersects(worldBounds))
				{
					continue;
				}
			}

			const bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);
			if (isSkinned)
			{
				skinningShader.BeginShadow(context);
				DrawContext drawContext;
				const auto& boneMatrices = cModel.m_spModelData->GetBoneMatrices();
				drawContext.BoneMatrices = &boneMatrices;
				skinningShader.BeginModel(*cModel.m_spModelData, drawContext);
			}
			else
			{
				shadowShader.Begin(context);
			}

			const Math::Matrix& world = cTransform.m_worldMatrix;
			const auto& nodes = cModel.m_spModelData->GetNodes();
			for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
			{
				const auto& node = nodes[nodeIndex];
				if (isSkinned)
				{
					skinningShader.BeginNode(node, world);
				}
				else
				{
					shadowShader.BeginNode(node, cModel.GetNodeAnimDelta(nodeIndex, node) * world);
				}
				for (const auto& meshHandle : node.meshes)
				{
					Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
					if (!pMesh || !pMesh->IsReady())
					{
						continue;
					}
					// 半透明(ガラスなど)は影を落とさない。電球のガラスが光源を囲んで光を全部さえぎらないように
					if (pMesh->GetMaterial().Constants.alphaMode == 2)
					{
						continue;
					}
					if (isSkinned)
					{
						skinningShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
					}
					else
					{
						shadowShader.BeforeDrawMesh(*pMesh, pMesh->GetMaterial());
					}
					pMesh->DrawInstanced(pMesh->GetInstanceCount());
				}
			}
		}
	}

	static bool IsShadowCaster(const ModelRenderData& model)
	{
		return model.m_isVisible && model.m_castShadow && model.m_modelType != ModelType::Sky &&
			model.m_spModelData && model.m_spModelData->IsLoaded();
	}

	// 影を落とす物のうち、前のフレームから動いた(または現れた・消えた)物の範囲を集める。
	// 局所光源の影の枠は、この範囲がライトの届く範囲に重なったときだけ描き直す
	void UpdateShadowCasterChanges()
	{
		++m_shadowCasterFrame;
		m_changedShadowCasterBounds.clear();

		for (auto const& entity : m_entities)
		{
			auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);
			auto& cModel = m_pCoordinator->GetComponent<ModelRenderData>(entity);
			if (!IsShadowCaster(cModel))
			{
				continue;
			}

			DirectX::BoundingSphere bounds(cTransform.m_worldMatrix.Translation(), 2.0f);
			DirectX::BoundingBox localBounds;
			if (cModel.m_spModelData->TryGetLocalBounds(localBounds))
			{
				DirectX::BoundingBox worldBounds;
				localBounds.Transform(worldBounds, cTransform.m_worldMatrix);
				DirectX::BoundingSphere::CreateFromBoundingBox(bounds, worldBounds);
			}

			// ワールド行列と、ノードのアニメーション(扉の開閉など)の差分行列から変化を調べる
			uint64_t signature = ShaderCompiler::HashBytes(&cTransform.m_worldMatrix, sizeof(Math::Matrix));
			const auto& nodes = cModel.m_spModelData->GetNodes();
			for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
			{
				signature = ShaderCompiler::HashBytes(&cModel.GetNodeAnimDelta(nodeIndex, nodes[nodeIndex]), sizeof(Math::Matrix), signature);
			}
			// スキニングのモデルは骨のアニメーションで毎フレーム形が変わるので、常に動いたものとして扱う
			const bool isSkinned = (cModel.m_modelType == ModelType::Dynamic);

			auto it = m_shadowCasterStates.find(entity);
			if (it == m_shadowCasterStates.end())
			{
				m_changedShadowCasterBounds.push_back(bounds);
				m_shadowCasterStates[entity] = { signature, bounds, m_shadowCasterFrame };
				continue;
			}
			ShadowCasterState& state = it->second;
			if (isSkinned || state.signature != signature)
			{
				m_changedShadowCasterBounds.push_back(state.bounds);
				m_changedShadowCasterBounds.push_back(bounds);
			}
			state = { signature, bounds, m_shadowCasterFrame };
		}

		// このフレームで見つからなかった物(非表示になった・削除された)は、前の位置の影が消えるので変化として扱う
		for (auto it = m_shadowCasterStates.begin(); it != m_shadowCasterStates.end();)
		{
			if (it->second.lastSeenFrame != m_shadowCasterFrame)
			{
				m_changedShadowCasterBounds.push_back(it->second.bounds);
				it = m_shadowCasterStates.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	bool IsShadowCasterChangedNear(const Math::Vector3& lightPosition, float range) const
	{
		const DirectX::BoundingSphere lightBounds(lightPosition, range);
		for (const DirectX::BoundingSphere& changed : m_changedShadowCasterBounds)
		{
			if (lightBounds.Intersects(changed))
			{
				return true;
			}
		}
		return false;
	}

	struct ShadowCasterState
	{
		uint64_t signature = 0;
		DirectX::BoundingSphere bounds;
		uint64_t lastSeenFrame = 0;
	};
	std::unordered_map<Entity, ShadowCasterState> m_shadowCasterStates;
	std::vector<DirectX::BoundingSphere> m_changedShadowCasterBounds;
	uint64_t m_shadowCasterFrame = 0;

	// 速度のパス: 物ごとの前のフレームの位置
	struct MotionState
	{
		Math::Matrix world;
		std::vector<Math::Matrix> nodeDeltas; // スタティックメッシュのノードのアニメーション(扉の開閉など)
		std::vector<Math::Matrix> bones;      // スキニングの骨
		uint64_t lastSeenFrame = 0;
	};
	static bool IsSameMatrices(const Math::Matrix* pA, const Math::Matrix* pB, size_t count)
	{
		return count == 0 || std::memcmp(pA, pB, sizeof(Math::Matrix) * count) == 0;
	}
	std::unordered_map<Entity, MotionState> m_motionStates;
	MotionState m_scratchMotionState;
	uint64_t m_motionFrame = 0;
	Math::Vector2 m_projectionJitter = Math::Vector2::Zero;

	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
	// デバッグ: 1フレーム目のみログを出力するためのカウンタ
	int m_debugLogFrameCount = 0;
};

