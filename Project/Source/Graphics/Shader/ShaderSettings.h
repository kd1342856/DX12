#pragma once
#include "SettingsField.h"

enum class DebugView : int
{
    Final = 0, Albedo, Normal, WorldNormal, Roughness, Metallic, AO, Depth, Position, Emissive
};

struct DebugViewItem
{
    DebugView Value;
    const char* Name;
};

constexpr DebugViewItem DebugViewNames[] = {
    { DebugView::Final, "Final" },
    { DebugView::Albedo, "Albedo" },
    { DebugView::Normal, "Normal" },
    { DebugView::WorldNormal, "World Normal" },
    { DebugView::Roughness, "Roughness" },
    { DebugView::Metallic, "Metallic" },
    { DebugView::AO, "AO" },
    { DebugView::Depth, "Depth" },
    { DebugView::Position, "Position" },
    { DebugView::Emissive, "Emissive" }
};

//------------------------------------------------------------
// 各設定のVisitFieldsは、値の定義の一覧(SettingsField.h)。
// エディターの部品・保存・初期値に戻す・A/B比較・Undo/Redoはすべてこの一覧から作る。
// visit(Field(キー, 表示名, 説明)..., メンバー) の順に並べたとおりにエディターに出る。
//------------------------------------------------------------

struct RendererSettings
{
    bool IsDirty = true;

    // SSR (スクリーンスペース反射)。平面反射(専用の反射カメラ設定)が無いガラス等の
    // フォールバックとして、Opaqueパスの深度+カラーをレイマーチする。
    bool  EnableSSR = true;
    float SSRStepSize = 0.35f; // ビュー空間での1ステップの距離
    float SSROpaqueIntensity = 0.5f; // Opaque材質向けSSR(床・タイル等)の強さ。ガラスのSSRとは別パラメータ

    // コンタクトシャドウ: 光源の方向へ画面の深度を短くたどり、シャドウマップでは拾えない接地の細かい影を足す
    bool  EnableContactShadows = true;
    float ContactShadowLength = 0.2f; // たどる長さ(m)

    // レイトレースの影(画質が「高」以上のときだけ使える)。スポット・点光源・月の影をシャドウマップの代わりにレイで調べる。
    // 光源の大きさの円板から毎フレーム1点ずつ選び、TAAで平均して柔らかい影にする
    bool  EnableRaytracedShadows = false;

