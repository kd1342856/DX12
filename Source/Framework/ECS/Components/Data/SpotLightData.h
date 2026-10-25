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
    Math::Vector3 m_color = {1.0f, 0.95f, 0.85f};
    float m_intensity = 3.0f;
    float m_range = 10.0f;
    float m_innerAngle = 20.0f; // 中心からこの角度(度)までは最大の明るさ
    float m_outerAngle = 30.0f; // この角度(度)で0まで減衰する
    bool m_enabled = true;

    Math::Vector3 m_localOffset = {0.0f, 0.0f, 0.0f};    // 光源位置(ローカル空間。エンティティのスケールも掛かる)
    Math::Vector3 m_localDirection = {0.0f, 0.0f, 1.0f}; // 照らす向き(ローカル空間)

    // 同じエンティティのモデル自体の発光(Emissive)。点灯中だけ m_color * m_modelGlow で光らせ、
    // Bloomの閾値を超えさせて「光っている」見た目にする。0なら発光しない。
    float m_modelGlow = 8.0f;
    std::string m_glowMaterial; // 空なら全メッシュ。指定時はこの名前のマテリアルのメッシュだけ光らせる

    // 実行時のみ(保存しない)。点滅演出で「見た目だけ一瞬消えている」状態。描画と発光だけがこれを見る。
    // m_enabled(点灯しているかどうか)自体は変えないので、ゴーストの判定(照明の中で減速・影女の
    // 退治条件など)は点滅中もぶれない。
    bool m_flickerOff = false;
};

// =============================================
// LightFlickerState
// 電気が不安定な感じの点滅を作る小さなタイマー(フラッシュライト用)。Start()で点滅を始め、
// 毎フレームUpdate()を呼ぶと「今は見た目上消えているか」を返す。
// (照明側はStimulusSourceが同じ考え方の点滅を自前で持っている)
// =============================================
struct LightFlickerState
{
    float timer = 0.0f;       // 残りの点滅時間
    float toggleTimer = 0.0f; // 次に点/消を切り替えるまでの時間
    bool lit = true;          // 点滅中の現在の見た目

    void Start(float duration)
    {
        if (timer <= 0.0f)
        {
            toggleTimer = 0.0f;
            lit = true;
        } // 次のUpdateですぐ一度消える
        timer = (std::max)(timer, duration);
    }
    bool IsFlickering() const
    {
        return timer > 0.0f;
    }

    // 戻り値: 見た目上消えているならtrue
    bool Update(float deltaTime)
    {
        if (timer <= 0.0f)
        {
            return false;
        }
        timer -= deltaTime;
        if (timer <= 0.0f)
        {
            timer = 0.0f;
            lit = true;
            return false;
        }
        toggleTimer -= deltaTime;
        if (toggleTimer <= 0.0f)
        {
            lit = !lit;
            // 消えている時間は短め、点いている時間は不規則に
            toggleTimer = lit ? Random::Instance().Range(0.03f, 0.25f) : Random::Instance().Range(0.02f, 0.12f);
        }
        return !lit;
    }
};
