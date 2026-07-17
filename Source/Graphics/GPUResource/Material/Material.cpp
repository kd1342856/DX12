#include "../../../Pch.h"
#include "Material.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Shader/ShaderManager/ShaderManager.h"

std::shared_ptr<Texture> Material::GetTexture(const std::string& name) const
{
	auto it = m_defaultTextures.find(name);
	if (it != m_defaultTextures.end()) {
		return it->second;
	}
	return nullptr;
}

float Material::GetFloat(const std::string& name) const
{
	auto it = m_defaultFloats.find(name);
	if (it != m_defaultFloats.end()) {
		return it->second;
	}
	return 0.0f;
}

Math::Vector4 Material::GetVector(const std::string& name) const
{
	auto it = m_defaultVectors.find(name);
	if (it != m_defaultVectors.end()) {
		return it->second;
	}
	return Math::Vector4(0, 0, 0, 0);
}

void Material::Bind(GraphicsDevice* pDevice)
{
	if (!m_pProgram) return;

	// This is a placeholder for actual constant buffer updates and texture binding.
	// We'll iterate over the shader reflection information and bind corresponding resources.
	
	// Example pseudo code for texture binding:
	/*
	for (const auto& binding : m_pProgram->Bindings) {
		if (binding.Type == ShaderBindingType::SRV) {
			auto tex = GetTexture(binding.Name);
			if (tex) {
				tex->Set(m_pProgram->GetRootParameterIndex(binding.Type, binding.BindPoint));
			}
		}
	}
	*/
}

void MaterialInstance::Bind(GraphicsDevice* pDevice)
{
	if (!m_spBaseMaterial) return;
	auto* pProgram = m_spBaseMaterial->GetShader();
	if (!pProgram) return;

	// In the real implementation, we will merge parameters or let the override parameters take precedence.
	
	// Pseudo code:
	/*
	for (const auto& binding : pProgram->Bindings) {
		if (binding.Type == ShaderBindingType::SRV) {
			auto it = m_overrideTextures.find(binding.Name);
			std::shared_ptr<Texture> tex = nullptr;
			if (it != m_overrideTextures.end()) {
				tex = it->second;
			} else {
				tex = m_spBaseMaterial->GetTexture(binding.Name);
			}

			if (tex) {
				tex->Set(pProgram->GetRootParameterIndex(binding.Type, binding.BindPoint));
			}
		}
	}
	*/
}
