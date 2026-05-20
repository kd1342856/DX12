#pragma once

enum class ModelType {
    Static,
    Dynamic
};

// =============================================
// ModelRenderData
// 3Dモデルデータの保持
// =============================================
struct ModelRenderData
{
	std::shared_ptr<ModelData> m_spModelData;
	std::string m_filePath;
	ModelType m_modelType = ModelType::Static;
};