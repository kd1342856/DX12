#pragma once

// =============================================
// PointLightData
// ロウソク/裸電球等の点光源。TransformDataの位置を光源座標として使う。
// 最大8個まで(LightSystemが毎フレーム、カメラに近い順で上位8個をシェーダーへ送る)。
// =============================================
struct PointLightData
{
    Math::Vector3 m_color = { 1.0f, 0.8f, 0.5f }; // ろうそく寄りの暖色をデフォルトに
    float m_intensity = 1.0f;
    float m_range = 5.0f;
    bool  m_enabled = true;

    // フリッカー(明滅) - 電球の揺らぎ/ろうそくの炎等
    bool  m_flickerEnabled = false;
    float m_flickerSpeed = 8.0f;     // 明滅の速さ(Hz相当)
    float m_flickerIntensity = 0.4f; // 0=明滅なし, 1=最大で完全に消えるまで暗くなる
    float m_flickerSeed = 0.0f;      // 複数の光源が同じ位相で明滅しないようずらす(未設定なら生成時にランダム化)
};
