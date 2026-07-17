#pragma once
#include "MeshData/MeshData.h"
#include "../../GPUResource/Buffer/VertexBuffer.h"
#include "../../GPUResource/Buffer/IndexBuffer.h"

class Texture;

struct MeshFace
{
	UINT Idx[3];
};
#include "../../GPUResource/Material/Material.h"

class Mesh
{
public:
	void Create(GraphicsDevice* pGraphicsDevice, const std::vector<MeshVertex>& vertices,
		const std::vector<MeshFace>& faces, const Material& material);

	void DrawInstanced(UINT vertexCount)const;

	const Material& GetMaterial()const { return m_material; }
	UINT GetInstanceCount()const { return m_instanceCount; }

	const std::vector<MeshVertex>& GetVertices() const { return m_vertices; }
	const std::vector<MeshFace>& GetFaces() const { return m_faces; }
private:
	GraphicsDevice* m_pDevice = nullptr;

	std::vector<MeshVertex> m_vertices;
	std::vector<MeshFace> m_faces;

	VertexBuffer m_vertexBuffer;
	IndexBuffer m_indexBuffer;

	UINT m_instanceCount;
	Material m_material;
};
