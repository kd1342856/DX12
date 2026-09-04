#include "../../../Pch.h"

#include "../../../Framework/Manager/Asset/AssetManager.h"
#include "../../../Framework/Manager/Asset/LoadModelOption.h"
#include "../../../Framework/Manager/Asset/MeshManager.h"
#include "../../../Framework/DirectX/Utility/Logger.h"
#include "../../Geometry/Mesh/Mesh.h"


bool ModelData::Load(const std::string& filepath)
{
	LoadModelOption option;
	if (!AssetManager::Instance().LoadModel(filepath, option, this))
	{
		OutputDebugStringA(("���f���̃��[�h�Ɏ��s���܂���: " + filepath + "\n").c_str());
		return false;
	}
	SetLoaded(true);
	return true;
}

bool ModelData::TryGetLocalBounds(DirectX::BoundingBox& outBounds) const
{
	if (m_localBoundsComputed)
	{
		outBounds = m_localBounds;
		return true;
	}

	bool any = false;
	DirectX::BoundingBox merged;
	for (const auto& node : m_nodes)
	{
		for (const auto& meshHandle : node.meshes)
		{
			Mesh* pMesh = MeshManager::Instance().Get(meshHandle);
			if (!pMesh || !pMesh->IsReady()) return false; // まだ準備できていないメッシュがある - 次のフレームで再挑戦

			const DirectX::BoundingBox& meshBounds = pMesh->GetLocalAABB();
			if (!any)
			{
				merged = meshBounds;
				any = true;
			}
			else
			{
				DirectX::BoundingBox::CreateMerged(merged, merged, meshBounds);
			}
		}
	}

	if (!any) return false; // メッシュが(まだ)1つも無い - カリングの判定対象が無い

	m_localBounds = merged;
	m_localBoundsComputed = true;
	outBounds = m_localBounds;
	return true;
}

int ModelData::GetTotalMeshCount() const
{
	if (!m_totalMeshCountComputed)
	{
		m_totalMeshCount = 0;
		for (const auto& node : m_nodes) m_totalMeshCount += static_cast<int>(node.meshes.size());
		m_totalMeshCountComputed = true;
	}
	return m_totalMeshCount;
}

bool ModelData::IsNodeAnimated(const std::string& nodeName) const
{
	if (!m_animatedNodeNamesBuilt)
	{
		for (const auto& anim : m_animations)
		{
			for (const auto& channel : anim.channels)
			{
				m_animatedNodeNames.insert(channel.nodeName);
			}
		}
		m_animatedNodeNamesBuilt = true;
	}
	return m_animatedNodeNames.count(nodeName) != 0;
}

