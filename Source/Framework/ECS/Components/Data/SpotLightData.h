#pragma once

// =============================================
// SpotLightData
// 懐中電灯/天井照明/ランプ等のスポットライト。m_enabledでオンオフする。
// 光源位置と向きはエンティティのローカル空間で指定し、TransformDataのワールド行列で変換する
// (モデルと同じエンティティに付けておけば、配置時に回転・拡縮してもモデルと一緒に動く)。
// 最大10個まで(SpotLightSystemが毎フレーム、カメラに近い順で上位10個をシェーダーへ送る)。
// 影は未対応(シェーダー側のSpotShadowは未使用)。
// =============================================
struct SpotLightData
{
    Math::Vector3 m_color = { 1.0f, 0.95f, 0.85f };
    float m_intensity = 3.0f;
    float m_range = 10.0f;
    float m_innerAngle = 20.0f; // 中心からこの角度(度)までは最大の明るさ
    float m_outerAngle = 30.0f; // この角度(度)で0まで減衰する
    bool  m_enabled = true;

    Math::Vector3 m_localOffset = { 0.0f, 0.0f, 0.0f };    // 光源位置(ローカル空間。エンティティのスケールも掛かる)
    Math::Vector3 m_localDirection = { 0.0f, 0.0f, 1.0f }; // 照らす向き(ローカル空間)
};
