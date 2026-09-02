#include "../../../Pch.h"
#include "GltfImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#include "../../../Graphics/Geometry/Model/Model.h"
#include "AssetManager.h"
#include "MaterialManager.h"

inline Math::Matrix ConvertMatrix(const aiMatrix4x4& m)
{
	Math::Matrix mat;
	mat._11 = m.a1; mat._12 = m.b1; mat._13 = m.c1; mat._14 = m.d1;
	mat._21 = m.a2; mat._22 = m.b2; mat._23 = m.c2; mat._24 = m.d2;
	mat._31 = m.a3; mat._32 = m.b3; mat._33 = m.c3; mat._34 = m.d3;
	mat._41 = m.a4; mat._42 = m.b4; mat._43 = m.c4; mat._44 = m.d4;
	return mat;
}

// Forward declaration of internal helpers
static void ProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx, ModelData* pModelData, const AssetImportContext& context, const LoadModelOption& option);
static AssetHandle<Mesh> ParseMesh(const aiScene* pScene, const aiMesh* pMesh, const aiMaterial* pMaterial, const AssetImportContext& context, const aiMatrix4x4& transform, ModelData* pModelData);
static AssetHandle<Material> ParseMaterial(const aiMaterial* pMaterial, const AssetImportContext& context);

bool GltfImporter::ImportModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData)
{
	unsigned int pFlags = aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights;
	if (option.generateTangent) pFlags |= aiProcess_CalcTangentSpace;
	if (option.optimizeMesh) pFlags |= aiProcess_OptimizeMeshes;
	if (option.mergeMeshes) pFlags |= aiProcess_OptimizeGraph;

	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(filepath, pFlags);

	if (!pScene || !pScene->mRootNode)
	{
		std::string errorStr = importer.GetErrorString();
		OutputDebugStringA(("Failed to load model file: " + filepath + " Error: " + errorStr + "\n").c_str());
		Logger::Instance().AddLog(Logger::LogLevel::Error, "Assimp Error: " + errorStr);
		return false;
	}

	AssetImportContext context;
	context.modelPath = filepath;
	context.rootDirectory = std::filesystem::path(filepath).parent_path();
	context.pScene = pScene;

	ProcessNode(pScene->mRootNode, pScene, -1, pOutModelData, context, option);

	// Map bones to nodes
	auto& bones = pOutModelData->GetBonesRef();
	auto& nodes = pOutModelData->GetNodes();
	auto& boneMap = pOutModelData->GetBoneMapRef();
	
	for(auto& pair : boneMap)
	{
		std::string nodeName = pair.first;
		int bIdx = pair.second;
		
		for(int i = 0; i < (int)nodes.size(); ++i) {
			if(nodes[i].name == nodeName) {
				bones[bIdx].nodeIndex = i;
				break;
			}
		}
	}

	// Load animations
	if (option.loadAnimation && pScene->HasAnimations()) {
		for (unsigned int i = 0; i < pScene->mNumAnimations; ++i) {
			aiAnimation* pAnim = pScene->mAnimations[i];
			AnimationData animData;
			animData.name = pAnim->mName.C_Str();
			
			// AssimpのmTicksPerSecondが0または極小値の場合の保護
			float tps = (float)pAnim->mTicksPerSecond;
			if (tps < 0.001f) {
				tps = 25.0f; // Default ticks per second
			}
			animData.ticksPerSecond = tps;
			animData.duration = (float)pAnim->mDuration;

			//char msg[256];
			//sprintf_s(msg, "[GltfImporter] Anim='%s' Duration=%.2f TicksPerSec=%.2f", animData.name.c_str(), animData.duration, animData.ticksPerSecond);
			//Logger::Instance().AddLog(Logger::LogLevel::Info, msg);

			for (unsigned int c = 0; c < pAnim->mNumChannels; ++c) {
				aiNodeAnim* pChannel = pAnim->mChannels[c];
				AnimationChannel channelData;
				channelData.nodeName = pChannel->mNodeName.C_Str();

				for (unsigned int p = 0; p < pChannel->mNumPositionKeys; ++p) {
					AnimationKeyVector key;
					key.time = (float)pChannel->mPositionKeys[p].mTime;
					key.value = Math::Vector3(pChannel->mPositionKeys[p].mValue.x, pChannel->mPositionKeys[p].mValue.y, pChannel->mPositionKeys[p].mValue.z);
					channelData.positionKeys.push_back(key);
				}
				for (unsigned int r = 0; r < pChannel->mNumRotationKeys; ++r) {
					AnimationKeyQuaternion key;
					key.time = (float)pChannel->mRotationKeys[r].mTime;
					// Assimp quaternion is (w, x, y, z) but Math::Quaternion is (x, y, z, w) usually
					// aiQuaternion mValue has x, y, z, w
					key.value = Math::Quaternion(pChannel->mRotationKeys[r].mValue.x, pChannel->mRotationKeys[r].mValue.y, pChannel->mRotationKeys[r].mValue.z, pChannel->mRotationKeys[r].mValue.w);
					channelData.rotationKeys.push_back(key);
				}
				for (unsigned int s = 0; s < pChannel->mNumScalingKeys; ++s) {
					AnimationKeyVector key;
					key.time = (float)pChannel->mScalingKeys[s].mTime;
					key.value = Math::Vector3(pChannel->mScalingKeys[s].mValue.x, pChannel->mScalingKeys[s].mValue.y, pChannel->mScalingKeys[s].mValue.z);
					channelData.scalingKeys.push_back(key);
				}
				animData.channels.push_back(channelData);
			}
			pOutModelData->GetAnimationsRef().push_back(animData);
		}
	}

	// Compute globalTransform for each node (parent -> child order) after load
	{
		auto& initNodes = pOutModelData->GetNodesRef();
		for (int i = 0; i < (int)initNodes.size(); ++i)
		{
			if (initNodes[i].parentIndex == -1)
			{
				initNodes[i].globalTransform = initNodes[i].localTransform;
			}
			else
			{
				initNodes[i].globalTransform = initNodes[i].localTransform * initNodes[initNodes[i].parentIndex].globalTransform;
			}
			// Store as original (never changes after load)
			initNodes[i].originalGlobalTransform = initNodes[i].globalTransform;
			// Delta is Identity at load time (no animation yet)
			initNodes[i].animDeltaTransform = Math::Matrix::Identity;
		}
	}

	return true;
}

