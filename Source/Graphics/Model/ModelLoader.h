#pragma once

#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Model.h"

class Modeloader
{
public:
	bool Load(std::string filepath, std::vector<ModelData::Node>& nodes);

private:
	//	‰ğÍ
	std::shared_ptr<Mesh> Parse(const aiScene* pScene, const aiMesh* pMesh,
		const aiMaterial* pMaterial, const std::string& dirPath);

	//	ƒ}ƒeƒŠƒAƒ‹‚Ì‰ğÍ
	const Material ParseMaterial(const aiMaterial* pMaterial, const std::string& dirPath);
};
