#include "ShaderEditor.h"
#include "../../../../Library/ImGui/imgui.h"

void ShaderEditor::Initialize()
{
    m_Panels.push_back(std::make_unique<RendererPanel>());
    m_Panels.push_back(std::make_unique<LightingPanel>());
    m_Panels.push_back(std::make_unique<ShadowPanel>());
    m_Panels.push_back(std::make_unique<IBLPanel>());
    m_Panels.push_back(std::make_unique<PostProcessPanel>());
    m_Panels.push_back(std::make_unique<FogPanel>());
    m_Panels.push_back(std::make_unique<DebugPanel>());
    m_Panels.push_back(std::make_unique<MaterialPanel>());
}

void ShaderEditor::Draw(EditorContext& ctx)
{
    ImGui::Begin("Shader Editor");
    for (auto& panel : m_Panels)
    {
        panel->Draw(ctx);
    }
    ImGui::End();
}
