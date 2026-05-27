#include "Model.h"
#include "ModelLoader.h"
#include "../../Framework/ECS/Components/Data/AnimationData.h"
#include <assimp/scene.h>


bool ModelData::Load(const std::string& filepath)
{
	Modeloader modelLoader;
	if (!modelLoader.Load(filepath, this))
	{
		assert(0 && "モデルのロードに失敗しました");
		return false;
	}
	return true;
}

std::vector<Math::Matrix> ModelData::GetBoneMatrices() const
{
	std::vector<Math::Matrix> globalTransforms(m_nodes.size());
	for (int i = 0; i < (int)m_nodes.size(); ++i)
	{
		aiMatrix4x4 aiMat = m_nodes[i].localTransform;
		aiMat.Transpose();
		Math::Matrix localMat = *(Math::Matrix*)&aiMat;

		if (m_nodes[i].parentIndex == -1)
		{
			globalTransforms[i] = localMat;
		}
		else
		{
			globalTransforms[i] = localMat * globalTransforms[m_nodes[i].parentIndex];
		}
	}
	std::vector<Math::Matrix> boneMatrices(m_bones.size());
	for (int i = 0; i < (int)m_bones.size(); ++i)
	{
		int nodeIndex = m_bones[i].nodeIndex;
		if (nodeIndex != -1)
		{
			boneMatrices[i] = m_bones[i].offsetMatrix * globalTransforms[nodeIndex];
		}
		else
		{
			boneMatrices[i] = Math::Matrix::Identity;
		}
	}
	return boneMatrices;
}

void ModelData::UpdateAnimation(int animationIndex, float ticks)
{
	if (animationIndex < 0 || animationIndex >= (int)m_animations.size()) return;
	const AnimationData& anim = m_animations[animationIndex];

	for (const auto& channel : anim.channels)
	{
		int targetNodeIdx = -1;
		for (int i = 0; i < (int)m_nodes.size(); ++i) {
			if (m_nodes[i].name == channel.nodeName) { targetNodeIdx = i; break; }
		}
		if (targetNodeIdx == -1) continue;

		// ★重要修正：キーフレームが無い場合に備え、初期姿勢(0.01のスケール等)を分解してデフォルト値にする
		aiVector3D defScale, defPos;
		aiQuaternion defRot;
		m_nodes[targetNodeIdx].originalLocalTransform.Decompose(defScale, defRot, defPos);

		// 1. 位置(Position)の補間
		Math::Vector3 finalPos = Math::Vector3(defPos.x, defPos.y, defPos.z);
		if (!channel.positionKeys.empty()) {
			if (channel.positionKeys.size() == 1 || ticks <= channel.positionKeys[0].time) {
				finalPos = channel.positionKeys[0].value;
			}
			else if (ticks >= channel.positionKeys.back().time) {
				finalPos = channel.positionKeys.back().value;
			}
			else {
				UINT idx = 0;
				for (UINT i = 0; i < channel.positionKeys.size() - 1; ++i) {
					if (ticks < channel.positionKeys[i + 1].time) { idx = i; break; }
				}
				const auto& k1 = channel.positionKeys[idx];
				const auto& k2 = channel.positionKeys[idx + 1];
				float t = (ticks - k1.time) / (k2.time - k1.time);
				finalPos = Math::Vector3::Lerp(k1.value, k2.value, t);
			}
		}

		// 2. 回転(Rotation)の補間
		Math::Quaternion finalRot = Math::Quaternion(defRot.x, defRot.y, defRot.z, defRot.w);
		if (!channel.rotationKeys.empty()) {
			if (channel.rotationKeys.size() == 1 || ticks <= channel.rotationKeys[0].time) {
				finalRot = channel.rotationKeys[0].value;
			}
			else if (ticks >= channel.rotationKeys.back().time) {
				finalRot = channel.rotationKeys.back().value;
			}
			else {
				UINT idx = 0;
				for (UINT i = 0; i < channel.rotationKeys.size() - 1; ++i) {
					if (ticks < channel.rotationKeys[i + 1].time) { idx = i; break; }
				}
				const auto& k1 = channel.rotationKeys[idx];
				const auto& k2 = channel.rotationKeys[idx + 1];
				float t = (ticks - k1.time) / (k2.time - k1.time);
				finalRot = Math::Quaternion::Slerp(k1.value, k2.value, t);
			}
		}

		// 3. スケール(Scale)の補間
		Math::Vector3 finalScale = Math::Vector3(defScale.x, defScale.y, defScale.z);
		if (!channel.scalingKeys.empty()) {
			if (channel.scalingKeys.size() == 1 || ticks <= channel.scalingKeys[0].time) {
				finalScale = channel.scalingKeys[0].value;
			}
			else if (ticks >= channel.scalingKeys.back().time) {
				finalScale = channel.scalingKeys.back().value;
			}
			else {
				UINT idx = 0;
				for (UINT i = 0; i < channel.scalingKeys.size() - 1; ++i) {
					if (ticks < channel.scalingKeys[i + 1].time) { idx = i; break; }
				}
				const auto& k1 = channel.scalingKeys[idx];
				const auto& k2 = channel.scalingKeys[idx + 1];
				float t = (ticks - k1.time) / (k2.time - k1.time);
				finalScale = Math::Vector3::Lerp(k1.value, k2.value, t);
			}
		}

		Math::Matrix matSRT = Math::Matrix::CreateScale(finalScale) * Math::Matrix::CreateFromQuaternion(finalRot) * Math::Matrix::CreateTranslation(finalPos);
		matSRT = matSRT.Transpose();
		m_nodes[targetNodeIdx].localTransform = *(aiMatrix4x4*)&matSRT;
	}
}

void ModelData::ResetAnimation()
{
	for (auto& node : m_nodes)
	{
		node.localTransform = node.originalLocalTransform;
	}
}
