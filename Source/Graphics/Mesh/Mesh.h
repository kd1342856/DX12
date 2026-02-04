#pragma once
#include "MeshData/MeshData.h"

struct MeshFace
{
	UINT Idx[3];
};
struct Material
{
	std::string Name;
	std::shared_ptr<Texture>	spBaseColorTex;
	Math::Vector4 BaseColor = { 1,1,1,1 };

	std::shared_ptr<Texture> spMetallicRoughnessTex;
	float Metallic = 0.0f;
	float Roughness = 1.0f;

	std::shared_ptr<Texture> spEmissiveTex;
	Math::Vector3 Emissive = { 0,0,0 };

	std::shared_ptr<Texture> spNormalTex;
};

class Mesh
{
public:
	void Create(GraphicsDevice* pGraphicsDevice, const std::vector<MeshVertex>& vertices,
		const std::vector<MeshFace>& faces, const Material& material);

	void DrawInstanced(UINT vertexCount)const;

	const Material& GetMaterial()const { return m_material; }
	UINT GetInstanceCount()const { return m_instanceCount; }
private:
	GraphicsDevice* m_pDevice = nullptr;

	ComPtr<ID3D12Resource>		m_pVBuffer = nullptr;
	ComPtr<ID3D12Resource>		m_pIBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	m_vbView;
	D3D12_INDEX_BUFFER_VIEW		m_ibView;

	UINT m_instanceCount;
	Material m_material;
};