    // リファレンスのパストレーサー(開発用)。オンの間は、シーンの色をパストレースの結果で置き換える
    bool  EnableReferencePathTracer = false;
    int   PathTracerMaxBounces = 4; // 拡散の反射を繰り返す回数

    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("EnableSSR", u8"画面の反射(SSR)を使う",
            u8"床やガラスに、画面に映っている物を映り込ませる。画質設定が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"画面の反射(SSR)").NotSaved(), EnableSSR);
        visit(Field("SSROpaqueIntensity", u8"床・タイルの映り込みの強さ",
            u8"床やタイルなど不透明な面の映り込みの強さ。0で映り込まない。粗さ・金属度は見ない近似(非金属のF0=0.04)。")
            .Range(0.0f, 2.0f).When("EnableSSR"), SSROpaqueIntensity);
        visit(Field("SSRStepSize", u8"映り込みを探す刻み",
            u8"映り込む物を画面の深度でたどるときの1歩の長さ。小さいほど細かく正確だが、遠くまで届かない。")
            .Unit("m").Range(0.05f, 1.5f).When("EnableSSR").Advanced(), SSRStepSize);

        visit(Field("EnableContactShadows", u8"接地の細かい影を足す",
            u8"物が床に触れているところなど、シャドウマップでは拾えない小さな影を画面の深度から足す。")
            .Kind(FieldKind::Toggle).Section(u8"接地の影"), EnableContactShadows);
        visit(Field("ContactShadowLength", u8"接地の影をたどる長さ",
            u8"光の方向へ深度をたどる長さ。長いほど大きな影まで拾うが、見当違いの影も出やすい。目安は0.1～0.3 m。")
            .Unit("m").Range(0.02f, 1.0f).When("EnableContactShadows"), ContactShadowLength);

        visit(Field("EnableRaytracedShadows", u8"影をレイトレースで調べる",
            u8"スポット・点光源・月の影を、シャドウマップの代わりにレイで調べる(画質「高」以上)。反射は画質「最高」で自動で使う。\nオプション画面が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"レイトレース").NotSaved(), EnableRaytracedShadows);

        visit(Field("EnableReferencePathTracer", u8"パストレーサーの正解の絵を出す",
            u8"拡散の反射を繰り返す正解の絵に置き換える(カメラを止めると足し続けてノイズが消える)。\nラスタライズの描画(DDGI・反射プローブ・影)と見比べる。鏡面・スキニングのモデル・半透明は写らない。開発用なので保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"パストレーサー(開発用)").NotSaved(), EnableReferencePathTracer);
        visit(Field("PathTracerMaxBounces", u8"反射を繰り返す回数",
            u8"光が拡散で跳ね返る回数。多いほど部屋の奥まで明るくなるが、1枚にかかる時間が延びる。")
            .Kind(FieldKind::Integer).Unit(u8"回").Range(0.0f, 8.0f), PathTracerMaxBounces);
    }
};

struct LightingSettings
{
    bool IsDirty = true;
    DirectX::XMFLOAT3 DirectionalLightDir = { 0.5f, -1.0f, 0.3f };
    // 平行光(月)。色は色温度(K。0以下なら使わない)×色味を輝度1に正規化したもので、明るさは照度(lx)で持つ
    DirectX::XMFLOAT3 DirectionalLightColor = { 1.0f, 1.0f, 1.0f };
    float DirectionalLightTemperature = 4100.0f;
    float DirectionalLightIntensity = 0.1f; // 照度 lx(満月の夜で0.1～0.3)
    // 環境光の輝度 nit(cd/m2)。Step 5のGIで置き換えるまでの仮の値
    DirectX::XMFLOAT3 AmbientLight = { 0.02f, 0.02f, 0.02f };
    // 空(スカイスフィアのテクスチャ)の明るさ。テクスチャの値1.0が何nitになるか
    float SkyLuminance = 0.05f;

    // Indoor mask: pixels inside any RoomArea AABB never receive outside light (moon/ambient/sky),
    // treated as fully in the moon's shadow regardless of whether the shadow map actually covers them.
    bool EnableIndoorMask = true;
    float IndoorMargin = 0.3f; // grows each RoomArea AABB by this much (m) so walls/floors on the boundary count as indoor

    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("DirectionalLightIntensity", u8"月の明るさ(照度)",
            u8"月の光が真上から当たる面の明るさ。目安：新月の夜 0.001 lx、満月の夜 0.1～0.3 lx、薄曇りの昼 10000 lx。")
            .Unit("lx").Range(0.001f, 100000.0f).Logarithmic().Section(u8"月の光")
            .Ref(u8"新月の夜(星明かり)", 0.001f).Ref(u8"半月の夜", 0.03f).Ref(u8"満月の夜", 0.25f).Ref(u8"日没後の薄明", 10.0f)
            .Ref(u8"曇りの昼", 10000.0f).Ref(u8"晴れの昼", 100000.0f), DirectionalLightIntensity);
        visit(Field("DirectionalLightTemperature", u8"月の光の色温度",
            u8"光の色を色温度で決める。低いほど赤く、高いほど青い。目安：ロウソク 1900 K、電球 2700 K、月 4100 K、昼光 6500 K。0で使わない(色味だけ)。")
            .Kind(FieldKind::Temperature).Unit("K").Range(1000.0f, 12000.0f)
            .Ref(u8"ロウソク", 1850.0f).Ref(u8"白熱電球", 2700.0f).Ref(u8"月の光", 4100.0f).Ref(u8"昼白色の蛍光灯", 5000.0f).Ref(u8"昼光", 6500.0f), DirectionalLightTemperature);
        visit(Field("DirectionalLightColor", u8"月の光の色味",
            u8"色温度の色に掛ける色。白で色温度のまま。明るさは照度で決まるので、ここでは色合いだけを変える。")
            .Kind(FieldKind::Color), DirectionalLightColor);
        visit(Field("DirectionalLightDir", u8"月の光の向き",
            u8"光が進む向き(XYZ)。Yが負で上から下へ照らす。長さは1にそろえる。")
            .Kind(FieldKind::Direction).Range(-1.0f, 1.0f).Advanced(), DirectionalLightDir);

        visit(Field("SkyLuminance", u8"空の明るさ",
            u8"空(スカイスフィア)の明るさ。テクスチャの白(1.0)が何nitになるか。目安：月夜の空 0.01～0.1 nit。")
            .Unit("nit").Range(0.001f, 10000.0f).Logarithmic().Section(u8"空と環境光")
            .Ref(u8"月の無い夜空", 0.001f).Ref(u8"満月の夜空", 0.05f).Ref(u8"日没後の空", 10.0f).Ref(u8"曇りの昼の空", 2000.0f), SkyLuminance);
        visit(Field("AmbientLight", u8"家の外の環境光",
            u8"DDGIのボリューム(家)の外で使う、一様な空からの光の明るさ。拡散と鏡面の両方に使う。")
            .Kind(FieldKind::ColorHDR).Unit("nit").Range(0.0f, 1000.0f), AmbientLight);

        visit(Field("EnableIndoorMask", u8"部屋の中に月の光を入れない",
            u8"RoomAreaの範囲の中には直射の月光が届かないものとして扱う(屋根のシャドウマップが届かないときの漏れを防ぐ)。間接光はDDGIが求める。")
            .Kind(FieldKind::Toggle).Section(u8"部屋の中の判定"), EnableIndoorMask);
        visit(Field("IndoorMargin", u8"部屋の範囲を広げる幅",
            u8"RoomAreaの範囲をこの分だけ広げて判定する(境界の上の壁・床も部屋の中として扱うため)。")
            .Unit("m").Range(0.0f, 2.0f).When("EnableIndoorMask").Advanced(), IndoorMargin);
    }
};

struct ShadowSettings
{
    bool IsDirty = true;
    bool EnableShadows = true;
    float ShadowPower = 2.5f;
    float ShadowBias = 0.001f;

    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("EnableShadows", u8"月の影を落とす",
            u8"月の光のシャドウマップを使う。切ると月の光は物を通り抜ける。")
            .Kind(FieldKind::Toggle).Section(u8"月の影"), EnableShadows);
        visit(Field("ShadowPower", u8"月の影の濃さ",
            u8"影の効き方。1で影の中は月の光が届かない。1より大きいと影の縁が硬く暗くなり、0で影が消える。")
            .Range(0.0f, 3.0f).When("EnableShadows"), ShadowPower);
        visit(Field("ShadowBias", u8"影のずらし量",
            u8"面が自分の影で縞になる(シャドウアクネ)のを防ぐずらし量。大きすぎると影が物から浮く。")
            .Range(0.0f, 0.01f).When("EnableShadows").Advanced(), ShadowBias);
    }
};

