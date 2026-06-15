#pragma once
#include <assimp/matrix4x4.h>
#include <atomic>

class Mesh;
struct AnimationData;
class ModelData
{
public:
	struct Node
	{
		std::string							name;
		aiMatrix4x4							localTransform;
		aiMatrix4x4							originalLocalTransform;
		int									parentIndex = -1;
		std::vector<int>					children;
		std::vector<std::shared_ptr<Mesh>>	meshes;
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

private:
	std::atomic<bool> m_isLoaded{false};
	std::vector<Node> m_nodes;
	std::vector<BoneInfo> m_bones;
	std::unordered_map<std::string, int> m_boneNameToIndex;
	std::vector<AnimationData> m_animations;
};