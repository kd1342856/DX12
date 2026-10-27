#include "ShaderEditor.h"
#include "ImGui/imgui.h"

void ShaderEditor::Initialize()
{
    // コンパイルエラーに気付けるよう、ホットリロードの状態を一番上に置く
    m_Panels.push_back(std::make_unique<ShaderReloadPanel>());
    m_Panels.push_back(std::make_unique<ShaderSettingsPanel>());
    m_Panels.push_back(std::make_unique<RendererPanel>());
    m_Panels.push_back(std::make_unique<LightingPanel>());
    m_Panels.push_back(std::make_unique<ShadowPanel>());
    m_Panels.push_back(std::make_unique<SSAOPanel>());
    m_Panels.push_back(std::make_unique<PostProcessPanel>());
    m_Panels.push_back(std::make_unique<FogPanel>());
    m_Panels.push_back(std::make_unique<DebugPanel>());
    m_Panels.push_back(std::make_unique<MaterialPanel>());
}

void ShaderEditor::Draw(EditorContext& ctx)
{
    ImGui::Begin(u8"シェーダーエディター###Shader Editor");
    for (auto& panel : m_Panels)
    {
        panel->Draw(ctx);
    }
    ImGui::End();
}
