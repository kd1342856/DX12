#include "../../../Pch.h"
#include "TextureCache.h"
#include "../../Shader/ShaderCompiler/ShaderCompiler.h"

namespace
{
    // これより小さいテクスチャは、圧縮しても得が少ないのでキャッシュしない
    constexpr size_t kMinCacheSize = 64;

#ifdef _DEBUG
    // Debugビルドでは圧縮しない。最適化していないDirectXTexではBC7の圧縮に1枚で数分かかり、途中でやめられるのも
    // ブロックの1行ごとなので、ウィンドウを閉じた後もプロセスが残る。Releaseで作ったキャッシュは読む
    constexpr bool kCanCompress = false;
#else
    constexpr bool kCanCompress = true;
#endif
}

TextureCache& TextureCache::Instance()
{
    static TextureCache instance;
    return instance;
}

TextureCache::~TextureCache()
{
    Shutdown();
}

std::filesystem::path TextureCache::GetCacheDirectory()
{
    return AppPaths::GetSavedDir() / L"TextureCache";
}

std::filesystem::path TextureCache::GetCachePath(uint64_t key)
{
    wchar_t fileName[32];
    swprintf_s(fileName, L"%016llx.dds", static_cast<unsigned long long>(key));
    return GetCacheDirectory() / fileName;
}

uint64_t TextureCache::MakeFileKey(const std::string& filePath, bool isSrgb)
{
    // 中身が変わったら作り直すよう、大きさと更新日時もキーに含める
    std::error_code errorCode;
    const std::filesystem::path path(filePath);
    const uint64_t fileSize = static_cast<uint64_t>(std::filesystem::file_size(path, errorCode));
    const auto writeTime = std::filesystem::last_write_time(path, errorCode).time_since_epoch().count();
    uint64_t hash = ShaderCompiler::HashBytes(filePath.data(), filePath.size());
    hash = ShaderCompiler::HashBytes(&fileSize, sizeof(fileSize), hash);
    hash = ShaderCompiler::HashBytes(&writeTime, sizeof(writeTime), hash);
    const uint32_t settings[2] = { kVersion, isSrgb ? 1u : 0u };
    return ShaderCompiler::HashBytes(settings, sizeof(settings), hash);
}

uint64_t TextureCache::MakeMemoryKey(const void* pData, size_t size, bool isSrgb)
{
    uint64_t hash = ShaderCompiler::HashBytes(pData, size);
    const uint32_t settings[2] = { kVersion, isSrgb ? 1u : 0u };
    return ShaderCompiler::HashBytes(settings, sizeof(settings), hash);
}

bool TextureCache::TryLoad(uint64_t key, DirectX::ScratchImage& outImage) const
{
    const std::filesystem::path path = GetCachePath(key);
    std::error_code errorCode;
    if (!std::filesystem::exists(path, errorCode))
    {
        return false;
    }
    DirectX::TexMetadata metadata = {};
    if (FAILED(DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, outImage)))
    {
        // 壊れたキャッシュは消して、元の画像から読み直す
        std::filesystem::remove(path, errorCode);
        return false;
    }
    return true;
}

void TextureCache::RequestStore(uint64_t key, DirectX::ScratchImage&& image)
{
    if constexpr (!kCanCompress)
    {
        return;
    }
    const DirectX::TexMetadata& metadata = image.GetMetadata();
    // BC7は4x4のブロックで圧縮するので、ミップ0が4の倍数の大きさのものだけ
    if (metadata.width < kMinCacheSize || metadata.height < kMinCacheSize ||
        (metadata.width % 4) != 0 || (metadata.height % 4) != 0 || DirectX::IsCompressed(metadata.format))
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isStopping)
    {
        return;
    }
    if (!m_isStarted)
    {
        m_isStarted = true;
        m_worker = std::thread([this]() { WorkerLoop(); });
    }
    Job job;
    job.key = key;
    job.image = std::move(image);
    m_jobs.push_back(std::move(job));
    m_condition.notify_one();
}

void TextureCache::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isStopping = true;
        m_jobs.clear();
    }
    m_condition.notify_all();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void TextureCache::WorkerLoop()
{
    // 描画やゲームの処理を邪魔しないよう、低い優先度で動かす
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    while (true)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() { return m_isStopping || !m_jobs.empty(); });
            if (m_isStopping)
            {
                return;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        Compress(job);
    }
}

void TextureCache::Compress(Job& job)
{
    const DirectX::TexMetadata& metadata = job.image.GetMetadata();
    const DXGI_FORMAT compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
    DirectX::TexMetadata compressedMetadata = metadata;
    compressedMetadata.format = compressedFormat;
    DirectX::ScratchImage compressed;
    if (FAILED(compressed.Initialize(compressedMetadata)))
    {
        return;
    }
    // 速さを優先したBC7の圧縮(品質はほとんど変わらない)。ゲームの処理を邪魔しないよう、このスレッド1本だけで行う。
    // DirectXTexが途中でやめられるのは画像1枚ごとで、大きいテクスチャの1枚にはReleaseでも数秒かかる。
    // 終了するときにすぐやめられるよう、1枚を横長の帯に分けて圧縮し、帯ごとに終了かを見る
    constexpr size_t kStripRows = 64; // 4の倍数(BC7のブロックの高さ)
    for (size_t imageIndex = 0; imageIndex < job.image.GetImageCount(); ++imageIndex)
    {
        const DirectX::Image& source = job.image.GetImages()[imageIndex];
        const DirectX::Image& destination = compressed.GetImages()[imageIndex];
        for (size_t row = 0; row < source.height; row += kStripRows)
        {
            if (m_isStopping)
            {
                return;
            }
            DirectX::Image strip = source;
            strip.height = (std::min)(kStripRows, source.height - row);
            strip.pixels = source.pixels + row * source.rowPitch;
            strip.slicePitch = strip.rowPitch * strip.height;
            DirectX::ScratchImage compressedStrip;
            if (FAILED(DirectX::Compress(strip, compressedFormat, DirectX::TEX_COMPRESS_BC7_QUICK, DirectX::TEX_THRESHOLD_DEFAULT, compressedStrip)))
            {
                return;
            }
            // 帯のブロックの行を、全体の画像の同じ位置へ写す
            const DirectX::Image& stripBlocks = *compressedStrip.GetImage(0, 0, 0);
            const size_t blockRowCount = (strip.height + 3) / 4;
            const size_t copyBytes = (std::min)(stripBlocks.rowPitch, destination.rowPitch);
            for (size_t blockRow = 0; blockRow < blockRowCount; ++blockRow)
            {
                std::memcpy(destination.pixels + (row / 4 + blockRow) * destination.rowPitch, stripBlocks.pixels + blockRow * stripBlocks.rowPitch, copyBytes);
            }
        }
    }

    // 書きかけのファイルを読まないよう、別の名前で書いてから置き換える
    std::error_code errorCode;
    std::filesystem::create_directories(GetCacheDirectory(), errorCode);
    const std::filesystem::path path = GetCachePath(job.key);
    std::filesystem::path temporaryPath = path;
    temporaryPath += L".tmp";
    if (FAILED(DirectX::SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DirectX::DDS_FLAGS_NONE, temporaryPath.c_str())))
    {
        std::filesystem::remove(temporaryPath, errorCode);
        return;
    }
    std::filesystem::rename(temporaryPath, path, errorCode);
    if (errorCode)
    {
        std::filesystem::remove(temporaryPath, errorCode);
    }
}
