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
    // 色は色温度(K。0以下なら使わない)×色味を輝度1に正規化したもの。明るさはm_intensityだけで決まる
    Math::Vector3 m_color = { 1.0f, 1.0f, 1.0f };
    float m_temperature = 2700.0f; // 白熱電球の色温度をデフォルトに
    float m_intensity = 100.0f;    // 中心方向の光度 cd(コーンの角度を変えても中心の明るさは変わらない)
    float m_range = 10.0f;         // 光の届く範囲。減衰は逆二乗で、ここで滑らかに0にする
    float m_innerAngle = 20.0f; // 中心からこの角度(度)までは最大の明るさ
    float m_outerAngle = 30.0f; // この角度(度)で0まで減衰する
    bool  m_enabled = true;
    bool  m_castShadow = true;     // 影を落とすか(近い順に、空いている影の枠があれば割り当てる)
    float m_sourceRadius = 0.03f;  // 光源の半径(m)。大きいほど影の輪郭がぼける(電球で約3cm)

    Math::Vector3 m_localOffset = { 0.0f, 0.0f, 0.0f };    // 光源位置(ローカル空間。エンティティのスケールも掛かる)
    Math::Vector3 m_localDirection = { 0.0f, 0.0f, 1.0f }; // 照らす向き(ローカル空間)

    // 同じエンティティのモデル自体の発光(Emissive)の輝度 nit(cd/m2)。点灯中だけライトの色で光らせる。
    // 0なら発光しない。電球のかさや蛍光灯の表面は数千nit程度
    float m_modelGlow = 2000.0f;
    std::string m_glowMaterial; // 空なら全メッシュ。指定時はこの名前のマテリアルのメッシュだけ光らせる
    // 光源の光を受けて明るく見える面(吊り下げ照明のかさの内側など)。このマテリアルの裏面(内側)だけを
    // m_modelGlow * m_glowReflectScale の輝度で光らせる。空なら無し。
    std::string m_glowReflectMaterial;
    float m_glowReflectScale = 0.3f;

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
    bool  lit = true;         // 点滅中の現在の見た目

    void Start(float duration)
    {
        if (timer <= 0.0f) { toggleTimer = 0.0f; lit = true; } // 次のUpdateですぐ一度消える
        timer = (std::max)(timer, duration);
    }
    bool IsFlickering() const { return timer > 0.0f; }

    // 戻り値: 見た目上消えているならtrue
    bool Update(float deltaTime)
    {
        if (timer <= 0.0f) return false;
        timer -= deltaTime;
        if (timer <= 0.0f) { timer = 0.0f; lit = true; return false; }
        toggleTimer -= deltaTime;
        if (toggleTimer <= 0.0f) {
            lit = !lit;
            // 消えている時間は短め、点いている時間は不規則に
            toggleTimer = lit ? Random::Instance().Range(0.03f, 0.25f) : Random::Instance().Range(0.02f, 0.12f);
        }
        return !lit;
    }
};
