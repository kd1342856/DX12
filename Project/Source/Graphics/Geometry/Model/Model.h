#pragma once

#include <unordered_set>
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

	// モデル内の全メッシュを合算したバインドポーズのバウンズ。各メッシュ自身のローカル
	// AABBと同じ空間(つまり「node.animDeltaTransform * entityWorld」がワールドへ写す先 -
	// Mesh::GetLocalAABB()参照)。メッシュ単位のカリングに進む前に、エンティティ全体を
	// フラスタムカリングするのに使う。参照している全メッシュのGPUデータ(とローカルAABB)が
	// 揃った時点で遅延計算・キャッシュされる。
	// いずれかのメッシュがまだ準備できていない場合はfalseを返す(outBoundsは変更しない) -
	// 呼び出し側は、誤ったボックスで推測するより「カリングせずそのまま描く」扱いにすること。
	bool TryGetLocalBounds(DirectX::BoundingBox& outBounds) const;

	// このノードを対象とするアニメーションチャンネルがあるか、つまり動く可能性がある
	// ノード(開閉するドア等)かどうか。モデルルートに対して常に固定なノードとは区別する。
	// ルームカリング(RoomVisibilityManager)はこれを特別扱いする: 初回問い合わせ時に
	// メッシュの部屋割り当てを永久にキャッシュしてしまうため、ワールド位置が変化するもの
	// には不適切 - なので動くノードはルームカリングの対象外にしている。
	bool IsNodeAnimated(const std::string& nodeName) const;

	// 全ノードを合計したメッシュ数。ルームカリングにリスクを冒す価値があるのは、家の
	// ように多くの部屋にまたがる何十個ものメッシュを持つモデルだけ(部屋単位で除外できれば
	// 実際にドローコールを削減できる) - ピックアップアイテム1メッシュのような小物では
	// 恩恵がほぼ無い上に、部屋割り当ての境界/パディングの問題(RoomVisibilityManager参照)の
	// 影響を受けやすいので、呼び出し側はある閾値未満ならルームカリングをスキップして
	// フラスタムカリングのみに頼ること。
	int GetTotalMeshCount() const;

private:
	std::atomic<bool> m_isLoaded{false};
	std::vector<Node> m_nodes;
	std::vector<BoneInfo> m_bones;
	std::unordered_map<std::string, int> m_boneNameToIndex;
	std::vector<AnimationData> m_animations;

	mutable bool m_localBoundsComputed = false;
	mutable DirectX::BoundingBox m_localBounds;

	mutable bool m_animatedNodeNamesBuilt = false;
	mutable std::unordered_set<std::string> m_animatedNodeNames;

	mutable bool m_totalMeshCountComputed = false;
	mutable int m_totalMeshCount = 0;
};