static void ProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx, ModelData* pModelData, const AssetImportContext& context, const LoadModelOption& option)
{
	int currentNodeIdx = (int)pModelData->GetNodesRef().size();
	ModelData::Node node;
	node.name = pSrcNode->mName.C_Str();
	node.parentIndex = parentIdx;
	node.localTransform = ConvertMatrix(pSrcNode->mTransformation);

	if (parentIdx >= 0) {
		node.globalTransform = node.localTransform * pModelData->GetNodesRef()[parentIdx].globalTransform;
	} else {
		node.globalTransform = node.localTransform;
	}
	node.originalLocalTransform = node.localTransform;
	node.originalGlobalTransform = node.globalTransform;

	pModelData->GetNodesRef().push_back(node);

	if (parentIdx >= 0) {
		pModelData->GetNodesRef()[parentIdx].children.push_back(currentNodeIdx);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumMeshes; i++) {
		aiMesh* pMesh = pScene->mMeshes[pSrcNode->mMeshes[i]];
		aiMaterial* pMaterial = pScene->mMaterials[pMesh->mMaterialIndex];

		// GLTFの仕様上、静的メッシュの頂点に焼き込むべきはグローバル変換（あるいは親までの累積変換）
		// Convert DX Matrix back to Assimp Matrix to pass to ParseMesh
		aiMatrix4x4 globalAiTransform;
		globalAiTransform.a1 = node.globalTransform._11; globalAiTransform.a2 = node.globalTransform._21; globalAiTransform.a3 = node.globalTransform._31; globalAiTransform.a4 = node.globalTransform._41;
		globalAiTransform.b1 = node.globalTransform._12; globalAiTransform.b2 = node.globalTransform._22; globalAiTransform.b3 = node.globalTransform._32; globalAiTransform.b4 = node.globalTransform._42;
		globalAiTransform.c1 = node.globalTransform._13; globalAiTransform.c2 = node.globalTransform._23; globalAiTransform.c3 = node.globalTransform._33; globalAiTransform.c4 = node.globalTransform._43;
		globalAiTransform.d1 = node.globalTransform._14; globalAiTransform.d2 = node.globalTransform._24; globalAiTransform.d3 = node.globalTransform._34; globalAiTransform.d4 = node.globalTransform._44;

		AssetHandle<Mesh> meshHandle = ParseMesh(pScene, pMesh, pMaterial, context, globalAiTransform, pModelData);
		pModelData->GetNodesRef()[currentNodeIdx].meshes.push_back(meshHandle);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumChildren; i++) {
		ProcessNode(pSrcNode->mChildren[i], pScene, currentNodeIdx, pModelData, context, option);
	}
}

