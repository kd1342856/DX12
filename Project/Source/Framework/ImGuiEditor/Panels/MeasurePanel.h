#pragma once
#include "../IEditorPanel.h"

// 計測と比較: 明るさのヒストグラム・ピクセルインスペクター・リファレンス比較(見本とのワイプと差分)
class MeasurePanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;

private:
    void DrawHistogram();
    void DrawPixelInspector();
    void DrawReferenceCompare();

    bool m_isInspecting = false;
    char m_imagePath[260] = {};
};
