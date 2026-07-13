#pragma once
#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"
#include "../../Renderer/RenderContext.h"
#include "../../Renderer/DrawContext.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

class ModelData;
struct Node;
class Mesh;
struct Material;

class GraphicsShader {
public:
	virtual ~GraphicsShader() = default;

	virtual void Create(GraphicsDevice* pDevice) = 0;

	virtual void Begin(RenderContext& context)
	{
		auto* cmd = m_pDevice->GetCmdList();
		cmd->SetPipelineState(m_pipeline->GetPipeline());
		cmd->SetGraphicsRootSignature(m_rootSignature->GetRootSignature());
		
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(m_pipeline->GetTopologyType());
		switch (topologyType) {
		case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST); break;
		case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST); break;
		case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); break;
		case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH: cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST); break;
		}
	}

	virtual void BeginModel(const ModelData& model, const DrawContext& drawContext) {}
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) {}
	virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) {}
	virtual void EndModel() {}

	virtual void End() {}

protected:
	GraphicsDevice* m_pDevice = nullptr;
	std::unique_ptr<Pipeline> m_pipeline;
	std::unique_ptr<RootSignature> m_rootSignature;
};