static AssetHandle<Mesh> ParseMesh(const aiScene* pScene, const aiMesh* pMesh, const aiMaterial* pMaterial, const AssetImportContext& context, const aiMatrix4x4& transform, ModelData* pModelData)
{
	AssetMeshData meshData;
	meshData.vertices.resize(pMesh->mNumVertices);

	aiMatrix3x3 normalTransform(transform);
	normalTransform.Inverse();
	normalTransform.Transpose();

	for (unsigned int i = 0; i < pMesh->mNumVertices; i++) {
		aiVector3D pos = pMesh->mVertices[i];
		if (!pMesh->HasBones()) pos *= transform;
		meshData.vertices[i].Position.x = pos.x;
		meshData.vertices[i].Position.y = pos.y;
		meshData.vertices[i].Position.z = pos.z;

		if (pMesh->HasTextureCoords(0)) {
			meshData.vertices[i].UV.x = static_cast<float>(pMesh->mTextureCoords[0][i].x);
			meshData.vertices[i].UV.y = static_cast<float>(pMesh->mTextureCoords[0][i].y);
		}

		aiVector3D normal = pMesh->mNormals[i];
		if (!pMesh->HasBones()) {
			normal *= normalTransform;
			normal.Normalize();
		}
		meshData.vertices[i].Normal.x = normal.x;
		meshData.vertices[i].Normal.y = normal.y;
		meshData.vertices[i].Normal.z = normal.z;

		if (pMesh->HasTangentsAndBitangents()) {
			aiVector3D tangent = pMesh->mTangents[i];
			if (!pMesh->HasBones()) {
				tangent *= normalTransform;
				tangent.Normalize();
			}
			meshData.vertices[i].Tangent.x = tangent.x;
			meshData.vertices[i].Tangent.y = tangent.y;
			meshData.vertices[i].Tangent.z = tangent.z;
		}

		if (pMesh->HasVertexColors(0)) {
			uint8_t r = (uint8_t)(pMesh->mColors[0][i].r * 255.0f);
			uint8_t g = (uint8_t)(pMesh->mColors[0][i].g * 255.0f);
			uint8_t b = (uint8_t)(pMesh->mColors[0][i].b * 255.0f);
			uint8_t a = (uint8_t)(pMesh->mColors[0][i].a * 255.0f);
			meshData.vertices[i].Color = (a << 24) | (b << 16) | (g << 8) | r;
		}
	}

	if (pMesh->HasBones())
	{
		auto& bones = pModelData->GetBonesRef();
		auto& boneMap = pModelData->GetBoneMapRef();
		std::vector<int> boneIndexCounts(pMesh->mNumVertices, 0);

		for (unsigned int i = 0; i < pMesh->mNumBones; ++i)
		{
			aiBone* pBone = pMesh->mBones[i];
			std::string boneName = pBone->mName.C_Str();

			int boneIndex = -1;
			auto it = boneMap.find(boneName);
			if (it == boneMap.end())
			{
				boneIndex = (int)bones.size();
				ModelData::BoneInfo bInfo;
				bInfo.offsetMatrix = ConvertMatrix(pBone->mOffsetMatrix);
				bInfo.nodeIndex = -1; 
				bones.push_back(bInfo);
				boneMap[boneName] = boneIndex;
			}
			else
			{
				boneIndex = it->second;
			}

			for (unsigned int w = 0; w < pBone->mNumWeights; ++w)
			{
				const aiVertexWeight& weight = pBone->mWeights[w];
				unsigned int vertexId = weight.mVertexId;
				float wVal = weight.mWeight;

				if (boneIndexCounts[vertexId] < 4)
				{
					meshData.vertices[vertexId].SkinIndex[boneIndexCounts[vertexId]] = boneIndex;
					meshData.vertices[vertexId].SkinWeight[boneIndexCounts[vertexId]] = wVal;
					boneIndexCounts[vertexId]++;
				}
			}
		}

		// ★Weightの正規化 (Normalize weights to sum to 1.0)
		for (size_t v = 0; v < meshData.vertices.size(); ++v)
		{
			float sum = 0.0f;
			for (int i = 0; i < 4; ++i) {
				sum += meshData.vertices[v].SkinWeight[i];
			}

			if (sum > 0.0f) {
				for (int i = 0; i < 4; ++i) {
					meshData.vertices[v].SkinWeight[i] /= sum;
				}
			}

			// Debug: Log the first vertex of the first mesh that has bones
			static bool s_weightLogged = false;
			if (!s_weightLogged && pMesh->HasBones()) {
				s_weightLogged = true;
				float s = meshData.vertices[v].SkinWeight[0] + meshData.vertices[v].SkinWeight[1] + 
				          meshData.vertices[v].SkinWeight[2] + meshData.vertices[v].SkinWeight[3];
				//Logger::Instance().AddLog(Logger::LogLevel::Info, "First Vertex Skin: Idx(%u, %u, %u, %u) Weight(%f, %f, %f, %f) Sum=%f",
				//	meshData.vertices[v].SkinIndex[0], meshData.vertices[v].SkinIndex[1], meshData.vertices[v].SkinIndex[2], meshData.vertices[v].SkinIndex[3],
				//	meshData.vertices[v].SkinWeight[0], meshData.vertices[v].SkinWeight[1], meshData.vertices[v].SkinWeight[2], meshData.vertices[v].SkinWeight[3],
				//	sum);
			}
		}
	}
	
	for (unsigned int i = 0; i < pMesh->mNumFaces; ++i) {
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[0]);
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[1]);
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[2]);
	}

	// Material
	AssetHandle<Material> matHandle = ParseMaterial(pMaterial, context);
	meshData.materialHandle = matHandle;

	return MeshManager::Instance().CreateMesh(meshData);
}

