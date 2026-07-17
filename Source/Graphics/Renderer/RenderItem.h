#pragma once
#include <memory>
// Math namespace is provided via Pch.h or other globally included headers in this framework.

class Mesh;
class Material;

struct RenderItem
{
	Mesh* mesh;
	Material* material;
	Math::Matrix world;
	uint64_t sortKey = 0;

	// Helper to generate a sort key based on Pipeline/Material/Mesh IDs
	void MakeSortKey(uint64_t pipelineID, uint64_t materialID, uint64_t meshID)
	{
		uint64_t key = 0;
		key |= (pipelineID & 0xFFFF) << 48; // 16 bits for pipeline
		key |= (materialID & 0xFFFF) << 32; // 16 bits for material
		key |= (meshID & 0xFFFFFFFF) << 0;  // 32 bits for mesh
		sortKey = key;
	}
};
