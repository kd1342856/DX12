#pragma once
#include "MeshData/MeshData.h"
#include "../../GPUResource/Buffer/VertexBuffer.h"
#include "../../GPUResource/Buffer/IndexBuffer.h"
// AssetState のみ必要（GPUUploadQueue.hを直接インクルードすると循環参照になる）
#include "../../../Framework/Manager/Asset/AssetState.h"

class Texture;

struct MeshFace
{
	UINT Idx[3];
};
#include "../../GPUResource/Material/Material.h"

// ============================================================
// Mesh
// GPU バッファの生成は CreateGPU() でメインスレッドのみ実行する
// Begin()/End() は GPUUploadQueue::Process() が外側で管理するため
// CreateGPU() 内では呼ばない
// ============================================================
class Mesh
{
public:
	// メインスレッド専用：CPUデータ → GPU バッファ生成＋コマンド記録
	// Begin()/End() は外側の GPUUploadQueue::Process() が管理する
	void CreateGPU(GraphicsDevice* pDevice,
		const std::vector<MeshVertex>& vertices,
		const std::vector<MeshFace>&   faces,
		const Material&                material);

	void DrawInstanced(UINT vertexCount) const;

	const Material& GetMaterial() const { return m_material; }
	Material& GetMaterialRef() { return m_material; }
	UINT GetInstanceCount() const { return m_instanceCount; }

	const std::vector<MeshVertex>& GetVertices() const { return m_vertices; }
	const std::vector<MeshFace>&   GetFaces()    const { return m_faces; }

	// AssetState：描画前に IsReady() を確認すること
	AssetState GetState() const { return m_state; }
	void SetState(AssetState state) { m_state = state; }
	bool IsReady() const { return m_state == AssetState::Ready; }

private:
	GraphicsDevice* m_pDevice = nullptr;

	std::vector<MeshVertex> m_vertices;
	std::vector<MeshFace>   m_faces;

	VertexBuffer m_vertexBuffer;
	IndexBuffer  m_indexBuffer;

	UINT     m_instanceCount = 0;
	Material m_material;

	AssetState m_state = AssetState::LoadingCPU;
};
