#pragma once
#include "../IEditorPanel.h"

// 解像度・画質・fpsの上限: 描画解像度の倍率(エディターだけの上書き)、画質のプリセット、fpsの上限を切り替える
class DisplayPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
