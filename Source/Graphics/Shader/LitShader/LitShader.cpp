#include "../../../Pch.h"
#include "LitShader.h"

#include "../../GDF/GDF.h"
#include "../../Renderer/RenderContext.h"
#include "../../Renderer/Renderer.h"
#include "../../../Framework/Manager/Asset/TextureManager.h"

void LitShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/LitShader/LitShader_VS.hlsl", L"Asset/Shader/LitShader/LitShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Opaque
	desc.CullMode = CullMode::None; // ���˂̋����`��ŃJ�����O�����]���Ă��`�悳���悤��None�ɂ���
	desc.BlendMode = BlendMode::None;
	desc.DepthBias = 0;
	desc.IsDepthMask = true;
	m_psoOpaque = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);

	// Mask
	desc.CullMode = CullMode::None;
	desc.BlendMode = BlendMode::None;
	desc.DepthBias = 0;
	desc.IsDepthMask = true;
	m_psoMask = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);

	// Blend (�f�J�[���p)
	desc.CullMode = CullMode::None;
	desc.BlendMode = BlendMode::Alpha;
	desc.DepthBias = -100; // Z�t�@�C�g������Ď�O�ɕ`��
	desc.IsDepthMask = false; // �������Ȃ̂�Z�������݂Ȃ�
	m_psoBlend = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);

	// �f�t�H���g
	m_pPipelineState = m_psoOpaque.Get();
}

void LitShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);

	int b3 = GetRootParameterIndex(ShaderBindingType::CBV, 3);
	if (b3 != -1) context.BindLight(b3);

	int b4 = GetRootParameterIndex(ShaderBindingType::CBV, 4);
	if (b4 != -1) context.BindSystem(b4);

	// Bind ShadowMap to t7
	int t7 = GetRootParameterIndex(ShaderBindingType::SRV, 7);
	if (t7 != -1) {
		auto* pShadowMap = m_pDevice->GetShadowMap();
		if (pShadowMap && pShadowMap->GetSRVNumber() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pShadowMap->GetSRVNumber());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t7, handle);
		}
	}

	// Bind Opaque Depth to t9
	// 以前はGraphicsDevice::GetDepthStencil()(共有の未使用気味なバッファ)を束縛していたが、
	// 実際のOpaqueパスはpSceneHDRが持つ専用深度バッファに書き込まれるため、血痕デカールの
	// 深度フェードやDOFのCoC計算がここを見ても正しい深度を拾えていなかった。
	// pSceneHDR自身の深度SRV(RenderTarget::GetDepthSRVIndex)を束縛するよう修正。
	int t9 = GetRootParameterIndex(ShaderBindingType::SRV, 9);
	if (t9 != -1) {
		auto* pSceneHDR = Renderer::GetSceneHDRRenderTarget();
		if (pSceneHDR && pSceneHDR->GetDepthSRVIndex() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pSceneHDR->GetDepthSRVIndex());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t9, handle);
		}
	}
	// Bind Refraction Map (Scene Opaque Copy) to t10
	int t10 = GetRootParameterIndex(ShaderBindingType::SRV, 10);
	if (t10 != -1) {
		auto* pOpaqueCopy = Renderer::GetSceneOpaqueCopyRenderTarget();
		if (pOpaqueCopy && pOpaqueCopy->GetSRVIndex() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pOpaqueCopy->GetSRVIndex());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t10, handle);
		}
	}
	
	// Bind Planar Reflection Map to t11
	int t11 = GetRootParameterIndex(ShaderBindingType::SRV, 11);
	if (t11 != -1) {
		auto* pReflection = Renderer::GetPlanarReflectionRenderTarget();
		if (pReflection && pReflection->GetSRVIndex() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pReflection->GetSRVIndex());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t11, handle);
		}
	}

	// Bind SSAO Map to t12
	// GameScene::RenderSSAOが 生成(index0)->ブラーH(index0→1)->ブラーV(index1→0) の順で処理する
	// ので、最終結果は必ずindex0に入っている。
	int t12 = GetRootParameterIndex(ShaderBindingType::SRV, 12);
	if (t12 != -1) {
		auto* pSSAO = Renderer::GetSSAORenderTarget(0);
		if (pSSAO && pSSAO->GetSRVIndex() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pSSAO->GetSRVIndex());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t12, handle);
		}
	}

	// Bind Point Light Shadow Map to t13
	int t13 = GetRootParameterIndex(ShaderBindingType::SRV, 13);
	if (t13 != -1) {
		auto* pPLShadowMap = m_pDevice->GetPointLightShadowMap();
		if (pPLShadowMap && pPLShadowMap->GetSRVNumber() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pPLShadowMap->GetSRVNumber());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t13, handle);
		}
	}
}

void LitShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = nodeWorld;
	int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
	if (b1 != -1) GDF::Instance().BindCBuffer(b1, cbDraw);
}

void LitShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	if (material.Constants.alphaMode == 2) {
		m_pDevice->GetCmdList()->SetPipelineState(m_psoBlend.Get());
	} else if (material.Constants.alphaMode == 1) {
		m_pDevice->GetCmdList()->SetPipelineState(m_psoMask.Get());
	} else {
		m_pDevice->GetCmdList()->SetPipelineState(m_psoOpaque.Get());
	}

	SetMaterial(material);
}

void LitShader::SetMaterial(const Material& material)
{
	int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
	if (b2 != -1) GDF::Instance().BindCBuffer(b2, material.Constants);

	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.BaseColor.handle);
		if (pTex && material.BaseColor.valid && pTex->IsReady()) pTex->Set(t0);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t0);
	}

	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Normal.handle);
		if (pTex && material.Normal.valid && pTex->IsReady()) pTex->Set(t1);
		else GraphicsDevice::Instance().GetNormalTex()->Set(t1);
	}

	int t2 = GetRootParameterIndex(ShaderBindingType::SRV, 2);
	if (t2 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.MetallicRoughness.handle);
		if (pTex && material.MetallicRoughness.valid && pTex->IsReady()) pTex->Set(t2);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t2);
	}

	int t3 = GetRootParameterIndex(ShaderBindingType::SRV, 3);
	if (t3 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Emissive.handle);
		if (pTex && material.Emissive.valid && pTex->IsReady()) pTex->Set(t3);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t3);
	}

	int t4 = GetRootParameterIndex(ShaderBindingType::SRV, 4);
	if (t4 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Occlusion.handle);
		if (pTex && material.Occlusion.valid && pTex->IsReady()) pTex->Set(t4);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t4);
	}
}
