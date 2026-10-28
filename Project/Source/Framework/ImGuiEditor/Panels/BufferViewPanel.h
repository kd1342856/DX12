#pragma once
#include "../IEditorPanel.h"

// バッファの一覧: 深度・法線・SSAO・速度などの途中のバッファを、見える色にしたサムネイルで並べる
class BufferViewPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
