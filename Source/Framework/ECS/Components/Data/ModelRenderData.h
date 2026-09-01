#pragma once

enum class ModelType {
    Static,
    Dynamic,
    Sky
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

	// falseの間はRenderSystemが描画をスキップする(影・反射も含む)。
	// Ghostの徘徊中の非表示切り替えなど、実行時のみの状態でシーンには保存しない。
	bool m_isVisible = true;
};
