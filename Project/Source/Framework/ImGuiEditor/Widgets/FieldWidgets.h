#pragma once
#include <functional>
#include "ImGui/imgui.h"
#include "../../../Graphics/Shader/SettingsField.h"

//=============================
//
// エディターの部品(仕様書「5. 部品の決まり」)
//
//   値1つを、左に表示名・右に操作の部品として1行に描く(呼び出し側で2列の表を開いておく)。
//   どの部品にも、初期値から変えた印(左の琥珀色)・説明の吹き出し・右クリックの「初期値に戻す」が付く。
//   部品の選び方はFieldMeta::kindで決まり、呼び出しは SettingsFieldUI::Draw(VisitFieldsのUI用のVisitor)と、
//   値の層を自分で持つマテリアルのパネル(MaterialPanel)から行う。
//
//=============================
namespace FieldWidgets
{
    // 操作の結果。Undo/Redoは activated で変更前を控え、committed で1回分として記録する
    struct EditResult
    {
        bool changed = false;   // このフレームで値が変わった
        bool activated = false; // 操作を始めた(ドラッグ・入力の開始)
        bool committed = false; // 操作を終え、値が変わっていた(Undoの1回)
        bool reset = false;     // 右クリックの「初期値に戻す」で戻した
    };

    // 初期値から変えた値の印の色
    ImU32 GetModifiedColor();

    // 次に描く1行を強調する(「変えた値の一覧」から飛んだとき)
    void SetNextRowHighlight(bool isHighlighted);
    // 次に描く1行の「変えた印」と「初期値に戻す」が使えるかを、値の比較ではなく呼び出し側が決める
    // (モデルの.metaのマテリアルのように、上書き中かどうかをフラグで持つ値)
    void SetNextModified(bool isModified);
    // 右クリックのメニューに足す項目(よく使う値に追加、など)。描き終えたらnullptrに戻す
    void SetContextMenuExtension(const std::function<void()>* pExtension);

    // 1行の表の列を開く。SettingsFieldUIがセクションごとに呼ぶ
    bool BeginFieldTable(const char* id);
    void EndFieldTable();

    EditResult Number(const FieldMeta& meta, float& value, float defaultValue);
    EditResult Integer(const FieldMeta& meta, int& value, int defaultValue);
    EditResult Toggle(const FieldMeta& meta, bool& value, bool defaultValue);
    EditResult Color(const FieldMeta& meta, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue);
    EditResult Direction(const FieldMeta& meta, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue);

    // 計算した値(編集できない)。元になった値を薄い文字で添える
    void ComputedValue(const char* label, const char* valueText, const char* sourceText);

    // 値を表示用の文字列にする(単位なし)。Undoの知らせや変えた値の一覧でも使う
    std::string FormatValue(const FieldMeta& meta, float value);
}
