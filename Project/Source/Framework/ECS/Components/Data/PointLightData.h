#pragma once
#include "../../../../Graphics/Shader/SettingsField.h"

// =============================================
// PointLightData
// ロウソク/裸電球等の点光源。TransformDataの位置を光源座標として使う。
// 最大8個まで(LightSystemが毎フレーム、カメラに近い順で上位8個をシェーダーへ送る)。
// =============================================
struct PointLightData
{
    // 色は色温度(K。0以下なら使わない)×色味を輝度1に正規化したもの。明るさはm_intensityだけで決まる
    Math::Vector3 m_color = { 1.0f, 1.0f, 1.0f };
    float m_temperature = 1800.0f; // ろうそくの色温度をデフォルトに
    float m_intensity = 12.0f;     // 光束 lm(ろうそく1本で約12 lm)
    float m_range = 5.0f;          // 光の届く範囲。減衰は逆二乗で、ここで滑らかに0にする
    bool  m_enabled = true;
    bool  m_castShadow = true;     // 影を落とすか(近い順に、空いている影の枠があれば割り当てる)
    float m_sourceRadius = 0.01f;  // 光源の半径(m)。大きいほど影の輪郭がぼける(ろうそくの炎で約1cm)

    // フリッカー(明滅) - 電球の揺らぎ/ろうそくの炎等
    bool  m_flickerEnabled = false;
    float m_flickerSpeed = 8.0f;     // 明滅の速さ(Hz相当)
    float m_flickerIntensity = 0.4f; // 0=明滅なし, 1=最大で完全に消えるまで暗くなる
    float m_flickerSeed = 0.0f;      // 複数の光源が同じ位相で明滅しないようずらす(未設定なら生成時にランダム化)

    // インスペクターの部品とUndo/Redoが使う値の定義(SettingsField.h)。保存はComponentSerializerRegistrationが行う
    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("Enabled", u8"点けておく", u8"切ると光らない。")
            .Kind(FieldKind::Toggle).Section(u8"点光源"), m_enabled);
        visit(Field("Intensity", u8"明るさ(光束)",
            u8"光源が全方向へ出す光の量。1 mの距離の明るさ(照度)は、およそ 光束 ÷ 12.6 lx。")
            .Unit("lm").Range(0.1f, 20000.0f).Logarithmic()
            .Ref(u8"ロウソク1本", 12.0f).Ref(u8"豆電球", 30.0f).Ref(u8"40W相当の電球", 485.0f).Ref(u8"60W相当の電球", 810.0f)
            .Ref(u8"100W相当の電球", 1520.0f), m_intensity);
        visit(Field("Temperature", u8"色温度",
            u8"光の色を色温度で決める。低いほど赤く、高いほど青い。0で使わない(色味だけ)。")
            .Kind(FieldKind::Temperature).Unit("K").Range(1000.0f, 12000.0f)
            .Ref(u8"ロウソク", 1850.0f).Ref(u8"白熱電球", 2700.0f).Ref(u8"電球色のLED", 3000.0f).Ref(u8"昼白色の蛍光灯", 5000.0f), m_temperature);
        visit(Field("Color", u8"色味", u8"色温度の色に掛ける色。明るさは光束で決まるので、ここでは色合いだけを変える。")
            .Kind(FieldKind::Color), m_color);
        visit(Field("Range", u8"光の届く範囲",
            u8"この距離で光を0にする(減衰は逆二乗で、ここで滑らかに0にする)。広げるほど重くなる。")
            .Unit("m").Range(0.1f, 30.0f), m_range);
        visit(Field("CastShadow", u8"影を落とす", u8"近い順に、空いている影の枠があれば割り当てる。")
            .Kind(FieldKind::Toggle).Section(u8"影"), m_castShadow);
        visit(Field("SourceRadius", u8"光源の大きさ(半径)",
            u8"大きいほど影の輪郭がぼける。")
            .Unit("m").Range(0.001f, 0.5f).Logarithmic().When("CastShadow")
            .Ref(u8"ロウソクの炎", 0.01f).Ref(u8"電球", 0.03f).Ref(u8"小さな照明のかさ", 0.1f), m_sourceRadius);
        visit(Field("FlickerEnabled", u8"明滅させる", u8"電球の揺らぎやロウソクの炎のように、明るさを揺らす。")
            .Kind(FieldKind::Toggle).Section(u8"明滅"), m_flickerEnabled);
        visit(Field("FlickerSpeed", u8"明滅の速さ", u8"1秒あたりの揺れのおよその回数。")
            .Unit("Hz").Range(0.0f, 30.0f).When("FlickerEnabled"), m_flickerSpeed);
        visit(Field("FlickerIntensity", u8"明滅の強さ", u8"0で明滅しない、1で最大のとき完全に消えるまで暗くなる。")
            .Range(0.0f, 1.0f).When("FlickerEnabled"), m_flickerIntensity);
        visit(Field("FlickerSeed", u8"明滅のずらし値",
            u8"同じ部屋に明滅する光源を複数置くとき、位相をずらしてばらばらに明滅させる。")
            .Range(0.0f, 100.0f).When("FlickerEnabled").Advanced(), m_flickerSeed);
    }
};
