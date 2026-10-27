#include "ShaderEditor.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Shader/ShaderSettingsIO/ShaderSettingsIO.h"

void ShaderEditor::Initialize()
{
    // コンパイルエラーに気付けるよう、ホットリロードの状態を一番上に置く
    m_Panels.push_back(std::make_unique<ShaderReloadPanel>());
    m_Panels.push_back(std::make_unique<ShaderSettingsPanel>());
    // よく使う値は、設定のパネルの前に置く
    m_Panels.push_back(std::make_unique<PinnedPanel>());
    m_Panels.push_back(std::make_unique<RendererPanel>());
    m_Panels.push_back(std::make_unique<LightingPanel>());
    m_Panels.push_back(std::make_unique<ShadowPanel>());
    m_Panels.push_back(std::make_unique<SSAOPanel>());
    m_Panels.push_back(std::make_unique<PostProcessPanel>());
    m_Panels.push_back(std::make_unique<FogPanel>());
    m_Panels.push_back(std::make_unique<DebugPanel>());
    m_Panels.push_back(std::make_unique<MeasurePanel>());
    m_Panels.push_back(std::make_unique<MaterialPanel>());
}

void ShaderEditor::Draw(EditorContext& ctx)
{
    // 保存していない変更があれば、見出しに「*」を付けて常に見せる(IDは###の後ろなので変わらない)
    const int unsavedCount = ShaderSettingsIO::CountUnsavedChanges();
    const std::string title = std::string(u8"シェーダーエディター") + (unsavedCount > 0 ? " *" : "") + "###Shader Editor";
    ImGui::Begin(title.c_str());

    // 検索: 日本語の名前と英語のキーのどちらでも探せる。当たった値のパネルを開き、それ以外を隠す
    static char s_search[64] = {};
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##search", u8"値を探す(日本語の名前か英語のキー)", s_search, sizeof(s_search));
    ImGui::SameLine();
    ImGui::BeginDisabled(s_search[0] == '\0');
    if (ImGui::Button(u8"消す"))
    {
        s_search[0] = '\0';
    }
    ImGui::EndDisabled();
    SettingsFieldUI::SetSearch(s_search);

    for (auto& panel : m_Panels)
    {
        panel->Draw(ctx);
    }
    ImGui::End();
}