struct PostProcessSettings
{
    bool IsDirty = true;
    bool EnableHDR = true;
    // プレイヤーの明るさ(オプション画面)。ゲームのカメラで描くときだけ掛ける。
    // エディターのカメラには、代わりにエディターの表示ガンマ(ShaderManager::SetEditorDisplayGamma)を掛ける
    float BrightnessGamma = 1.00f;

    // 露出。EV100で持ち、ライトの値に前もって掛けてからシェーダーへ送る(前露出)。
    // UseCameraSettingsがtrueなら、絞り・シャッター速度・ISO感度からEV100を求める
    float EV100 = 6.0f;
    bool  UseCameraSettings = false;
    float Aperture = 2.8f;            // F値
    float ShutterSpeed = 1.0f / 30.0f; // 秒
    float ISO = 800.0f;

    // TAA(テンポラル・アンチエイリアス)。シェーダーモデル6.6に対応していないGPUでは使えない
    bool  EnableTAA = true;
    float TAACurrentWeight = 0.1f; // 今のフレームを混ぜる割合。小さいほど滑らかだが、動きに残像が出やすい

    // 自動露出。オンのときは、EV100(とカメラの設定)の代わりに画面の明るさから露出を決める
    bool  EnableAutoExposure = false;
    // 暗い場面でもこれより明るくはしない(EV100が小さいほど明るく写る)。夜の暗さが残るよう、手動の既定値(6)から2段まで
    float AutoExposureMinEV100 = 4.0f;
    float AutoExposureMaxEV100 = 10.0f;
    float ExposureCompensation = 0.0f;  // 露出の補正(段)。+で明るく
    float AutoExposureLowPercent = 50.0f;  // 平均に使う範囲(暗い方から数えた割合%)。真っ暗な影に引っ張られないように
    float AutoExposureHighPercent = 95.0f; // 光源そのものに引っ張られないように、明るい側も除く
    float AutoExposureSpeedUp = 3.0f;   // 明るい場面へ順応する速さ(1/秒)
    float AutoExposureSpeedDown = 1.0f; // 暗い場面へ順応する速さ(1/秒)。目と同じく暗さへの順応は遅い

