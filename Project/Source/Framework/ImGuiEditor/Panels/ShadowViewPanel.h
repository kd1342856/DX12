#pragma once
#include "../IEditorPanel.h"

// 影の中身: 月の影・スポットの影(2D配列)・点光源の影(キューブ配列)の深度と、枠を使っているライトを並べる
class ShadowViewPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
