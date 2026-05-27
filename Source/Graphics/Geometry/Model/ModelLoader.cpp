#include "ModelLoader.h"
#include "Model.h"
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

bool Modeloader::Load(std::string filepath, ModelData* pModelData)
{
	Assimp::Importer importer;

	uint32_t flag = aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipWindingOrder |
		aiProcess_LimitBoneWeights;

	const aiScene* pScene = importer.ReadFile(filepath, flag);

	if (pScene == nullptr)
	{
		return false;
	}

	pModelData->GetNodesRef().clear();
	pModelData->GetBonesRef().clear();
	pModelData->GetBoneMapRef().clear();
	std::string dirPath = GetDirFromPath(filepath);

	InternalProcessNode(pScene->mRootNode, pScene, -1, pModelData, dirPath);

	// ボーンとノードのインデックスを紐付け
	for (auto& bone : pModelData->GetBonesRef())
	{
		std::string boneName = "";
		for (auto& pair : pModelData->GetBoneMapRef())
		{
			if (pair.second == (&bone - &pModelData->GetBonesRef()[0]))
			{
				boneName = pair.first;
				break;
			}
		}

		for (int i = 0; i < (int)pModelData->GetNodes().size(); ++i)
		{
			if (pModelData->GetNodes()[i].name == boneName)
			{
				bone.nodeIndex = i;
				break;
			}
		}
	}

	// ---------------------------------------------------------
	// アニメーションデータの読み込み (ModelLoader.cpp の Load 関数内)
	// ---------------------------------------------------------
	pModelData->GetAnimationsRef().clear();

	if (pScene->HasAnimations())
	{
		auto& anims = pModelData->GetAnimationsRef();
		anims.resize(pScene->mNumAnimations);

		for (unsigned int i = 0; i < pScene->mNumAnimations; ++i)
		{
			aiAnimation* pAnim = pScene->mAnimations[i];
			auto& animData = anims[i];

			// アニメーション名、長さ、速度
			animData.name = pAnim->mName.length > 0 ? pAnim->mName.C_Str() : "Anim_" + std::to_string(i);
			animData.duration = (float)pAnim->mDuration;
			animData.ticksPerSecond = pAnim->mTicksPerSecond != 0.0f ? (float)pAnim->mTicksPerSecond : 25.0f;

			// ボーンごとの変形命令（チャンネル）
			animData.channels.resize(pAnim->mNumChannels);
			for (unsigned int c = 0; c < pAnim->mNumChannels; ++c)
			{
				aiNodeAnim* pChannel = pAnim->mChannels[c];
				auto& channelData = animData.channels[c];
				channelData.nodeName = pChannel->mNodeName.C_Str();

				// 位置(Position)キー
				channelData.positionKeys.resize(pChannel->mNumPositionKeys);
				for (unsigned int k = 0; k < pChannel->mNumPositionKeys; ++k) {
					channelData.positionKeys[k].time = (float)pChannel->mPositionKeys[k].mTime;
					channelData.positionKeys[k].value = Math::Vector3(
						pChannel->mPositionKeys[k].mValue.x,
						pChannel->mPositionKeys[k].mValue.y,
						pChannel->mPositionKeys[k].mValue.z);
				}

				// 回転(Rotation)キー
				channelData.rotationKeys.resize(pChannel->mNumRotationKeys);
				for (unsigned int k = 0; k < pChannel->mNumRotationKeys; ++k) {
					channelData.rotationKeys[k].time = (float)pChannel->mRotationKeys[k].mTime;
					// ※aiQuaternionは(w, x, y, z)ですが、SimpleMathは(x, y, z, w)なので順番に注意
					channelData.rotationKeys[k].value = Math::Quaternion(
						pChannel->mRotationKeys[k].mValue.x,
						pChannel->mRotationKeys[k].mValue.y,
						pChannel->mRotationKeys[k].mValue.z,
						pChannel->mRotationKeys[k].mValue.w);
				}

				// スケール(Scaling)キー
				channelData.scalingKeys.resize(pChannel->mNumScalingKeys);
				for (unsigned int k = 0; k < pChannel->mNumScalingKeys; ++k) {
					channelData.scalingKeys[k].time = (float)pChannel->mScalingKeys[k].mTime;
					channelData.scalingKeys[k].value = Math::Vector3(
						pChannel->mScalingKeys[k].mValue.x,
						pChannel->mScalingKeys[k].mValue.y,
						pChannel->mScalingKeys[k].mValue.z);
				}
			}
		}
	}

	return true;
}

