#pragma once
#include "../../../../Graphics/Shader/SettingsField.h"

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

    // インスペクターの部品とUndo/Redoが使う値の定義(SettingsField.h)。保存はComponentSerializerRegistrationが行う。
    // マテリアルの名前(文字列)はこの一覧に入れず、インスペクターが入力欄で描く
    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("Enabled", u8"点けておく", u8"切ると光らない(モデルの発光も消える)。")
            .Kind(FieldKind::Toggle).Section(u8"スポットライト"), m_enabled);
        visit(Field("Intensity", u8"中心の明るさ(光度)",
            u8"照らす向きの中心の明るさ。距離 d m の所の明るさ(照度)は 光度 ÷ d² lx。コーンの角度を変えても中心の明るさは変わらない。")
            .Unit("cd").Range(1.0f, 100000.0f).Logarithmic()
            .Ref(u8"60W相当の電球を下向き", 130.0f).Ref(u8"小型のLEDライト", 1000.0f).Ref(u8"明るい懐中電灯", 5000.0f)
            .Ref(u8"強力なサーチライト", 50000.0f), m_intensity);
        visit(Field("Temperature", u8"色温度",
            u8"光の色を色温度で決める。低いほど赤く、高いほど青い。0で使わない(色味だけ)。")
            .Kind(FieldKind::Temperature).Unit("K").Range(1000.0f, 12000.0f)
            .Ref(u8"白熱電球", 2700.0f).Ref(u8"電球色のLED", 3000.0f).Ref(u8"LEDの懐中電灯", 5000.0f).Ref(u8"昼光色の蛍光灯", 6500.0f), m_temperature);
        visit(Field("Color", u8"色味", u8"色温度の色に掛ける色。明るさは光度で決まるので、ここでは色合いだけを変える。")
            .Kind(FieldKind::Color), m_color);
        visit(Field("Range", u8"光の届く範囲",
            u8"この距離で光を0にする(減衰は逆二乗で、ここで滑らかに0にする)。広げるほど重くなる。")
            .Unit("m").Range(0.1f, 50.0f), m_range);
        visit(Field("InnerAngle", u8"いちばん明るい範囲の角度", u8"中心からこの角度までは最大の明るさ。")
            .Unit(u8"度").Range(0.0f, 89.0f), m_innerAngle);
        visit(Field("OuterAngle", u8"光が届く範囲の角度", u8"この角度で0まで減衰する。内側の角度との差が大きいほど、縁がぼける。")
            .Unit(u8"度").Range(0.1f, 89.0f), m_outerAngle);
        visit(Field("CastShadow", u8"影を落とす", u8"近い順に、空いている影の枠があれば割り当てる。")
            .Kind(FieldKind::Toggle).Section(u8"影"), m_castShadow);
        visit(Field("SourceRadius", u8"光源の大きさ(半径)",
            u8"大きいほど影の輪郭がぼける。")
            .Unit("m").Range(0.001f, 0.5f).Logarithmic().When("CastShadow")
            .Ref(u8"LEDの粒", 0.005f).Ref(u8"電球", 0.03f).Ref(u8"照明のかさ", 0.15f), m_sourceRadius);
        visit(Field("LocalOffset", u8"光源の位置", u8"このオブジェクトのローカル空間での光源の位置(スケールも掛かる)。")
            .Kind(FieldKind::Vector).Section(u8"位置と向き").Advanced(), m_localOffset);
        visit(Field("LocalDirection", u8"照らす向き", u8"このオブジェクトのローカル空間での照らす向き。長さは1にそろえる。")
            .Kind(FieldKind::Direction).Range(-1.0f, 1.0f).Advanced(), m_localDirection);
        visit(Field("ModelGlow", u8"モデルの発光の明るさ",
            u8"点灯中にモデル自体を光らせる輝度。0で光らせない。")
            .Unit("nit").Range(0.0f, 20000.0f).Section(u8"モデルの発光")
            .Ref(u8"光らせない", 0.0f).Ref(u8"照明のかさ", 2000.0f).Ref(u8"蛍光灯の表面", 8000.0f).Ref(u8"むき出しの電球", 100000.0f), m_modelGlow);
        visit(Field("GlowReflectScale", u8"反射で光る強さ", u8"反射で光る面(かさの内側など)の明るさ。モデルの発光に対する倍率。")
            .Range(0.0f, 2.0f).Advanced(), m_glowReflectScale);
    }
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
