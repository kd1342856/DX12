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

	// Local-space bounding box, computed once in CreateGPU(). Lets mesh-collider checks
	// (CollisionSolver's Mesh case) reject a whole mesh with one box test instead of
	// transforming every vertex of every triangle just to find out none of them are close.
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