#include "TextureManager.h"

static void LoadTextureIfPresent(const aiMaterial* pMaterial, aiTextureType type, AssetImportContext& context, MaterialTexture& outTex)
{
	aiString path;
	if (pMaterial->GetTexture(type, 0, &path) == AI_SUCCESS)
	{
		if (path.C_Str()[0] == '*')
		{
			// Embedded texture
			if (context.pScene && context.pScene->HasTextures())
			{
				int textureIndex = atoi(path.C_Str() + 1);
				if (textureIndex >= 0 && textureIndex < (int)context.pScene->mNumTextures)
				{
					aiTexture* pAiTex = context.pScene->mTextures[textureIndex];
					if (pAiTex->mHeight == 0) // Compressed data (PNG/JPG)
					{
						std::string key = context.modelPath.string() + "_embedded_" + std::to_string(textureIndex);
						outTex.handle = TextureManager::Instance().LoadTextureFromMemory(key, pAiTex->pcData, pAiTex->mWidth);
						outTex.valid = outTex.handle.IsValid();
						return;
					}
				}
			}
		}

		std::filesystem::path texPath = context.rootDirectory / path.C_Str();
		outTex.handle = TextureManager::Instance().LoadTexture(texPath.string());
		outTex.valid = outTex.handle.IsValid();
	}
}

