#pragma once
#include <string>
#include "../IEditorPanel.h"

// シェーダーの設定の保存(シーンのJSON・プリセット)と、初期値からの変更の数、A/B比較
class ShaderSettingsPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;

private:
    void DrawSceneAndDefaults();
    void DrawChangedList();
    void DrawPresets();
    void DrawABCompare();
    void SetMessage(const std::string& message, bool isError);

    // A/B比較で記録した設定(ShaderSettingsIO::Captureの形)。空なら未記録
    nlohmann::json m_slots[2];
    int m_shownSlot = -1; // 最後に表示したスロット。-1なら表示していない

    std::string m_selectedPreset;
    std::string m_describedPreset;     // m_selectedDescriptionを読んだプリセット
    std::string m_selectedDescription;
    char m_presetName[64] = {};

    std::string m_message;
    bool m_isMessageError = false;
};