std::vector<Math::Matrix> ModelData::GetBoneMatrices() const
{
	std::vector<Math::Matrix> globalTransforms(m_nodes.size());
	for (int i = 0; i < (int)m_nodes.size(); ++i)
	{
		Math::Matrix localMat = m_nodes[i].localTransform;

		if (m_nodes[i].parentIndex == -1)
		{
			globalTransforms[i] = localMat;
		}
		else
		{
			globalTransforms[i] = localMat * globalTransforms[m_nodes[i].parentIndex];
		}
	}
	// Math::Matrix globalInverseRoot = Math::Matrix::Identity;
	// if (!m_nodes.empty()) {
	// 	globalInverseRoot = globalTransforms[0].Invert();
	// }

	std::vector<Math::Matrix> boneMatrices(m_bones.size());
	for (int i = 0; i < (int)m_bones.size(); ++i)
	{
		int nodeIndex = m_bones[i].nodeIndex;
		if (nodeIndex != -1)
		{
			boneMatrices[i] = m_bones[i].offsetMatrix * globalTransforms[nodeIndex];
			
			// Debug log for Bone 0 (Raw Matrix 16 elements)
			if (i == 0) {
				static bool s_logged = false;
				if (!s_logged) {
					s_logged = true;
					const auto& offset = m_bones[i].offsetMatrix;
					const auto& global = globalTransforms[nodeIndex];
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "Bone 0 Offset Raw:");
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", offset._11, offset._12, offset._13, offset._14);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", offset._21, offset._22, offset._23, offset._24);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", offset._31, offset._32, offset._33, offset._34);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", offset._41, offset._42, offset._43, offset._44);
					//
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "Bone 0 Global Raw:");
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", global._11, global._12, global._13, global._14);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", global._21, global._22, global._23, global._24);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", global._31, global._32, global._33, global._34);
					//Logger::Instance().AddLog(Logger::LogLevel::Info, "[%f, %f, %f, %f]", global._41, global._42, global._43, global._44);
				}
			}
		}
		else
		{
			boneMatrices[i] = Math::Matrix::Identity;
		}
	}

	// Debug log for all bones' scales
	static bool s_allBonesLogged = false;
	if (!s_allBonesLogged && !boneMatrices.empty()) {
		s_allBonesLogged = true;
		Logger::Instance().AddLog(Logger::LogLevel::Info, "--- All Bones Final Scales ---");
		for (size_t i = 0; i < boneMatrices.size(); ++i) {
			Math::Vector3 s, t;
			Math::Quaternion q;
			// boneMatrices�͂��̂܂�Decompose�ł���͂������ADirectXMath�(Row-Major)�Ȃ̂ł��̂܂ܓn��
			boneMatrices[i].Decompose(s, q, t);
			//Logger::Instance().AddLog(Logger::LogLevel::Info, "Bone %zu: Scale(%.3f, %.3f, %.3f) Trans(%.3f, %.3f, %.3f)", i, s.x, s.y, s.z, t.x, t.y, t.z);
		}
		Logger::Instance().AddLog(Logger::LogLevel::Info, "------------------------------");
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
			if (m_nodes[i].name == channel.nodeName) {
				targetNodeIdx = i;
				break;
			}
		}
		if (targetNodeIdx == -1) 
		{
			// char msg[256];
			// sprintf_s(msg, "[Model] Anim node '%s' not found in model!", channel.nodeName.c_str());
			// Logger::Instance().AddLog(Logger::LogLevel::Warning, msg);
			continue;
		}

		// ���̃��[�J���ό`���珉���l���擾���Ă���
		Math::Vector3 defScale, defPos;
		Math::Quaternion defRot;
		m_nodes[targetNodeIdx].originalLocalTransform.Decompose(defScale, defRot, defPos);

		// 1. �ʒu(Position)�̕��
		Math::Vector3 finalPos = defPos;
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

		// 2. ��](Rotation)�̕��
		Math::Quaternion finalRot = defRot;
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

		// 3. �X�P�[��(Scale)�̕��
		Math::Vector3 finalScale = defScale;
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
		m_nodes[targetNodeIdx].localTransform = matSRT;
	}

	// Recompute globalTransform for all nodes after local transforms have been updated
	// Nodes are stored in parent->child order, so iterating 0..N gives correct parent ref
	for (int i = 0; i < (int)m_nodes.size(); ++i)
	{
		if (m_nodes[i].parentIndex == -1)
		{
			m_nodes[i].globalTransform = m_nodes[i].localTransform;
		}
		else
		{
			m_nodes[i].globalTransform = m_nodes[i].localTransform * m_nodes[m_nodes[i].parentIndex].globalTransform;
		}

		// animDeltaTransform = originalGlobal^-1 * newGlobal
		// For non-animated nodes: newGlobal == originalGlobal -> delta = Identity (no displacement)
		// For animated door nodes: delta = the rotation/movement relative to the original pose
		m_nodes[i].animDeltaTransform = m_nodes[i].originalGlobalTransform.Invert() * m_nodes[i].globalTransform;
	}
}

void ModelData::ResetAnimation()
{
	for (auto& node : m_nodes)
	{
		node.localTransform = node.originalLocalTransform;
		node.globalTransform = node.originalGlobalTransform;
		node.animDeltaTransform = Math::Matrix::Identity;
	}
}
