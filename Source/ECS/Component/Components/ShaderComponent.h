#pragma once

// =============================================
// ShaderComponent
// シェーダーと描画設定
// =============================================
struct ShaderComponent
{
std::shared_ptr<ShaderManager> m_spShader;
RenderingSetting m_renderingSetting;
std::vector<RangeType> m_rangeTypes;
std::wstring m_shaderFilePath;
};