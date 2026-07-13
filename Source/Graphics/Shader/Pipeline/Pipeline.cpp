#include "../../../Pch.h"
#include "Pipeline.h"

void Pipeline::Create(GraphicsDevice* pGraphicsDevice, const PipelineDesc& desc)
{
	m_pDevice = pGraphicsDevice;
	m_topologyType = desc.TopologyType;

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
	SetInputLayout(inputLayouts, desc.InputLayouts);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineState = {};

	if (desc.pBlobs.size() > 0 && desc.pBlobs[0]) {
		graphicsPipelineState.VS.pShaderBytecode = desc.pBlobs[0]->GetBufferPointer();
		graphicsPipelineState.VS.BytecodeLength = desc.pBlobs[0]->GetBufferSize();
	}
	if (desc.pBlobs.size() > 1 && desc.pBlobs[1]) {
		graphicsPipelineState.HS.pShaderBytecode = desc.pBlobs[1]->GetBufferPointer();
		graphicsPipelineState.HS.BytecodeLength = desc.pBlobs[1]->GetBufferSize();
	}
	if (desc.pBlobs.size() > 2 && desc.pBlobs[2]) {
		graphicsPipelineState.DS.pShaderBytecode = desc.pBlobs[2]->GetBufferPointer();
		graphicsPipelineState.DS.BytecodeLength = desc.pBlobs[2]->GetBufferSize();
	}
	if (desc.pBlobs.size() > 3 && desc.pBlobs[3]) {
		graphicsPipelineState.GS.pShaderBytecode = desc.pBlobs[3]->GetBufferPointer();
		graphicsPipelineState.GS.BytecodeLength = desc.pBlobs[3]->GetBufferSize();
	}
	if (desc.pBlobs.size() > 4 && desc.pBlobs[4]) {
		graphicsPipelineState.PS.pShaderBytecode = desc.pBlobs[4]->GetBufferPointer();
		graphicsPipelineState.PS.BytecodeLength = desc.pBlobs[4]->GetBufferSize();
	}

	graphicsPipelineState.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineState.RasterizerState.CullMode = static_cast<D3D12_CULL_MODE>(desc.CullMode);
	graphicsPipelineState.RasterizerState.FillMode = desc.IsWireFrame ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

	if (desc.IsDepth) {
		graphicsPipelineState.RasterizerState.DepthClipEnable = true;
		graphicsPipelineState.DepthStencilState.DepthEnable = true;
		graphicsPipelineState.DepthStencilState.DepthWriteMask = desc.IsDepthMask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		graphicsPipelineState.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		graphicsPipelineState.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	} else {
		graphicsPipelineState.RasterizerState.DepthClipEnable = false;
		graphicsPipelineState.DepthStencilState.DepthEnable = false;
		graphicsPipelineState.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	}
	
	graphicsPipelineState.DepthStencilState.StencilEnable = false;
	graphicsPipelineState.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineState.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineState.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineState.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	graphicsPipelineState.DepthStencilState.BackFace = graphicsPipelineState.DepthStencilState.FrontFace;
	
	graphicsPipelineState.BlendState.AlphaToCoverageEnable = false;
	graphicsPipelineState.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
	SetBlendMode(blendDesc, desc.BlendMode);
	graphicsPipelineState.BlendState.RenderTarget[0] = blendDesc;

	graphicsPipelineState.InputLayout.pInputElementDescs = inputLayouts.data();
	graphicsPipelineState.InputLayout.NumElements = (int)desc.InputLayouts.size();

	bool hasTessellation = (desc.pBlobs.size() > 2 && desc.pBlobs[1] && desc.pBlobs[2]);
	graphicsPipelineState.PrimitiveTopologyType = hasTessellation ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(desc.TopologyType);

	graphicsPipelineState.NumRenderTargets = (int)desc.Formats.size();
	for (int i = 0; i < (int)desc.Formats.size(); ++i) {
		graphicsPipelineState.RTVFormats[i] = desc.Formats[i];
	}

	graphicsPipelineState.SampleDesc.Count = 1;
	if (desc.pRootSignature) {
		graphicsPipelineState.pRootSignature = desc.pRootSignature->GetRootSignature();
	}

	auto hr = m_pDevice->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineState, IID_PPV_ARGS(&m_pPipelineState));
	if (FAILED(hr)) {
		assert(0 && "Failed to create PipelineState");
	}
}

void Pipeline::SetInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements, const std::vector<InputLayout>& inputLayouts)
{
	for (int i = 0; i < (int)inputLayouts.size(); ++i) {
		if (inputLayouts[i] == InputLayout::POSITION)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::TEXCOORD)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::NORMAL)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::COLOR)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::TANGENT)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::SKININDEX)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "SKININDEX", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		else if (inputLayouts[i] == InputLayout::SKINWEIGHT)
			inputElements.emplace_back(D3D12_INPUT_ELEMENT_DESC{ "SKINWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}
}

void Pipeline::SetBlendMode(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc, BlendMode blendMode)
{
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.SrcBlend = D3D12_BLEND_ONE;
	blendDesc.DestBlend = D3D12_BLEND_ZERO;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;

	if (blendMode == BlendMode::None) {
		blendDesc.BlendEnable = false;
		return;
	}
	blendDesc.BlendEnable = true;

	switch (blendMode) {
	case BlendMode::Add:
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case BlendMode::Alpha:
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	default:
		break;
	}
}