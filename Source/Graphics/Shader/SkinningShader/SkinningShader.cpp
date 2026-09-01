#include "../../../Pch.h"
#include "SkinningShader.h"

#include "../../Renderer/ModelRenderer.h"
#include "../../GDF/GDF.h"
#include "../../../Framework/Manager/Asset/TextureManager.h"

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
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	desc.CullMode = CullMode::None; // ”½ŽË‚Ì‹¾‘œ•`‰æ‚ÅƒJƒŠƒ“ƒO‚ª”½“]‚µ‚Ä‚à•`‰æ‚³‚ê‚é‚æ‚¤‚ÉNone‚É‚·‚é
	
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
	
	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);
}

void SkinningShader::BeginModel(const ModelData& model, const DrawContext& drawContext)
{
	m_pCurrentDrawContext = &drawContext;

	if (drawContext.BoneMatrices) {
		CBufferData::Bones cbBones = {};
		for (size_t i = 0; i < drawContext.BoneMatrices->size() && i < 256; ++i) {
			cbBones.mBones[i] = (*drawContext.BoneMatrices)[i];
		}
		
		int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
		if (b2 != -1) GDF::Instance().BindCBuffer(b2, cbBones);
	}
}

void SkinningShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw; 
	cbDraw.mWorld = nodeWorld; 
	
	int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
	if (b1 != -1) GDF::Instance().BindCBuffer(b1, cbDraw);
}

void SkinningShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void SkinningShader::SetMaterial(const Material& material)
{
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
		else GraphicsDevice::Instance().GetBlackTex()->Set(t3);
	}

	// SkinningShader.hlsl might not have t4 for occlusion or CBV 2 for MaterialConstants yet,
	// but I'll add them if they are in the root parameter index list for consistency.
	int t4 = GetRootParameterIndex(ShaderBindingType::SRV, 4);
	if (t4 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Occlusion.handle);
		if (pTex && material.Occlusion.valid && pTex->IsReady()) pTex->Set(t4);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t4);
	}

	int b3 = GetRootParameterIndex(ShaderBindingType::CBV, 3); // Check bindings! In LitShader it was b2. In Skinning b2 is Bones. Wait, let's just bind if b3 exists.
	if (b3 != -1) GDF::Instance().BindCBuffer(b3, material.Constants);
}

void SkinningShader::BeginShadow(RenderContext& context)
{
	if (m_pShadowPipelineState) m_pDevice->GetCmdList()->SetPipelineState(m_pShadowPipelineState);
	if (m_pProgram && m_pProgram->pRootSignature) m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 4096.0f; // ShadowMap‚Ì‰ð‘œ“x
	viewport.Height = 4096.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 4096;
	rect.bottom = 4096;
	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);
}


