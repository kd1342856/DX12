#pragma once

struct EditorContext;

class IEditorPanel
{
public:
    virtual ~IEditorPanel() = default;
    
    // パネルの描画処理。コンテキストからデータを受け取り、ImGuiで表示・編集を行う
    virtual void Draw(EditorContext& ctx) = 0;
};
