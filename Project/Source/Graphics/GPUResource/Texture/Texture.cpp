#include "../../../Pch.h"
#include "Texture.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Device/ResourceUploader.h"
#include "../../GPUUploadQueue/GPUUploadQueue.h"
#include "../../../Framework/DirectX/Utility/Thread.h"

namespace
{
    // ファイルに付いているsRGBの情報(PNGのsRGBチャンクなど)は付いていたりいなかったりするので見ず、
    // 使い道で指定された色空間に揃える
    DirectX::WIC_FLAGS ToWicFlags(TextureColorSpace colorSpace)
    {
        return colorSpace == TextureColorSpace::Srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB;
    }

    // WICで読み込んだ画像を、色空間とミップの指定どおりに整えてアップロード用のデータにする
    bool BuildUploadData(DirectX::ScratchImage& scratchImage, TextureColorSpace colorSpace, bool isMipmapped, TextureUploadData& outData)
    {
        DirectX::ScratchImage converted;
        DirectX::ScratchImage mipChain;
        DirectX::ScratchImage* pSource = &scratchImage;

        // _SRGBの版が無いフォーマット(グレースケールや16bitなど)は、色のスロットではRGBA8の_SRGBに変換する。
        // 値はもともとsRGBでエンコードされているので、SRGB_INを付けて値を変えずにフォーマットだけ入れ替える
        const DXGI_FORMAT sourceFormat = pSource->GetMetadata().format;
        if (colorSpace == TextureColorSpace::Srgb && !DirectX::IsSRGB(sourceFormat))
        {
            DirectX::TEX_FILTER_FLAGS filter = DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_SRGB_IN;
            if (sourceFormat == DXGI_FORMAT_R8_UNORM || sourceFormat == DXGI_FORMAT_R16_UNORM)
            {
                // グレースケールは赤だけにならないよう、GとBにも同じ値を入れる
                filter |= DirectX::TEX_FILTER_RGB_COPY_RED;
            }
            if (FAILED(DirectX::Convert(*pSource->GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, filter, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
            {
                return false;
            }
            pSource = &converted;
        }

        if (isMipmapped)
        {
            const DirectX::TexMetadata& sourceMetadata = pSource->GetMetadata();
            if (sourceMetadata.width > 1 || sourceMetadata.height > 1)
            {
                // WICの縮小はsRGBを考慮しないため、DirectXTex自前のフィルタを使う(_SRGBはリニアに戻してから平均される)
                HRESULT hr = DirectX::GenerateMipMaps(pSource->GetImages(), pSource->GetImageCount(), sourceMetadata,
                    DirectX::TEX_FILTER_LINEAR | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, mipChain);
                if (SUCCEEDED(hr))
                {
                    pSource = &mipChain;
                }
                else
                {
                    // ミップが作れなくても描画はできるので、0段目だけで続ける
                    OutputDebugStringA("Texture: GenerateMipMapsに失敗したため、ミップなしで読み込みます\n");
                }
            }
        }

        const DirectX::TexMetadata& metadata = pSource->GetMetadata();
        size_t totalSize = 0;
        for (size_t mipIndex = 0; mipIndex < metadata.mipLevels; ++mipIndex)
        {
            totalSize += pSource->GetImage(mipIndex, 0, 0)->slicePitch;
        }

        outData.pixels.resize(totalSize);
        outData.mipLevels.clear();
        size_t offset = 0;
        for (size_t mipIndex = 0; mipIndex < metadata.mipLevels; ++mipIndex)
        {
            const DirectX::Image* pImage = pSource->GetImage(mipIndex, 0, 0);
            TextureUploadData::MipLevel level;
            level.offset     = offset;
            level.width      = static_cast<UINT>(pImage->width);
            level.height     = static_cast<UINT>(pImage->height);
            level.rowPitch   = static_cast<UINT>(pImage->rowPitch);
            level.slicePitch = static_cast<UINT>(pImage->slicePitch);
            memcpy(outData.pixels.data() + offset, pImage->pixels, pImage->slicePitch);
            offset += pImage->slicePitch;
            outData.mipLevels.push_back(level);
        }

        outData.width    = outData.mipLevels[0].width;
        outData.height   = outData.mipLevels[0].height;
        outData.rowPitch = outData.mipLevels[0].rowPitch;
        outData.format   = metadata.format;
        return true;
    }
}

bool Texture::LoadCPU(const std::string& filePath, TextureUploadData& outData, TextureColorSpace colorSpace, bool isMipmapped)
{
    wchar_t wFilePath[128];
    MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePath, _countof(wFilePath));

    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImage = {};

    HRESULT hr = DirectX::LoadFromWICFile(wFilePath, ToWicFlags(colorSpace), &metadata, scratchImage);
    if (FAILED(hr) || !BuildUploadData(scratchImage, colorSpace, isMipmapped, outData))
    {
        OutputDebugStringA(("Texture::LoadCPU failed: " + filePath + "\n").c_str());
        m_state = AssetState::Failed;
        return false;
    }

    m_state = AssetState::WaitingGPU;
    return true;
}

bool Texture::LoadCPUFromMemory(const void* pData, size_t size, TextureUploadData& outData, TextureColorSpace colorSpace, bool isMipmapped)
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImage = {};

    HRESULT hr = DirectX::LoadFromWICMemory((const uint8_t*)pData, size, ToWicFlags(colorSpace), &metadata, scratchImage);
    if (FAILED(hr) || !BuildUploadData(scratchImage, colorSpace, isMipmapped, outData))
    {
        OutputDebugStringA("Texture::LoadCPUFromMemory failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    m_state = AssetState::WaitingGPU;
    return true;
}

bool Texture::CreateGPU(GraphicsDevice* pDevice, const TextureUploadData& data)
{
    assert(Thread::IsMainThread() && "Texture::CreateGPU must be called on the main thread!");

    m_device = pDevice;

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC recDesc = {};
    recDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    recDesc.Format           = data.format;
    recDesc.Width            = static_cast<UINT64>(data.width);
    recDesc.Height           = data.height;
    recDesc.DepthOrArraySize = 1;
    recDesc.MipLevels        = static_cast<UINT16>((std::max)(data.mipLevels.size(), static_cast<size_t>(1)));
    recDesc.SampleDesc.Count = 1;

    // CopyキューはCOMMON状態のリソースを自動的にCOPY_DESTへプロモートするためCOMMONで作成する
    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
    {
        OutputDebugStringA("Texture::CreateGPU: CreateCommittedResource failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    m_device->GetResourceUploader()->UploadTexture(this, data);

    m_srvNumber = m_device->CreateSRV(m_resource.Get());

    return true;
}

bool Texture::CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format)
{
    assert(Thread::IsMainThread() && "Texture::CreateFromMemory must be called on the main thread!");

    m_device = &GraphicsDevice::Instance();

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC recDesc = {};
    recDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    recDesc.Format           = format;
    recDesc.Width            = static_cast<UINT64>(width);
    recDesc.Height           = static_cast<UINT>(height);
    recDesc.DepthOrArraySize = 1;
    recDesc.MipLevels        = 1;
    recDesc.SampleDesc.Count = 1;

    // CopyキューはCOMMON状態のリソースを自動的にCOPY_DESTへプロモートするためCOMMONで作成する
    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
    {
        OutputDebugStringA("Texture::CreateFromMemory: CreateCommittedResource failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    UINT rowPitch   = width * 4;
    UINT slicePitch = rowPitch * height;

    m_device->GetResourceUploader()->Begin();
    m_device->GetResourceUploader()->UploadTexture(
        this, data, width, height, rowPitch, slicePitch, format);
    m_device->GetResourceUploader()->End();

    m_srvNumber = m_device->CreateSRV(m_resource.Get());
    m_state     = AssetState::Ready;
    return true;
}

void Texture::Set(int index)
{
    m_device->GetCmdList()->SetGraphicsRootDescriptorTable(
        index,
        m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(m_srvNumber));
}