void Modeloader::InternalProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx, ModelData* pModelData, const std::string& dirPath)
{
	int currentNodeIdx = (int)pModelData->GetNodesRef().size();
	ModelData::Node node;
	node.name = pSrcNode->mName.C_Str();
	node.parentIndex = parentIdx;
	node.localTransform = pSrcNode->mTransformation;

	node.originalLocalTransform = pSrcNode->mTransformation;
	pModelData->GetNodesRef().push_back(node);

	if (parentIdx >= 0)
	{
		pModelData->GetNodesRef()[parentIdx].children.push_back(currentNodeIdx);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumMeshes; i++)
	{
		aiMesh* pMesh = pScene->mMeshes[pSrcNode->mMeshes[i]];
		aiMaterial* pMaterial = pScene->mMaterials[pMesh->mMaterialIndex];

		pModelData->GetNodesRef()[currentNodeIdx].meshes.push_back(
			Parse(pScene, pMesh, pMaterial, dirPath, pSrcNode->mTransformation, pModelData) // ← pSrcNode->mTransformation に変更
		);
	}

	for (unsigned int i = 0; i < pSrcNode->mNumChildren; i++) 
	{
		InternalProcessNode(pSrcNode->mChildren[i], pScene, currentNodeIdx, pModelData, dirPath);
	}
}

std::shared_ptr<Mesh> Modeloader::Parse(const aiScene* pScene, const aiMesh* pMesh, const aiMaterial* pMaterial, const std::string& dirPath, const aiMatrix4x4& transform, ModelData* pModelData)
{
	std::vector<MeshVertex> vertices;
	std::vector<MeshFace> faces;

	vertices.resize(pMesh->mNumVertices);

	aiMatrix3x3 normalTransform(transform);

	normalTransform.Inverse();
	normalTransform.Transpose();

	for (UINT i = 0; i < pMesh->mNumVertices; i++)
	{
		// --- 位置(Position)に行列を適用---
		aiVector3D pos = pMesh->mVertices[i];
		pos *= transform;	//	頂点座標に変換行列を掛ける
		vertices[i].Position.x = pos.x;
		vertices[i].Position.y = pos.y;
		vertices[i].Position.z = pos.z;

		if (pMesh->HasTextureCoords(0))
		{
			vertices[i].UV.x = static_cast<float>(pMesh->mTextureCoords[0][i].x);
			vertices[i].UV.y = static_cast<float>(pMesh->mTextureCoords[0][i].y);
		}
		// --- 法線(Normal)に行列を適用 ---
		aiVector3D normal = pMesh->mNormals[i];
		normal *= normalTransform;
		normal.Normalize();		//	再正規化
		vertices[i].Normal.x = normal.x;
		vertices[i].Normal.y = normal.y;
		vertices[i].Normal.z = normal.z;

		if (pMesh->HasTangentsAndBitangents())
		{
			// --- 接ベクトル(Tangent)に行列を適用 ---
			aiVector3D tangent = pMesh->mTangents[i];
			tangent *= normalTransform;
			tangent.Normalize(); // 再正規化
			vertices[i].Tangent.x = tangent.x;
			vertices[i].Tangent.y = tangent.y;
			vertices[i].Tangent.z = tangent.z;
		}

		if (pMesh->HasVertexColors(0))
		{
			// (Colorの処理はそのまま)
			Math::Color color;
			color.x = pMesh->mColors[0][i].r;
			color.y = pMesh->mColors[0][i].g;
			color.z = pMesh->mColors[0][i].b;

			vertices[i].Color = color.RGBA().v;
		}
	}



		// ボーン情報の読み取りと頂点への割り当て
	if (pMesh->HasBones())
	{
		for (unsigned int i = 0; i < pMesh->mNumBones; i++)
		{
			aiBone* pBone = pMesh->mBones[i];
			std::string boneName = pBone->mName.C_Str();
			
			int boneIndex = 0;
			auto it = pModelData->GetBoneMapRef().find(boneName);
			if (it == pModelData->GetBoneMapRef().end())
			{
				boneIndex = (int)pModelData->GetBonesRef().size();
				pModelData->GetBoneMapRef()[boneName] = boneIndex;
				
				ModelData::BoneInfo boneInfo;
				aiMatrix4x4 aiMat = pBone->mOffsetMatrix;
				aiMat.Transpose();
				boneInfo.offsetMatrix = *(Math::Matrix*)&aiMat;
				pModelData->GetBonesRef().push_back(boneInfo);
			}
			else
			{
				boneIndex = it->second;
			}
			
			for (unsigned int w = 0; w < pBone->mNumWeights; w++)
			{
				UINT vertexID = pBone->mWeights[w].mVertexId;
				float weight = pBone->mWeights[w].mWeight;
				
				for (int j = 0; j < 4; j++)
				{
					if (vertices[vertexID].SkinWeight[j] == 0.0f)
					{
						vertices[vertexID].SkinIndex[j] = boneIndex;
						vertices[vertexID].SkinWeight[j] = weight;
						break;
					}
				}
			}
		}
	}

	// ウェイトの正規化とウェイトなし頂点の救済
	for (UINT i = 0; i < pMesh->mNumVertices; i++)
	{
		float sum = vertices[i].SkinWeight[0] + vertices[i].SkinWeight[1] + vertices[i].SkinWeight[2] + vertices[i].SkinWeight[3];
		if (sum > 0.0001f)
		{
			vertices[i].SkinWeight[0] /= sum;
			vertices[i].SkinWeight[1] /= sum;
			vertices[i].SkinWeight[2] /= sum;
			vertices[i].SkinWeight[3] /= sum;
		}
		else
		{
			vertices[i].SkinIndex[0] = 0;
			vertices[i].SkinWeight[0] = 1.0f;
		}
	}

	faces.resize(pMesh->mNumFaces);

	for (UINT i = 0; i < pMesh->mNumFaces; ++i)
	{
		faces[i].Idx[0] = pMesh->mFaces[i].mIndices[0];
		faces[i].Idx[1] = pMesh->mFaces[i].mIndices[1];
		faces[i].Idx[2] = pMesh->mFaces[i].mIndices[2];
	}

	std::shared_ptr<Mesh> spMesh = std::make_shared<Mesh>();
	spMesh->Create(&GraphicsDevice::Instance(), vertices, faces, ParseMaterial(pMaterial, dirPath));

	return spMesh;
}

