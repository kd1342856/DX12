#include "../../Pch.h"
#include "ReferenceCompare.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../../Framework/Manager/Asset/TextureManager.h"

bool ReferenceCompare::LoadImageFile(const std::string& filePath)
{
    std::error_code errorCode;
    if (filePath.empty() || !std::filesystem::exists(std::filesystem::u8path(filePath), errorCode))
    {
        return false;
    }
    // 読むたびに別のキーにする(同じファイルを描き直して読み直したときに、前の絵を使わないように)。
    // 前の見本のテクスチャは、まだGPUが使っているかもしれないので解放しない(開発用なので少しの残りは許す)
    ++m_imageLoadCount;
    const std::string cacheKey = "ReferenceCompare#" + std::to_string(m_imageLoadCount) + "#" + filePath;
    m_imageHandle = TextureManager::Instance().LoadTextureExact(cacheKey, filePath);
    m_hasImage = m_imageHandle.IsValid();
    m_imageFilePath = filePath;
    return m_hasImage;
}

bool ReferenceCompare::IsImageReady() const
{
    if (!m_hasImage)
    {
        return false;
    }
    Texture* pTexture = TextureManager::Instance().Get(m_imageHandle);
    return pTexture && pTexture->IsReady();
}

int ReferenceCompare::GetReferenceSRVIndex() const
{
    if (m_source == Source::Snapshot)
    {
        return m_upSnapshot ? m_upSnapshot->GetSRVIndex() : -1;
    }
    if (!IsImageReady())
    {
        return -1;
    }
    return TextureManager::Instance().Get(m_imageHandle)->GetSRVNumber();
}

void ReferenceCompare::FillConstants(CBufferData::PostProcess& data) const
{
    const bool isUsable = m_mode != Mode::Off && GetReferenceSRVIndex() != -1 && !GraphicsDevice::Instance().IsHDROutput();
    data.CompareMode = isUsable ? static_cast<float>(m_mode) : 0.0f;
    data.CompareWipePosition = m_wipePosition;
    data.CompareDifferenceGain = m_differenceGain;
}

RenderTarget* ReferenceCompare::BeginSnapshot(int width, int height)
{
    if (!m_isSnapshotRequested || GraphicsDevice::Instance().IsHDROutput())
    {
        m_isSnapshotRequested = false;
        return nullptr;
    }
    m_isSnapshotRequested = false;
    if (!m_upSnapshot || m_upSnapshot->GetWidth() != width || m_upSnapshot->GetHeight() != height)
    {
        // 最終合成のSDRのPSOと同じ形式(バックバッファと同じ)。深度は使わない
        auto upSnapshot = std::make_unique<RenderTarget>();
        if (!upSnapshot->Create(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, false))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ReferenceCompare] スナップショットのRTを作れませんでした");
            return nullptr;
        }
        // 前のRTは、このフレームの最終合成やGPUがまだ読んでいるかもしれないので、すぐには解放しない。
        // 次に大きさが変わったとき(何フレームも後)に解放する
        m_upRetiredSnapshot = std::move(m_upSnapshot);
        m_upSnapshot = std::move(upSnapshot);
    }
    return m_upSnapshot.get();
}

void ReferenceCompare::FinishSnapshot()
{
    if (m_upSnapshot)
    {
        GraphicsDevice::Instance().TransitionToSRV(m_upSnapshot.get());
    }
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[ReferenceCompare] 今の画面をスナップショットに撮りました");
}
