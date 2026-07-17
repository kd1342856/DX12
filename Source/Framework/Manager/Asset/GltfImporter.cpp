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

// Forward declaration of internal helpers
static void ProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx, ModelData* pModelData, const std::string& dirPath, const LoadModelOption& option);
static AssetHandle<Mesh> ParseMesh(const aiScene* pScene, const aiMesh* pMesh, const aiMaterial* pMaterial, const std::string& dirPath, const aiMatrix4x4& transform, ModelData* pModelData);
static AssetHandle<Material> ParseMaterial(const aiMaterial* pMaterial, const std::string& dirPath);

bool GltfImporter::ImportModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData)
{
	unsigned int pFlags = aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights;
	if (option.generateTangent) pFlags |= aiProcess_CalcTangentSpace;
	if (option.optimizeMesh) pFlags |= aiProcess_OptimizeMeshes;

	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(filepath, pFlags);

	if (pScene == nullptr) {
		assert(0 && "Failed to load model file");
		return false;
	}

	std::string dirPath = "";
	size_t slashPos = filepath.find_last_of('/');
	if (slashPos != std::string::npos) {
		dirPath = filepath.substr(0, slashPos + 1);
	}

	ProcessNode(pScene->mRootNode, pScene, -1, pOutModelData, dirPath, option);

	// Load animations
	if (option.loadAnimation && pScene->HasAnimations()) {
		// TODO: Parse animations
	}

	return true;
}

static void ProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx, ModelData* pModelData, const std::string& dirPath, const LoadModelOption& option)
{
	int currentNodeIdx = (int)pModelData->GetNodesRef().size();
	ModelData::Node node;
	node.name = pSrcNode->mName.C_Str();
	node.parentIndex = parentIdx;
	node.localTransform = pSrcNode->mTransformation;
	node.localTransform.Transpose();

	node.originalLocalTransform = node.localTransform;
	pModelData->GetNodesRef().push_back(node);

	if (parentIdx >= 0) {
		pModelData->GetNodesRef()[parentIdx].children.push_back(currentNodeIdx);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumMeshes; i++) {
		aiMesh* pMesh = pScene->mMeshes[pSrcNode->mMeshes[i]];
		aiMaterial* pMaterial = pScene->mMaterials[pMesh->mMaterialIndex];

		AssetHandle<Mesh> meshHandle = ParseMesh(pScene, pMesh, pMaterial, dirPath, pSrcNode->mTransformation, pModelData);
		pModelData->GetNodesRef()[currentNodeIdx].meshes.push_back(meshHandle);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumChildren; i++) {
		ProcessNode(pSrcNode->mChildren[i], pScene, currentNodeIdx, pModelData, dirPath, option);
	}
}

static AssetHandle<Mesh> ParseMesh(const aiScene* pScene, const aiMesh* pMesh, const aiMaterial* pMaterial, const std::string& dirPath, const aiMatrix4x4& transform, ModelData* pModelData)
{
	AssetMeshData meshData;
	meshData.vertices.resize(pMesh->mNumVertices);

	aiMatrix3x3 normalTransform(transform);
	normalTransform.Inverse();
	normalTransform.Transpose();

	for (unsigned int i = 0; i < pMesh->mNumVertices; i++) {
		aiVector3D pos = pMesh->mVertices[i];
		pos *= transform;
		meshData.vertices[i].Position.x = pos.x;
		meshData.vertices[i].Position.y = pos.y;
		meshData.vertices[i].Position.z = pos.z;

		if (pMesh->HasTextureCoords(0)) {
			meshData.vertices[i].UV.x = static_cast<float>(pMesh->mTextureCoords[0][i].x);
			meshData.vertices[i].UV.y = static_cast<float>(pMesh->mTextureCoords[0][i].y);
		}

		aiVector3D normal = pMesh->mNormals[i];
		normal *= normalTransform;
		normal.Normalize();
		meshData.vertices[i].Normal.x = normal.x;
		meshData.vertices[i].Normal.y = normal.y;
		meshData.vertices[i].Normal.z = normal.z;

		if (pMesh->HasTangentsAndBitangents()) {
			aiVector3D tangent = pMesh->mTangents[i];
			tangent *= normalTransform;
			tangent.Normalize();
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

	// Bone parsing omitted for brevity in this mock implementation.
	
	for (unsigned int i = 0; i < pMesh->mNumFaces; ++i) {
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[0]);
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[1]);
		meshData.indices.push_back(pMesh->mFaces[i].mIndices[2]);
	}

	// Material
	AssetHandle<Material> matHandle = ParseMaterial(pMaterial, dirPath);
	// Pass the material handle inside the MeshData (or associate it somehow, perhaps Material is separate in Mesh)
	// We'll need to extend AssetMeshData if we want it to hold a material, or we can just pass it directly.

	return MeshManager::Instance().CreateMesh(meshData); // NOTE: We also need to supply the MaterialHandle to CreateMesh.
}

static AssetHandle<Material> ParseMaterial(const aiMaterial* pMaterial, const std::string& dirPath)
{
	AssetMaterialData matData;
	// Retrieve textures via TextureManager::Instance().LoadTexture(path)
	// Return MaterialManager::Instance().CreateMaterial(matData)
	return AssetHandle<Material>();
}
