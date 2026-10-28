#include "ShaderEditor.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../EditorPreferences.h"
#include "../../../Graphics/Shader/ShaderSettingsIO/ShaderSettingsIO.h"

void ShaderEditor::Initialize()
{
    // コンパイルエラーに気付けるよう、ホットリロードの状態を一番上に置く
    AddPanel<ShaderReloadPanel>("ShaderReload", u8"シェーダーのホットリロード");
    AddPanel<ShaderSettingsPanel>("ShaderSettings", u8"設定の保存・プリセット・A/B比較");
    // よく使う値は、設定のパネルの前に置く
    AddPanel<PinnedPanel>("Pinned", u8"よく使う値");
    // 解像度・画質・fpsの上限(重さを確かめるときに使う)
    AddPanel<DisplayPanel>("Display", u8"解像度・画質・fpsの上限");
    AddPanel<RendererPanel>("Renderer", u8"描画");
    AddPanel<LightingPanel>("Lighting", u8"月の光と空");
    AddPanel<ShadowPanel>("Shadow", u8"月の影");
    AddPanel<SSAOPanel>("SSAO", u8"すき間の陰り");
    AddPanel<PostProcessPanel>("PostProcess", u8"露出とポストプロセス");
    AddPanel<FogPanel>("Fog", u8"霧とハントの演出");
    AddPanel<DebugPanel>("Debug", u8"デバッグビュー");
    AddPanel<MeasurePanel>("Measure", u8"計測と比較");
    AddPanel<BufferViewPanel>("BufferView", u8"バッファの一覧");
    AddPanel<MaterialPanel>("Material", u8"マテリアル");
}

void ShaderEditor::Draw(EditorContext& ctx)
{
    // 保存していない変更があれば、見出しに「*」を付けて常に見せる(IDは###の後ろなので変わらない)
    const int unsavedCount = ShaderSettingsIO::CountUnsavedChanges();
    const std::string title = std::string(u8"シェーダーエディター") + (unsavedCount > 0 ? " *" : "") + "###Shader Editor";
    ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_MenuBar);
    DrawWindowMenu();

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

    for (PanelEntry& entry : m_Panels)
    {
        if (!EditorPreferences::IsPanelDetached(entry.id))
        {
            entry.upPanel->Draw(ctx);
        }
    }
    ImGui::End();

    // 別のウィンドウに出したパネル。ドッキングでき、閉じるとシェーダーエディターに戻る
    for (PanelEntry& entry : m_Panels)
    {
        if (!EditorPreferences::IsPanelDetached(entry.id))
        {
            continue;
        }
        const std::string windowTitle = entry.name + "###ShaderEditor/" + entry.id;
        bool isOpen = true;
        ImGui::SetNextWindowSize(ImVec2(440.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(windowTitle.c_str(), &isOpen))
        {
            // パネルの見出し(折りたたみ)は、出したときに開いておく
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            entry.upPanel->Draw(ctx);
        }
        ImGui::End();
        if (!isOpen)
        {
            EditorPreferences::SetPanelDetached(entry.id, false);
        }
    }
}

void ShaderEditor::DrawWindowMenu()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }
    if (ImGui::BeginMenu(u8"ウィンドウ"))
    {
        ImGui::TextDisabled(u8"チェックしたパネルを別のウィンドウに出す(ドッキングできる)");
        ImGui::Separator();
        for (PanelEntry& entry : m_Panels)
        {
            bool isDetached = EditorPreferences::IsPanelDetached(entry.id);
            if (ImGui::MenuItem(entry.name.c_str(), nullptr, &isDetached))
            {
                EditorPreferences::SetPanelDetached(entry.id, isDetached);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(u8"すべてシェーダーエディターに戻す"))
        {
            for (PanelEntry& entry : m_Panels)
            {
                EditorPreferences::SetPanelDetached(entry.id, false);
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}
