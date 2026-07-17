#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "Mesh.h"
#include "../../Device/ResourceUploader.h"

void Mesh::Create(GraphicsDevice* pGraphicsDevice, const std::vector<MeshVertex>& vertices,
	const std::vector<MeshFace>& faces, const Material& material)
{
	m_pDevice = pGraphicsDevice;
	m_material = material;
	m_vertices = vertices;
	m_faces = faces;

	if (static_cast<int>(vertices.size()) == 0)
	{
		assert(0 && "’¸“_‚ª0ŒÂ‚Å‚·");
		return;
	}

	m_instanceCount = static_cast<UINT>(faces.size() * 3);

	pGraphicsDevice->GetResourceUploader()->Begin();
	m_vertexBuffer.Create(
		pGraphicsDevice,
		sizeof(MeshVertex),
		static_cast<UINT>(vertices.size()),
		nullptr);

	m_indexBuffer.Create(
		pGraphicsDevice,
		static_cast<UINT>(faces.size() * 3),
		nullptr);

	pGraphicsDevice->GetResourceUploader()->UploadVertexBuffer(
		&m_vertexBuffer,
		vertices.data(),
		sizeof(MeshVertex) * static_cast<UINT>(vertices.size()));

	pGraphicsDevice->GetResourceUploader()->UploadIndexBuffer(
		&m_indexBuffer,
		faces.data(),
		sizeof(MeshFace) * static_cast<UINT>(faces.size()));

	pGraphicsDevice->GetResourceUploader()->End();
}

void Mesh::DrawInstanced(UINT vertexCount)const
{
	OutputDebugStringA("DrawMesh\n");
	std::string name = m_material.Name.empty() ? "Unnamed Mesh" : m_material.Name;
	Profiler::Instance().AddDrawCall(name, 1);
	
	D3D12_VERTEX_BUFFER_VIEW vbView = m_vertexBuffer.GetView();
	m_pDevice->GetCmdList()->IASetVertexBuffers(0, 1, &vbView);

	D3D12_INDEX_BUFFER_VIEW ibView = m_indexBuffer.GetView();
	m_pDevice->GetCmdList()->IASetIndexBuffer(&ibView);
	
	m_pDevice->GetCmdList()->DrawIndexedInstanced(vertexCount, 1, 0, 0, 0);
}
