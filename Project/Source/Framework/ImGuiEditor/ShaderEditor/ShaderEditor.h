#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../IEditorPanel.h"
#include "../EditorContext.h"
#include "../Panels/DebugPanel.h"
#include "../Panels/LightingPanel.h"
#include "../Panels/ShadowPanel.h"
#include "../Panels/SSAOPanel.h"
#include "../Panels/MaterialPanel.h"
#include "../Panels/RendererPanel.h"
#include "../Panels/PostProcessPanel.h"
#include "../Panels/FogPanel.h"
#include "../Panels/ShaderReloadPanel.h"
#include "../Panels/ShaderSettingsPanel.h"
#include "../Panels/PinnedPanel.h"
#include "../Panels/MeasurePanel.h"
#include "../Panels/BufferViewPanel.h"
#include "../Panels/DisplayPanel.h"

// シェーダーエディター。パネルを1つのウィンドウに並べ、「ウィンドウ」メニューから選んだパネルは
// 別のウィンドウに出す(ドッキングできる)。出したパネルはエディターの設定(EditorSettings.json)に覚える
class ShaderEditor
{
public:
    void Initialize();
    void Draw(EditorContext& ctx);

private:
    struct PanelEntry
    {
        std::unique_ptr<IEditorPanel> upPanel;
        std::string id;   // 設定とウィンドウのIDに使う(英語。変えると出したウィンドウの位置が失われる)
        std::string name; // メニューとウィンドウの見出し(UTF-8)
    };

    template<typename TPanel>
    void AddPanel(const char* pId, const char* pName)
    {
        m_Panels.push_back({ std::make_unique<TPanel>(), pId, pName });
    }

    void DrawWindowMenu();

    std::vector<PanelEntry> m_Panels;
};
