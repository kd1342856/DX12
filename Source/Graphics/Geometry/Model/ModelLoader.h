#pragma once

class ModelData;
class Mesh;
struct Material;

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

class Modeloader
{
public:
	bool Load(std::string filepath, ModelData* pModelData);

private:
	void InternalProcessNode(aiNode* pSrcNode, const aiScene* pScene, int parentIdx,
		ModelData* pModelData, const std::string& dirPath);
	//	‰ğÍ
	std::shared_ptr<Mesh> Parse(const aiScene* pScene, const aiMesh* pMesh,
		const aiMaterial* pMaterial, const std::string& dirPath, const aiMatrix4x4& transform);

	//	ƒ}ƒeƒŠƒAƒ‹‚Ì‰ğÍ
	const Material ParseMaterial(const aiMaterial* pMaterial, const std::string& dirPath);

};
