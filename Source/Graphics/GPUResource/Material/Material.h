#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>


class Texture;
struct ShaderProgram;
class GraphicsDevice;

class Material
{
public:
	std::string Name;
	std::shared_ptr<Texture> spBaseColorTex;
	Math::Vector4 BaseColor = { 1, 1, 1, 1 };
	std::shared_ptr<Texture> spMetallicRoughnessTex;
	float Metallic = 0.0f;
	float Roughness = 1.0f;
	std::shared_ptr<Texture> spEmissiveTex;
	Math::Vector3 Emissive = { 0, 0, 0 };
	std::shared_ptr<Texture> spNormalTex;
	void SetShader(ShaderProgram* pProgram) { m_pProgram = pProgram; }
	ShaderProgram* GetShader() const { return m_pProgram; }

	void SetTexture(const std::string& name, std::shared_ptr<Texture> texture) { m_defaultTextures[name] = texture; }
	std::shared_ptr<Texture> GetTexture(const std::string& name) const;

	void SetFloat(const std::string& name, float value) { m_defaultFloats[name] = value; }
	void SetVector(const std::string& name, const Math::Vector4& value) { m_defaultVectors[name] = value; }

	float GetFloat(const std::string& name) const;
	Math::Vector4 GetVector(const std::string& name) const;

	// Binds the material properties to the current command list
	virtual void Bind(GraphicsDevice* pDevice);

protected:
	ShaderProgram* m_pProgram = nullptr;
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_defaultTextures;
	std::unordered_map<std::string, float> m_defaultFloats;
	std::unordered_map<std::string, Math::Vector4> m_defaultVectors;
};

class MaterialInstance : public Material
{
public:
	MaterialInstance(std::shared_ptr<Material> baseMaterial) : m_spBaseMaterial(baseMaterial) {}

	void SetOverrideTexture(const std::string& name, std::shared_ptr<Texture> texture) { m_overrideTextures[name] = texture; }
	void SetOverrideFloat(const std::string& name, float value) { m_overrideFloats[name] = value; }
	void SetOverrideVector(const std::string& name, const Math::Vector4& value) { m_overrideVectors[name] = value; }

	void Bind(GraphicsDevice* pDevice) override;

	std::shared_ptr<Material> GetBaseMaterial() const { return m_spBaseMaterial; }

private:
	std::shared_ptr<Material> m_spBaseMaterial;
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_overrideTextures;
	std::unordered_map<std::string, float> m_overrideFloats;
	std::unordered_map<std::string, Math::Vector4> m_overrideVectors;
};
