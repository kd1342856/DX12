#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../../../Framework/Manager/Asset/AssetHandle.h"
#include "../../../Framework/Manager/Asset/AssetData.h"

struct ShaderProgram;
class GraphicsDevice;
class Texture;

struct MaterialConstants
{
    Math::Vector4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    
    // Match alignment with HLSL completely
    float emissiveStrength = 1.0f;
    float emissiveFactorX = 0.0f;
    float emissiveFactorY = 0.0f;
    float emissiveFactorZ = 0.0f;
    
    int alphaMode = 0; // 0:Opaque, 1:Mask, 2:Blend
    int proceduralType = 0; // 0:None, 1:Blood, 2:Cobweb, 3:Glass, 4:Dissolve
    float dissolveAmount = 0.0f; // Dissolve用: 0=通常表示, 1=完全に消滅

    // 平面反射(窓ガラス等、proceduralType==3)がどの反射スロット(0〜2、最大3枚まで同時)を
    // 使うか。RenderSystem::RenderReflection()が毎フレーム、アクティブなReflectionComponent
    // の分だけ動的に割り当てる(-1=このフレームは専用カメラの反射が無い→SSRフォールバック)。
    int32_t reflectionSlot = -1;
};

class Material
{
public:
	std::string Name;

	MaterialTexture BaseColor;
	MaterialTexture Normal;
	MaterialTexture MetallicRoughness;
	MaterialTexture Occlusion;
	MaterialTexture Emissive;

	MaterialConstants Constants;

	// (Optional) Shader Handle for future pipeline state caching
	// AssetHandle<Shader> ShaderHandle;

	void SetShader(ShaderProgram* pProgram) { m_pProgram = pProgram; }
	ShaderProgram* GetShader() const { return m_pProgram; }

	// Binds the material properties to the current command list
	virtual void Bind(GraphicsDevice* pDevice);

protected:
	ShaderProgram* m_pProgram = nullptr;
};
