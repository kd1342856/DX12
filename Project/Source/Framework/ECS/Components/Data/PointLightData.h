#pragma once

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

    // フリッカー(明滅) - 電球の揺らぎ/ろうそくの炎等
    bool  m_flickerEnabled = false;
    float m_flickerSpeed = 8.0f;     // 明滅の速さ(Hz相当)
    float m_flickerIntensity = 0.4f; // 0=明滅なし, 1=最大で完全に消えるまで暗くなる
    float m_flickerSeed = 0.0f;      // 複数の光源が同じ位相で明滅しないようずらす(未設定なら生成時にランダム化)
};