    // モーションブラー。ぼけの長さはシャッターが開いていた時間に連動する。
    // UseCameraSettingsがtrueならシャッター速度とフレームの時間から、falseならシャッター角から決める
    bool  EnableMotionBlur = false;
    float MotionBlurShutterAngle = 180.0f; // 度。360で1フレームの間ずっと開いている
    float MotionBlurMaxPixels = 32.0f;     // ぼけの長さの上限(ピクセル)

    // Bloom - 半径・反復回数を上げてはじめて「ふわっと広がる」見た目になる。
    // 閾値/強度だけいじっても変化が薄いのはこのため。
    float BloomThreshold = 1.0f;
    float BloomIntensity = 1.0f;
    float BloomRadius = 8.0f;     // 1回のBlurパスでのぼかし半径(テクセル)
    int   BloomIterations = 3;    // Blur(横+縦)を何回繰り返すか。CPU側のみで使用(GPUには送らない)

    // Vignette
    bool  EnableVignette = true;
    // 自然な周辺減光(cos^4則)の効き。1で画角どおり、0で無し
    float VignetteIntensity = 1.0f;

    // Film Grain
    bool  EnableFilmGrain = false;
    float FilmGrainIntensity = 0.035f; // ISO 800での強さ。ISO感度の平方根に比例して強くなる

    // Chromatic Aberration
    bool  EnableChromaticAberration = true;
    float ChromaticAberrationIntensity = 0.4f;

    // Depth of Field
    bool  EnableDOF = false;
    float DOFFocusDistance = 5.0f;
    float DOFFocusRange = 8.0f;

    // God Rays (平行光の光条。既存の平行光/シャドウマップは使わず、Bloom結果を
    // 光源のスクリーン座標からラジアルブラーする軽量な近似)
    bool  EnableGodRays = false;
    float GodRaysDensity = 0.9f;
    float GodRaysDecay = 0.96f;
    float GodRaysWeight = 0.4f;
    float GodRaysExposure = 1.0f;
    float GodRaysIntensity = 1.0f;
    int   GodRaysNumSamples = 48;

