#pragma once

// =============================================
// ShaderData
// シェーダーと描画設定
// =============================================
struct ShaderData
{
	RenderingSetting m_renderingSetting;
	std::vector<RangeType> m_rangeTypes;
	std::wstring m_shaderFilePath;
};