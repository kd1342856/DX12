#pragma once

// =============================================
// ModelComponent
// 3Dモデルデータの保持
// =============================================
struct ModelComponent
{
	std::shared_ptr<ModelData> m_spModelData;
	std::string m_filePath;
};
