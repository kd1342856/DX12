#pragma once

struct Vertex
{
	Vertex(Math::Vector3 position, Math::Vector2 uv) :Position(position), UV(uv) {};
	Math::Vector3 Position;
	Math::Vector2 UV;
};

class Mesh
{
public:
	void Create(GraphicsDevice* pGraphicsDevice);

	void DrawInstanced()const;

private:
	GraphicsDevice* m_pDevice = nullptr;

	ComPtr<ID3D12Resource>		m_pVBuffer = nullptr;
	ComPtr<ID3D12Resource>		m_pIBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	m_vbView;
	D3D12_INDEX_BUFFER_VIEW		m_ibView;

	std::vector<Vertex> m_vertices;
	std::vector<UINT> m_indices;
};
