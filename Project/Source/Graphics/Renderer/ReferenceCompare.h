#pragma once
#include <memory>
#include <string>
#include "../../Framework/Manager/Asset/AssetHandle.h"

class RenderTarget;
class Texture;
namespace CBufferData
{
    struct PostProcess;
}

//=============================
//
// リファレンス比較
//
//   最終の表示の色(トーンマッピングとsRGBの後)を、見本と比べる。見本は次のどちらか。
//     ・スナップショット: 今の画面を撮っておいたもの(設定を変える前と後を比べる)
//     ・画像ファイル: Cyclesなどで描いた正解の絵(PNG・JPGなど。ミップと圧縮をかけずにそのまま読む)
//   比べ方は、画面を左右に分けて左に見本を出す「ワイプ」と、差の絶対値を強調して出す「差分」。
//   比べる処理は最終合成のシェーダー(PostProcessShader_PS.hlsl)の最後で行う。HDR出力のときは使えない。
//
//=============================
class ReferenceCompare
{
public:
    enum class Mode : int
    {
        Off = 0,
        Wipe = 1,
        Difference = 2,
    };

    enum class Source : int
    {
        Snapshot = 0,
        ImageFile = 1,
    };

    static ReferenceCompare& Instance()
    {
        static ReferenceCompare instance;
        return instance;
    }

    Mode GetMode() const { return m_mode; }
    void SetMode(Mode mode) { m_mode = mode; }
    Source GetSource() const { return m_source; }
    void SetSource(Source source) { m_source = source; }
    float GetWipePosition() const { return m_wipePosition; }
    void SetWipePosition(float position) { m_wipePosition = position; }
    float GetDifferenceGain() const { return m_differenceGain; }
    void SetDifferenceGain(float gain) { m_differenceGain = gain; }

    // 次のフレームの最終合成の後で、今の画面をスナップショットに撮る
    void RequestSnapshot() { m_isSnapshotRequested = true; }
    bool HasSnapshot() const { return m_upSnapshot != nullptr; }

    // 画像ファイルを読む(読み込みは後で終わる)。失敗したらfalse
    bool LoadImageFile(const std::string& filePath);
    const std::string& GetImageFilePath() const { return m_imageFilePath; }
    // 画像が読み終わって使えるか
    bool IsImageReady() const;

    // 見本のSRVの番号。使える見本が無ければ-1
    int GetReferenceSRVIndex() const;
    // 最終合成の定数バッファに、比べ方を書く(使えないときはOffにする)
    void FillConstants(CBufferData::PostProcess& data) const;

    // スナップショットを撮るなら、撮るためのRTを用意して返す。撮らないならnullptr。
    // 呼び出し側は返ったRTへ、比べるのを切った最終合成を描き、終わったらFinishSnapshotを呼ぶ
    RenderTarget* BeginSnapshot(int width, int height);
    void FinishSnapshot();

private:
    ReferenceCompare() = default;

    Mode m_mode = Mode::Off;
    Source m_source = Source::Snapshot;
    float m_wipePosition = 0.5f;
    float m_differenceGain = 4.0f;

    bool m_isSnapshotRequested = false;
    std::unique_ptr<RenderTarget> m_upSnapshot;
    std::unique_ptr<RenderTarget> m_upRetiredSnapshot; // 大きさが変わって使わなくなったRT(GPUが読み終えるまで残す)

    std::string m_imageFilePath;
    AssetHandle<Texture> m_imageHandle;
    bool m_hasImage = false;
    int m_imageLoadCount = 0;
};
