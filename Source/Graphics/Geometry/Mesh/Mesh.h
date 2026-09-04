#pragma once
#include "MeshData/MeshData.h"
#include "../../GPUResource/Buffer/VertexBuffer.h"
#include "../../GPUResource/Buffer/IndexBuffer.h"
// AssetState �̂ݕK�v�iGPUUploadQueue.h�𒼐ڃC���N���[�h����Əz�Q�ƂɂȂ�j
#include "../../../Framework/Manager/Asset/AssetState.h"

class Texture;

struct MeshFace
{
	UINT Idx[3];
};
#include "../../GPUResource/Material/Material.h"

// ============================================================
// Mesh
// GPU �o�b�t�@�̐����� CreateGPU() �Ń��C���X���b�h�̂ݎ��s����
// Begin()/End() �� GPUUploadQueue::Process() ���O���ŊǗ����邽��
// CreateGPU() ���ł͌Ă΂Ȃ�
// ============================================================
class Mesh
{
public:
	// ���C���X���b�h��p�FCPU�f�[�^ �� GPU �o�b�t�@�����{�R�}���h�L�^
	// Begin()/End() �͊O���� GPUUploadQueue::Process() ���Ǘ�����
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

	// ローカル空間のバウンディングボックス、CreateGPU()内で一度だけ計算される。
	// メッシュコライダーの判定(CollisionSolverのMeshケース)で、全三角形の全頂点を
	// 変換して結局どれも近くないと分かるくらいなら、ボックス1回の判定でメッシュ
	// 全体を棄却できるようにする。
	const DirectX::BoundingBox& GetLocalAABB() const { return m_localAABB; }

	// AssetState�F�`��O�� IsReady() ���m�F���邱��
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
	DirectX::BoundingBox m_localAABB;

	AssetState m_state = AssetState::LoadingCPU;
};
