#pragma once
#include "../IEditorPanel.h"

// よく使う値(右クリックの「よく使う値に追加」で並べた値)。シェーダーエディターの一番上に出す
class PinnedPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
