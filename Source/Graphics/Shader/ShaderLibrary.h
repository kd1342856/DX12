#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include "GraphicsShader/GraphicsShader.h"

class ShaderLibrary {
public:
	static ShaderLibrary& Instance() {
		static ShaderLibrary instance;
		return instance;
	}

	template <typename T>
	void Register(GraphicsDevice* pDevice) {
		auto typeId = std::type_index(typeid(T));
		auto shader = std::make_unique<T>();
		shader->Create(pDevice);
		m_shaders[typeId] = std::move(shader);
	}

	template <typename T>
	T& Get() {
		auto typeId = std::type_index(typeid(T));
		return *static_cast<T*>(m_shaders[typeId].get());
	}

private:
	ShaderLibrary() = default;
	~ShaderLibrary() = default;

	std::unordered_map<std::type_index, std::unique_ptr<GraphicsShader>> m_shaders;
};