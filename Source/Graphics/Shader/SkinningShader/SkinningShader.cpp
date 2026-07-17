#include "../../../Pch.h"
#include "SkinningShader.h"

#include "../../Renderer/ModelRenderer.h"
#include "../../GDF/GDF.h"

void SkinningShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/SkinningShader/SkinningShader_VS.hlsl", L"Asset/Shader/SkinningShader/SkinningShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL,
		InputLayout::COLOR, InputLayout::TANGENT,
		InputLayout::SKININDEX, InputLayout::SKINWEIGHT
	};
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);

	PipelineDesc shadowDesc = desc;
	// Shadow pipeline (depth only)
	shadowDesc.Formats = {};
	shadowDesc.CullMode = CullMode::None;
	shadowDesc.pBlobs = { m_pProgram->pVS.Get(), nullptr, nullptr, nullptr, nullptr };
	// RootSignature will be overriden inside GetPipelineState
	m_pShadowPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, shadowDesc);
}

void SkinningShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	context.BindCamera(0);
}

void SkinningShader::BeginModel(const ModelData& model, const DrawContext& drawContext)
{
	m_pCurrentDrawContext = &drawContext;

	if (drawContext.BoneMatrices) {
		CBufferData::Bones cbBones;
		for (size_t i = 0; i < drawContext.BoneMatrices->size() && i < 256; ++i) {
			cbBones.mBones[i] = (*drawContext.BoneMatrices)[i];
		}
		GDF::Instance().BindCBuffer(2, cbBones);
	}
}

void SkinningShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw; 
	cbDraw.mWorld = nodeWorld; 
	GDF::Instance().BindCBuffer(1, cbDraw);
}

void SkinningShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void SkinningShader::SetMaterial(const Material& material)
{
	if (material.spBaseColorTex) material.spBaseColorTex->Set(4);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(4);

	if (material.spNormalTex) material.spNormalTex->Set(4 + 1);
	else GraphicsDevice::Instance().GetNormalTex()->Set(4 + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(4 + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(4 + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(4 + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(4 + 3);
}

void SkinningShader::BeginShadow(RenderContext& context)
{
	if (m_pShadowPipelineState) m_pDevice->GetCmdList()->SetPipelineState(m_pShadowPipelineState);
	if (m_pProgram && m_pProgram->pRootSignature) m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 4096.0f; // ShadowMapの解像度
	viewport.Height = 4096.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 4096;
	rect.bottom = 4096;
	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	context.BindCamera(0);
}