    // BrightnessGammaはオプション画面だけで変えるので、ここには並べない
    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("UseCameraSettings", u8"カメラの設定から露出を決める",
            u8"オンで、絞り・シャッター速度・ISO感度から露出(EV100)を求める。オフで、露出(EV100)を直接決める。")
            .Kind(FieldKind::Toggle).Section(u8"露出"), UseCameraSettings);
        visit(Field("EV100", u8"露出",
            u8"小さいほど明るく写る(1段ごとに2倍)。目安：月夜の屋外 -2～0、暗い室内 2～4、明るい室内 6～8、晴れた昼 15。")
            .Unit("EV100").Range(-6.0f, 16.0f).When("UseCameraSettings", false, true)
            .Ref(u8"月夜の屋外", -2.5f).Ref(u8"ロウソクだけの部屋", -1.5f).Ref(u8"懐中電灯で照らした所", 6.0f).Ref(u8"明るい室内", 7.0f)
            .Ref(u8"曇りの屋外", 12.0f).Ref(u8"晴れの屋外", 15.0f), EV100);
        visit(Field("Aperture", u8"絞り(F値)",
            u8"小さいほど明るく写る。1段(F1.4→F2)で光は半分。被写界深度(ぼけ)を使うときは、小さいほどぼける。")
            .Kind(FieldKind::Aperture).Unit("F").Range(1.0f, 22.0f).When("UseCameraSettings", true, true)
            .Ref(u8"明るいレンズの開放", 1.4f).Ref(u8"スマートフォン", 1.8f).Ref(u8"ふつうのレンズの開放", 2.8f).Ref(u8"風景(全体にピント)", 8.0f), Aperture);
        visit(Field("ShutterSpeed", u8"シャッター速度",
            u8"長いほど明るく写る。モーションブラーのぼけの長さもこれで決まる。目安：1/30～1/60 秒。")
            .Kind(FieldKind::Shutter).Unit(u8"秒").Range(1.0f / 4000.0f, 1.0f).Logarithmic().When("UseCameraSettings", true, true)
            .Ref(u8"映画(シャッター角180度・24fps)", 1.0f / 48.0f).Ref(u8"暗い場面の手持ち", 1.0f / 30.0f).Ref(u8"ふつうの動き", 1.0f / 60.0f)
            .Ref(u8"速い動きを止める", 1.0f / 500.0f), ShutterSpeed);
        visit(Field("ISO", u8"ISO感度",
            u8"大きいほど明るく写る。フィルムグレインも感度に合わせて荒くなる。目安：暗い室内 1600～6400。")
            .Kind(FieldKind::ISO).Range(50.0f, 102400.0f).When("UseCameraSettings", true, true)
            .Ref(u8"晴れの屋外", 100.0f).Ref(u8"室内", 800.0f).Ref(u8"暗い室内", 3200.0f).Ref(u8"夜の屋外(高感度)", 12800.0f), ISO);
        visit(Field("EnableHDR", u8"トーンマッピングする(ACES)",
            u8"明るすぎるところを白く飛ばさず、映画のフィルムのように丸めて表示する。")
            .Kind(FieldKind::Toggle).Advanced(), EnableHDR);

        visit(Field("EnableAutoExposure", u8"自動露出を使う",
            u8"オンで、画面の明るさから露出を決める(目が暗さに慣れるように、ゆっくり合わせる)。")
            .Kind(FieldKind::Toggle).Section(u8"自動露出"), EnableAutoExposure);
        visit(Field("ExposureCompensation", u8"露出の補正",
            u8"自動露出が決めた明るさをずらす。+で明るく、1段で2倍。")
            .Unit("EV").Range(-4.0f, 4.0f).When("EnableAutoExposure", true, true), ExposureCompensation);
        visit(Field("AutoExposureMinEV100", u8"これより明るくしない",
            u8"暗い場面でも、露出をこれより明るくはしない(EV100は小さいほど明るい)。夜の暗さを残すための下限。")
            .Unit("EV100").Range(-10.0f, 16.0f).When("EnableAutoExposure", true, true), AutoExposureMinEV100);
        visit(Field("AutoExposureMaxEV100", u8"これより暗くしない",
            u8"明るい場面でも、露出をこれより暗くはしない。")
            .Unit("EV100").Range(-10.0f, 16.0f).When("EnableAutoExposure", true, true), AutoExposureMaxEV100);
        visit(Field("AutoExposureLowPercent", u8"平均に使う範囲(暗い側)",
            u8"画面の画素を暗い方から並べ、この割合より暗い画素を明るさの平均から除く。真っ暗な影に露出が引っ張られないようにする。")
            .Unit("%").Range(0.0f, 100.0f).When("EnableAutoExposure", true, true).Advanced(), AutoExposureLowPercent);
        visit(Field("AutoExposureHighPercent", u8"平均に使う範囲(明るい側)",
            u8"この割合より明るい画素を平均から除く。光源そのものに露出が引っ張られないようにする。")
            .Unit("%").Range(0.0f, 100.0f).When("EnableAutoExposure", true, true).Advanced(), AutoExposureHighPercent);
        visit(Field("AutoExposureSpeedUp", u8"明るさに慣れる速さ",
            u8"明るい場面へ合わせる速さ。大きいほど早く合う。")
            .Unit(u8"/秒").Range(0.1f, 10.0f).When("EnableAutoExposure", true, true).Advanced(), AutoExposureSpeedUp);
        visit(Field("AutoExposureSpeedDown", u8"暗さに慣れる速さ",
            u8"暗い場面へ合わせる速さ。目と同じく、暗さへの順応は遅めにする。")
            .Unit(u8"/秒").Range(0.1f, 10.0f).When("EnableAutoExposure", true, true).Advanced(), AutoExposureSpeedDown);

        visit(Field("EnableTAA", u8"TAAでギザギザを消す",
            u8"前のフレームと混ぜて、輪郭のギザギザとちらつきを消す。オプション画面が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"アンチエイリアス").NotSaved(), EnableTAA);
        visit(Field("TAACurrentWeight", u8"今のフレームを混ぜる割合",
            u8"小さいほど滑らかでちらつかないが、動いたときに残像が出やすい。")
            .Range(0.03f, 0.5f).When("EnableTAA").Advanced(), TAACurrentWeight);

        visit(Field("EnableMotionBlur", u8"モーションブラーを使う",
            u8"動いた物やカメラの動きをぼかす。オプション画面が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"モーションブラー").NotSaved(), EnableMotionBlur);
        visit(Field("MotionBlurShutterAngle", u8"シャッター角",
            u8"1フレームのうちシャッターが開いている割合(度)。360で1フレームの間ずっと開いている。カメラの設定を使うときはシャッター速度で決まる。")
            .Unit(u8"度").Range(0.0f, 360.0f).When("UseCameraSettings", false, true), MotionBlurShutterAngle);
        visit(Field("MotionBlurMaxPixels", u8"ぼけの長さの上限",
            u8"ぼけを伸ばす長さの上限。速い動きで絵が崩れないようにする。")
            .Unit("px").Range(4.0f, 128.0f).When("EnableMotionBlur").Advanced(), MotionBlurMaxPixels);

        visit(Field("BloomIntensity", u8"光のにじみの強さ",
            u8"明るいところからにじむ光(ブルーム)の強さ。0でにじまない。")
            .Range(0.0f, 3.0f).Section(u8"光のにじみ(ブルーム)"), BloomIntensity);
        visit(Field("BloomThreshold", u8"にじみ始める明るさ",
            u8"これより明るいところだけがにじむ。下げるほど暗いところまでにじむ。")
            .Range(0.0f, 5.0f), BloomThreshold);
        visit(Field("BloomRadius", u8"にじみの広さ",
            u8"1回のぼかしの半径。広げてはじめて「ふわっと広がる」見た目になる。強さだけ上げても変化が薄いときはここを上げる。")
            .Unit("px").Range(1.0f, 24.0f).Advanced(), BloomRadius);
        visit(Field("BloomIterations", u8"ぼかしを繰り返す回数",
            u8"ぼかしを繰り返す回数。多いほど広く柔らかくにじむ。画質設定が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Integer).Unit(u8"回").Range(1.0f, 8.0f).NotSaved().Advanced(), BloomIterations);

        visit(Field("EnableVignette", u8"周辺を暗くする",
            u8"レンズの自然な周辺減光(cos^4則)。画角から求める。")
            .Kind(FieldKind::Toggle).Section(u8"レンズ"), EnableVignette);
        visit(Field("VignetteIntensity", u8"周辺の暗さ",
            u8"周辺減光の効き。1で物理どおり、0で無し。")
            .Range(0.0f, 3.0f).When("EnableVignette"), VignetteIntensity);
        visit(Field("EnableChromaticAberration", u8"色のにじみ(色収差)を出す",
            u8"画面の端ほど、赤と青が少しずれてにじむ。")
            .Kind(FieldKind::Toggle), EnableChromaticAberration);
        visit(Field("ChromaticAberrationIntensity", u8"色のにじみの強さ",
            u8"色収差のずれの大きさ。")
            .Range(0.0f, 3.0f).When("EnableChromaticAberration"), ChromaticAberrationIntensity);
        visit(Field("EnableFilmGrain", u8"フィルムの粒を出す",
            u8"画面に細かい粒(ノイズ)を足す。ISO感度が高いほど荒くなる。")
            .Kind(FieldKind::Toggle), EnableFilmGrain);
        visit(Field("FilmGrainIntensity", u8"粒の強さ(ISO 800のとき)",
            u8"ISO 800での粒の強さ。ISO感度の平方根に比例して強くなり、暗いところほど粒の量は小さい。")
            .Range(0.0f, 0.3f).When("EnableFilmGrain"), FilmGrainIntensity);

        visit(Field("EnableDOF", u8"ピントの外をぼかす(被写界深度)",
            u8"ピントの合う距離から外れたところをぼかす。")
            .Kind(FieldKind::Toggle).Section(u8"被写界深度"), EnableDOF);
        visit(Field("DOFFocusDistance", u8"ピントの合う距離",
            u8"カメラからこの距離にピントが合う。")
            .Unit("m").Range(0.1f, 50.0f).When("EnableDOF"), DOFFocusDistance);
        visit(Field("DOFFocusRange", u8"ピントの合う幅",
            u8"ピントの合う距離の前後で、くっきり見える幅。")
            .Unit("m").Range(0.1f, 30.0f).When("EnableDOF"), DOFFocusRange);

        visit(Field("EnableGodRays", u8"光の筋(ゴッドレイ)を出す",
            u8"月の方向からブルームの光を放射状に伸ばす近似。月がカメラの後ろにあるときは自動で消える。")
            .Kind(FieldKind::Toggle).Section(u8"光の筋(ゴッドレイ)"), EnableGodRays);
        visit(Field("GodRaysIntensity", u8"光の筋の強さ",
            u8"光の筋の明るさ。")
            .Range(0.0f, 3.0f).When("EnableGodRays"), GodRaysIntensity);
        visit(Field("GodRaysDensity", u8"光の筋の長さ",
            u8"光源から伸ばす長さの割合。")
            .Range(0.1f, 1.5f).When("EnableGodRays").Advanced(), GodRaysDensity);
        visit(Field("GodRaysDecay", u8"光の筋の減り方",
            u8"光源から離れるほど弱くする割合。1に近いほど遠くまで届く。")
            .Range(0.8f, 0.999f).When("EnableGodRays").Advanced(), GodRaysDecay);
        visit(Field("GodRaysWeight", u8"光の筋の重み",
            u8"1回のサンプルの重み。")
            .Range(0.0f, 2.0f).When("EnableGodRays").Advanced(), GodRaysWeight);
        visit(Field("GodRaysExposure", u8"光の筋の露出",
            u8"最後に掛ける明るさの倍率。")
            .Range(0.0f, 3.0f).When("EnableGodRays").Advanced(), GodRaysExposure);
        visit(Field("GodRaysNumSamples", u8"光の筋のサンプル数",
            u8"多いほど筋がなめらかになるが重くなる。")
            .Kind(FieldKind::Integer).Range(8.0f, 96.0f).When("EnableGodRays").Advanced(), GodRaysNumSamples);
    }
};

struct DebugSettings
{
    bool IsDirty = true;
    DebugView CurrentDebugView = DebugView::Final;
};

struct SSAOSettings
{
    bool  EnableSSAO = true;
    float Radius = 0.5f;
    float Bias = 0.03f;
    float Power = 1.5f;
    float Intensity = 1.0f;

    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("EnableSSAO", u8"すき間を暗くする(SSAO)",
            u8"画面の深度から、物と物のすき間や角を暗くする(環境光にだけ効く)。スキニングのモデルと空は含まない。\n画質設定が決める値なので、シーンには保存しない。")
            .Kind(FieldKind::Toggle).Section(u8"すき間の陰り(SSAO)").NotSaved(), EnableSSAO);
        visit(Field("Intensity", u8"陰りの強さ",
            u8"すき間を暗くする強さ。0で効かない。")
            .Range(0.0f, 2.0f).When("EnableSSAO"), Intensity);
        visit(Field("Radius", u8"陰りを探す半径",
            u8"周りの物を探す半径。大きいほど広い範囲が暗くなる。")
            .Unit("m").Range(0.05f, 3.0f).When("EnableSSAO"), Radius);
        visit(Field("Power", u8"陰りのコントラスト",
            u8"大きいほど暗いところがより暗く、くっきりする。")
            .Range(0.2f, 4.0f).When("EnableSSAO").Advanced(), Power);
        visit(Field("Bias", u8"陰りのずらし量",
            u8"平らな面が自分で暗くなる(まだら)のを防ぐずらし量。")
            .Range(0.0f, 0.3f).When("EnableSSAO").Advanced(), Bias);
    }
};

struct FogSettings
{
    bool IsDirty = true;

    // Distance fog (always present at FogDensity from the start of a match; grows to
    // HuntFogDensity while a ghost is hunting)
    bool EnableFog = true;
    DirectX::XMFLOAT3 FogColor = { 0.5f, 0.5f, 0.55f };
    float FogDensity = 0.02f;      // Density under normal conditions
    float HuntFogDensity = 0.18f;  // Density while a ghost is hunting

    // Screen darkening while a ghost is hunting (scales ambient + directional light down)
    bool EnableHuntDarken = true;
    float HuntDarkenAmount = 0.6f; // 0 = no darkening, 1 = lights fully off at full hunt intensity

    // Shared normal<->hunt blend rate, in "fraction per second" (1.0 = full transition in ~1s).
    // Drives both the fog density interpolation and the darkening above, so they move together.
    float HuntTransitionSpeed = 0.8f;

    // ボリュメトリックフォグ(フロクセル)。オンのときは、上の濃さを消散係数(1/m)として使い、
    // FogColorを散乱の割合(色)として使う。霧の明るさはライト・影・DDGIの環境光から求まる
    bool  EnableVolumetricFog = true;
    float VolumetricFogAnisotropy = 0.3f;    // 正で前方散乱(光の方を向くと霧が明るく光る)
    float VolumetricFogFar = 40.0f;          // フロクセルの奥の端(m)。家の大きさに合わせる
    float VolumetricFogHistoryWeight = 0.9f; // 前のフレームの結果を残す割合

    template<typename TVisitor>
    void VisitFields(TVisitor&& visit)
    {
        visit(Field("EnableFog", u8"霧を出す",
            u8"試合の始めから薄い霧を出し、幽霊がハントしている間は濃くする。")
            .Kind(FieldKind::Toggle).Section(u8"霧"), EnableFog);
        visit(Field("FogDensity", u8"霧の濃さ(ふだん)",
            u8"1 mあたりに光が霧に遮られる割合(消散係数)。目安：薄いもや 0.01、濃い霧 0.1 以上。")
            .Unit("1/m").Range(0.0005f, 1.0f).Logarithmic().When("EnableFog")
            .Ref(u8"薄いもや(見通し約300 m)", 0.01f).Ref(u8"霧(見通し約60 m)", 0.05f).Ref(u8"濃い霧(見通し約15 m)", 0.2f), FogDensity);
        visit(Field("HuntFogDensity", u8"霧の濃さ(ハント中)",
            u8"幽霊がハントしている間の霧の濃さ。")
            .Unit("1/m").Range(0.0005f, 1.0f).Logarithmic().When("EnableFog")
            .Ref(u8"薄いもや(見通し約300 m)", 0.01f).Ref(u8"霧(見通し約60 m)", 0.05f).Ref(u8"濃い霧(見通し約15 m)", 0.2f), HuntFogDensity);
        visit(Field("FogColor", u8"霧の色",
            u8"霧が光を散らす割合(色)。ボリュメトリックフォグでは、霧の明るさはライト・影・DDGIの環境光から求まる。")
            .Kind(FieldKind::Color).When("EnableFog"), FogColor);

        visit(Field("EnableHuntDarken", u8"ハント中に画面を暗くする",
            u8"幽霊がハントしている間、環境光と月の光を弱くする。")
            .Kind(FieldKind::Toggle).Section(u8"ハントの演出"), EnableHuntDarken);
        visit(Field("HuntDarkenAmount", u8"暗くする量",
            u8"0で暗くしない、1でハントが最大のとき光が消える。")
            .Range(0.0f, 1.0f).When("EnableHuntDarken"), HuntDarkenAmount);
        visit(Field("HuntTransitionSpeed", u8"ハントへの切り替えの速さ",
            u8"ふだんとハント中の間を移る速さ(1秒あたりの割合)。1でおよそ1秒。霧の濃さと暗さの両方に効く。")
            .Unit(u8"/秒").Range(0.01f, 10.0f).Advanced(), HuntTransitionSpeed);

        visit(Field("EnableVolumetricFog", u8"光の当たる霧(ボリュメトリック)",
            u8"霧の中を光が通る様子を立体的に求める(ライトの光の筋や影が霧に見える)。")
            .Kind(FieldKind::Toggle).Section(u8"ボリュメトリックフォグ").When("EnableFog"), EnableVolumetricFog);
        visit(Field("VolumetricFogAnisotropy", u8"光の方向への散らばり",
            u8"正で前方散乱(光の方を向くと霧が明るく光る)。0で全方向に同じ。")
            .Range(-0.9f, 0.9f).When("EnableVolumetricFog").Advanced(), VolumetricFogAnisotropy);
        visit(Field("VolumetricFogFar", u8"霧を求める奥行き",
            u8"立体の霧を求める奥の端。家の大きさに合わせる。")
            .Unit("m").Range(5.0f, 100.0f).When("EnableVolumetricFog").Advanced(), VolumetricFogFar);
        visit(Field("VolumetricFogHistoryWeight", u8"前のフレームを残す割合",
            u8"大きいほどちらつかないが、明るさの変化への追従が遅れる。")
            .Range(0.0f, 0.98f).When("EnableVolumetricFog").Advanced(), VolumetricFogHistoryWeight);
    }
};
