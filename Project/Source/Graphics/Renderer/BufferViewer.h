#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

//=============================
//
// バッファの可視化(シェーダーエディターの「バッファの一覧」)
//
//   深度・法線・SSAO・速度などの途中のバッファを、そのままでは真っ黒や真っ白で見えないので、
//   コンピュートで見える色(sRGBのRGBA8)に直した小さなテクスチャへ書き、ImGuiで並べて出す。
//   選んだ1つは大きいテクスチャにも書く。パネルが毎フレームRequest()を呼んでいる間だけ動く。
//
//=============================
class BufferViewer
{
public:
    static BufferViewer& Instance()
    {
        static BufferViewer instance;
        return instance;
    }

    enum class Buffer
    {
        SceneColor,
        Depth,
        Normal,
        Roughness,
        SSAO,
        Velocity,
        Bloom,
        TAAResult,
        TAAChange,
        Count,
    };
    static constexpr int kBufferCount = static_cast<int>(Buffer::Count);

    struct BufferInfo
    {
        const char* pLabel;       // 表示名(UTF-8)
        const char* pDescription; // 説明(UTF-8)
    };
    static const BufferInfo& GetInfo(Buffer buffer);

    // 見たいときに毎フレーム呼ぶ。呼ばなければ何もしない
    void Request() { m_isRequested = true; }
    // 大きく出すバッファ
    void SetSelected(Buffer buffer) { m_selected = buffer; }
    Buffer GetSelected() const { return m_selected; }

    // ポストの後に呼ぶ。projectionはそのフレームのカメラの投影(深度から距離を戻すのに使う)
    void Dispatch(const Math::Matrix& projection);

    // コンピュートが使えないGPUではfalse
    bool IsAvailable() const { return ComputeShader::IsSupported() && !m_hasFailed; }
    // 一度でも書いたか(書く前の中身は不定なので出さない)
    bool HasContent() const { return m_hasContent; }
    // ImGuiのヒープのSRVの番号
    int GetThumbnailImGuiSRVIndex(Buffer buffer) const { return m_thumbnailImGuiSrvIndices[static_cast<int>(buffer)]; }
    int GetLargeImGuiSRVIndex() const { return m_largeImGuiSrvIndex; }
    // 元のバッファの大きさ(説明に出す)
    void GetSourceSize(Buffer buffer, int& outWidth, int& outHeight) const;

    static constexpr UINT kThumbnailWidth = 320;
    static constexpr UINT kThumbnailHeight = 180;
    static constexpr UINT kLargeWidth = 960;
    static constexpr UINT kLargeHeight = 540;

private:
    BufferViewer() = default;
    bool EnsureResources();
    void Convert(Buffer buffer, GPUTexture& destination);

    ComputeShader m_shader;
    GPUTexture m_thumbnails[kBufferCount];
    GPUTexture m_large;
    int m_thumbnailImGuiSrvIndices[kBufferCount] = {};
    int m_largeImGuiSrvIndex = -1;
    bool m_isCreated = false;
    bool m_hasFailed = false;
    bool m_hasContent = false;
    bool m_isRequested = false;
    Buffer m_selected = Buffer::SceneColor;
};