const Material Modeloader::ParseMaterial(const aiMaterial* pMaterial, const std::string& dirPath)
{
	Material material = {};

	// マテリアルの名前を取得
	{
		aiString name;

		if (pMaterial->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
		{
			material.Name = name.C_Str();
		}
	}

	// Diffuseテクスチャの取得
	{
		aiString path;

		if (pMaterial->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &path) == AI_SUCCESS)
		{
			auto filePath = std::string(path.C_Str());

			material.spBaseColorTex = std::make_shared<Texture>();

			if (!material.spBaseColorTex->Load(&GraphicsDevice::Instance(), dirPath + filePath))
			{
				assert(0);
				return Material();
			}
		}
	}

	// DiffuseColorの取得
	{
		aiColor4D diffuse;

		if (pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
		{
			material.BaseColor.x = diffuse.r;
			material.BaseColor.y = diffuse.g;
			material.BaseColor.z = diffuse.b;
			material.BaseColor.w = diffuse.a;
		}
	}

	// MetallicRoughnessテクスチャの取得
	{
		aiString path;

		if (pMaterial->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &path) == AI_SUCCESS ||
			pMaterial->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &path) == AI_SUCCESS)
		{
			auto filePath = std::string(path.C_Str());

			material.spMetallicRoughnessTex = std::make_shared<Texture>();

			if (!material.spMetallicRoughnessTex->Load(&GraphicsDevice::Instance(), dirPath + filePath))
			{
				assert(0);
				return Material();
			}
		}
	}

	// Metallicを取得
	{
		float metallic = 0.0f;

		if (pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
		{
			material.Metallic = metallic;
		}
	}

	// Roughness
	{
		float roughness = 1.0f;

		if (pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
		{
			material.Roughness = roughness;
		}
	}

	// Emissiveテクスチャの取得
	{
		aiString path;

		if (pMaterial->GetTexture(AI_MATKEY_EMISSIVE_TEXTURE, &path) == AI_SUCCESS)
		{
			auto filePath = std::string(path.C_Str());

			material.spEmissiveTex = std::make_shared<Texture>();

			if (!material.spEmissiveTex->Load(&GraphicsDevice::Instance(), dirPath + filePath))
			{
				assert(0);
				return Material();
			}
		}
	}

	// Emissiveの取得
	{
		aiColor3D emissive;

		if (pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
		{
			material.Emissive.x = emissive.r;
			material.Emissive.y = emissive.g;
			material.Emissive.z = emissive.b;
		}
	}

	// 法線テクスチャの取得
	{
		aiString path;

		if (pMaterial->GetTexture(AI_MATKEY_NORMAL_TEXTURE, &path) == AI_SUCCESS)
		{
			auto filePath = std::string(path.C_Str());

			material.spNormalTex = std::make_shared<Texture>();

			if (!material.spNormalTex->Load(&GraphicsDevice::Instance(), dirPath + filePath))
			{
				assert(0);
				return Material();
			}
		}
	}

	return material;
}