static AssetHandle<Material> ParseMaterial(const aiMaterial* pMaterial, const AssetImportContext& context)
{
	AssetMaterialData matData;
	aiString name;
	if (pMaterial->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
		matData.name = name.C_Str();
	}

	// Textures
	LoadTextureIfPresent(pMaterial, aiTextureType_BASE_COLOR, const_cast<AssetImportContext&>(context), matData.baseColor);
	if (!matData.baseColor.valid) LoadTextureIfPresent(pMaterial, aiTextureType_DIFFUSE, const_cast<AssetImportContext&>(context), matData.baseColor);
	
	LoadTextureIfPresent(pMaterial, aiTextureType_NORMALS, const_cast<AssetImportContext&>(context), matData.normal);
	
	LoadTextureIfPresent(pMaterial, aiTextureType_METALNESS, const_cast<AssetImportContext&>(context), matData.metallicRoughness);
	if (!matData.metallicRoughness.valid) LoadTextureIfPresent(pMaterial, aiTextureType_DIFFUSE_ROUGHNESS, const_cast<AssetImportContext&>(context), matData.metallicRoughness);
	if (!matData.metallicRoughness.valid) LoadTextureIfPresent(pMaterial, aiTextureType_UNKNOWN, const_cast<AssetImportContext&>(context), matData.metallicRoughness);

	LoadTextureIfPresent(pMaterial, aiTextureType_LIGHTMAP, const_cast<AssetImportContext&>(context), matData.occlusion); // gltf occlusion is often mapped to lightmap or ambient in assimp
	if (!matData.occlusion.valid) LoadTextureIfPresent(pMaterial, aiTextureType_AMBIENT, const_cast<AssetImportContext&>(context), matData.occlusion);
	
	LoadTextureIfPresent(pMaterial, aiTextureType_EMISSIVE, const_cast<AssetImportContext&>(context), matData.emissive);

	// Factors
	aiColor4D color;
	if (pMaterial->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS) matData.baseColorFactor = { color.r, color.g, color.b, color.a };
	else if (pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) matData.baseColorFactor = { color.r, color.g, color.b, color.a };

	aiColor3D emissive;
	if (pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
		matData.emissiveFactorX = emissive.r;
		matData.emissiveFactorY = emissive.g;
		matData.emissiveFactorZ = emissive.b;
	}
	float metallic;
	if (pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) matData.metallicFactor = metallic;

	float roughness;
	if (pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) matData.roughnessFactor = roughness;

#ifndef AI_MATKEY_GLTF_ALPHAMODE
#define AI_MATKEY_GLTF_ALPHAMODE "$mat.gltf.alphaMode", 0, 0
#endif

	// Alpha Mode
	aiString alphaMode;
	if (pMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
		if (strcmp(alphaMode.C_Str(), "MASK") == 0) matData.alphaMode = 1;
		else if (strcmp(alphaMode.C_Str(), "BLEND") == 0) matData.alphaMode = 2;
		else matData.alphaMode = 0;
	} else {
		// Fallback for names containing typical keywords
		std::string n = matData.name;
		std::transform(n.begin(), n.end(), n.begin(), ::tolower);
		if (n.find("blood") != std::string::npos || n.find("web") != std::string::npos || n.find("decal") != std::string::npos || n.find("blend") != std::string::npos || n.find("glass") != std::string::npos || n.find("window") != std::string::npos) {
			matData.alphaMode = 2;
		} else if (n.find("mask") != std::string::npos) {
			matData.alphaMode = 1;
		}
	}

	// Always detect procedural type from name
	std::string n = matData.name;
	std::transform(n.begin(), n.end(), n.begin(), ::tolower);
	if (n.find("blood") != std::string::npos) {
		matData.proceduralType = 1; // Blood
	} else if (n.find("web") != std::string::npos) {
		matData.proceduralType = 2; // Cobweb
	} else if (n.find("glass") != std::string::npos || n.find("window") != std::string::npos) {
		matData.proceduralType = 3; // Glass
		matData.alphaMode = 2;      // Force blend mode for glass
	}

	//Logger::Instance().AddLog(Logger::LogLevel::Info, "[GltfImporter] Material='%s' AlphaMode=%d Procedural=%d", matData.name.c_str(), matData.alphaMode, matData.proceduralType);

	return MaterialManager::Instance().CreateMaterial(matData);
}
