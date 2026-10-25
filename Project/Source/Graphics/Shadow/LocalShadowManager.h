#pragma once
#include <vector>

//=============================
//
// 局所光源(スポットライト・点光源)の影の枠の割り当てとキャッシュ
//
//   ・毎フレーム、ライトのシステムが優先度の高い順(カメラに近い順)に影を要求する。
//     同じライトには前のフレームと同じ枠を割り当てるので、中身を使い回せる
//   ・枠の中身は、ライトの位置や向きが変わったとき、または範囲内で影を落とす物が動いたときだけ
//     RenderSystem::RenderLocalLightShadowsが描き直す(動かないライトの影はキャッシュになる)
//   ・外側の角度がkMaxSpotShadowOuterAngle以下のスポットは2D配列、それより広いスポットと
//     点光源はキューブ配列に描く
//
//=============================
class LocalShadowManager
{
public:
    static constexpr UINT kSpotShadowCount = 4;
    static constexpr UINT kCubeShadowCount = 4;
    // 起動時の解像度(画質設定の「高」と同じ)。画質設定で変わる(QualitySettings)
    static constexpr UINT kSpotShadowResolution = 1024;
    static constexpr UINT kCubeShadowResolution = 512;
    // 影の投影の手前の面(m)。シェーダー(inc_Lighting.hlsliのkShadowNear)と合わせる
    static constexpr float kShadowNear = 0.05f;
    // 2D配列で影を描くスポットの外側の角度の上限(度)。これより広いと解像度が足りないのでキューブで描く
    static constexpr float kMaxSpotShadowOuterAngle = 60.0f;
    // スポットの影の視野は外側の角度より少し広げる(ラジアン。シェーダーのkSpotShadowFovMarginと合わせる)
    static constexpr float kSpotShadowFovMargin = 0.0349066f; // 2度

    struct Slot
    {
        uint32_t lightKey = UINT32_MAX; // 割り当てたライト(エンティティの番号)。UINT32_MAXなら空き
        uint64_t lastRequestedFrame = 0;
        bool isRequested = false;       // このフレームで使われるか
        bool isValid = false;           // 中身が今の位置・向きで描かれているか
        Math::Vector3 position;
        Math::Vector3 direction;        // スポットのみ
        float fovRadians = 0.0f;        // スポットのみ
        float range = 0.0f;
        Math::Matrix view[6];           // スポットは[0]だけ使う
        Math::Matrix projection;
    };

    static LocalShadowManager& Instance();

    // 毎フレーム、ライトのシステムが影を要求する前に呼ぶ
    void BeginFrame();

    // すべての枠の中身を古いものとして扱い、次に使うときに描き直させる(影の解像度を変えたときなど)
    void InvalidateAll();

    // スポットライトの影を要求する。戻り値は2D配列の枚の番号(-1なら影なし)。outViewProjに影の行列を返す
    int RequestSpotShadow(uint32_t lightKey, const Math::Vector3& position, const Math::Vector3& direction,
        float outerAngleRadians, float range, Math::Matrix& outViewProj);

    // 点光源(または広角のスポット)の影を要求する。戻り値はキューブ配列の個の番号(-1なら影なし)
    int RequestCubeShadow(uint32_t lightKey, const Math::Vector3& position, float range);

    std::vector<Slot>& GetSpotSlots() { return m_spotSlots; }
    std::vector<Slot>& GetCubeSlots() { return m_cubeSlots; }

    // キューブの投影の奥の面。シェーダーも同じ式(max(range, 1))で求める
    static float GetCubeFar(float range) { return (std::max)(range, 1.0f); }

private:
    LocalShadowManager();

    int AcquireSlot(std::vector<Slot>& slots, uint32_t lightKey);

    std::vector<Slot> m_spotSlots;
    std::vector<Slot> m_cubeSlots;
    uint64_t m_frame = 1;
};
