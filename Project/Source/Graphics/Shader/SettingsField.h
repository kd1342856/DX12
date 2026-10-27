#pragma once
#include <cstdint>

//=============================
//
// 設定の値1つの定義。各設定の構造体のVisitFieldsで、値と一緒に渡す
//
//   ここに書いた表示名・説明・単位・範囲から、シェーダーエディターの部品が描かれ、
//   保存(シーン・プリセット)、初期値に戻す、A/B比較、Undo/Redoも同じ一覧を使う。
//   値を足すときは、構造体のメンバーとVisitFieldsの1行を足せばエディターに出る
//   (シェーダーへ送るなら、CBufferData・ShaderManagerでのコピー・HLSLのcbufferも)。
//
//=============================

// 値の描き方
enum class FieldKind : uint8_t
{
    Number,      // 数値(スライダーと入力欄)
    Integer,     // 整数(スライダーと入力欄)
    Toggle,      // オンオフ
    Color,       // 色(色見本とRGB)
    ColorHDR,    // 1を超える色(明るさの値を持つ色)
    Direction,   // 向き(XYZ、長さ1に正規化する)
    Temperature, // 色温度(K)。スライダーの線を色のグラデーションにする
    Shutter,     // シャッター速度(秒)。1/xで表示する
    Aperture,    // F値
};

namespace FieldFlags
{
    constexpr uint32_t kNone = 0;
    constexpr uint32_t kAdvanced = 1 << 0;    // 「詳細」の折りたたみの中に出す
    constexpr uint32_t kNotSaved = 1 << 1;    // シーンとプリセットに保存しない(画質設定・オプション画面・開発用が決める値)
    constexpr uint32_t kLogarithmic = 1 << 2; // スライダーの目盛りを対数にする(範囲が何桁にもわたる値)
}

struct FieldMeta
{
    const char* key;          // JSONのキー(英語)。保存したファイルとの互換のため変えないこと
    const char* label;        // 表示名(UTF-8)。画面で何が起きるかの言葉で書く
    const char* description;  // 説明(UTF-8)。何が変わるか・目安の値
    const char* unit = "";    // 単位(UTF-8)。無ければ空
    float minValue = 0.0f;    // スライダーの範囲(破綻しない範囲)。範囲の外は入力欄からだけ入れられる
    float maxValue = 1.0f;
    FieldKind kind = FieldKind::Number;
    uint32_t flags = FieldFlags::kNone;
    const char* section = ""; // 見出し(UTF-8)。前の値と違うときに見出しを出す
    // 別のオンオフの値(キー)で、出すかどうか・触れるかどうかを決める。空なら常に出す
    const char* condition = "";
    bool conditionValue = true; // conditionの値がこれのときに有効
    bool hideWhenInactive = false; // 有効でないとき、薄くせずに隠す(どちらか一方だけを出す値の組)

    // VisitFieldsで読みやすく書くための、1項目ずつ変えた写しを返す関数
    FieldMeta Unit(const char* text) const { FieldMeta meta = *this; meta.unit = text; return meta; }
    FieldMeta Range(float minimum, float maximum) const { FieldMeta meta = *this; meta.minValue = minimum; meta.maxValue = maximum; return meta; }
    FieldMeta Kind(FieldKind fieldKind) const { FieldMeta meta = *this; meta.kind = fieldKind; return meta; }
    FieldMeta Section(const char* text) const { FieldMeta meta = *this; meta.section = text; return meta; }
    FieldMeta Advanced() const { FieldMeta meta = *this; meta.flags |= FieldFlags::kAdvanced; return meta; }
    FieldMeta NotSaved() const { FieldMeta meta = *this; meta.flags |= FieldFlags::kNotSaved; return meta; }
    FieldMeta Logarithmic() const { FieldMeta meta = *this; meta.flags |= FieldFlags::kLogarithmic; return meta; }
    // conditionKeyのオンオフがvalueのときだけ触れる(hide=trueなら、そうでないときは隠す)
    FieldMeta When(const char* conditionKey, bool value = true, bool hide = false) const
    {
        FieldMeta meta = *this;
        meta.condition = conditionKey;
        meta.conditionValue = value;
        meta.hideWhenInactive = hide;
        return meta;
    }
};

// VisitFieldsの1行目に書く。キー・表示名・説明は必ず書く
inline FieldMeta Field(const char* key, const char* label, const char* description)
{
    FieldMeta meta;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    return meta;
}
