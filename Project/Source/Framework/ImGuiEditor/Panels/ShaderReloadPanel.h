#pragma once
#include "../IEditorPanel.h"

// シェーダーのホットリロードの状態と、コンパイルエラーの一覧(ファイル名・行番号つき)
class ShaderReloadPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;

    // エディターを閉じていてもエラーに気付けるよう、画面の隅に件数を出す。エラーが無ければ何もしない
    static void DrawErrorNotice();
};
