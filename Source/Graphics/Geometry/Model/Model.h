#pragma once

#include "../../../Framework/Manager/Asset/AssetHandle.h"
class Mesh;
struct AnimationData;
class ModelData
{
public:
	struct Node
	{
		std::string							name;
		Math::Matrix						localTransform;
		Math::Matrix						originalLocalTransform;
		Math::Matrix						globalTransform;			// current global (recomputed each anim update)
		Math::Matrix						originalGlobalTransform;	// global at load time (never changes)
		Math::Matrix						animDeltaTransform;			// delta from original: Identity if not animated
		int									parentIndex = -1;
		std::vector<int>					children;
		std::vector<AssetHandle<Mesh>>		meshes;
	};

	struct BoneInfo
	{
		Math::Matrix offsetMatrix;
		int			nodeIndex = -1;
	};



	bool Load(const std::string& filepath);
	bool IsLoaded() const { return m_isLoaded; }
	void SetLoaded(bool loaded) { m_isLoaded = loaded; }

	const std::vector<Node>& GetNodes() const { return m_nodes; }
	const std::vector<BoneInfo>& GetBones() const { return m_bones; }
	const std::unordered_map<std::string, int>& GetBoneMap() const { return m_boneNameToIndex; }

	std::vector<Math::Matrix> GetBoneMatrices() const;
	std::vector<Node>& GetNodesRef() { return m_nodes; }
	std::vector<BoneInfo>& GetBonesRef() { return m_bones; }
	std::unordered_map<std::string, int>& GetBoneMapRef() { return m_boneNameToIndex; }

	void UpdateAnimation(int animationIndex, float ticks);
	void ResetAnimation();

	const std::vector<AnimationData>& GetAnimations() const { return m_animations; }
	std::vector<AnimationData>& GetAnimationsRef() { return m_animations; }

	// Aggregate bind-pose bounds across every mesh in the model, in the same space as
	// each mesh's own local AABB (i.e. what "node.animDeltaTransform * entityWorld" maps
	// to the world - see Mesh::GetLocalAABB()). Used for frustum-culling the whole entity
	// before descending to per-mesh culling. Computed lazily and cached once every
	// referenced mesh has its GPU data (and local AABB) ready.
	// Returns false (and leaves outBounds untouched) if any mesh isn't ready yet - callers
	// should treat that as "don't cull, just draw it" rather than guess a wrong box.
	bool TryGetLocalBounds(DirectX::BoundingBox& outBounds) const;

private:
	std::atomic<bool> m_isLoaded{false};
	std::vector<Node> m_nodes;
	std::vector<BoneInfo> m_bones;
	std::unordered_map<std::string, int> m_boneNameToIndex;
	std::vector<AnimationData> m_animations;

	mutable bool m_localBoundsComputed = false;
	mutable DirectX::BoundingBox m_localBounds;
};