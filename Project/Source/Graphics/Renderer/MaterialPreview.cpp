#include "../../Pch.h"
#include "MaterialPreview.h"
#include "../../Framework/Manager/Asset/TextureManager.h"
#include "../../Framework/DirectX/Utility/Profiler.h"
#include <cmath>

namespace
{
    // MaterialPreview_CS.hlslのg_constants(b0)と同じ並び
    struct PreviewConstants
    {
        uint32_t destinationIndex;
        uint32_t size;
        uint32_t baseColorIndex;
        uint32_t normalIndex;
        uint32_t metallicRoughnessIndex;
        uint32_t occlusionIndex;
        uint32_t emissiveIndex;
        uint32_t alphaMode;
        float textureLod;
    };

    // MaterialPreview_CS.hlslのcbMaterial(b1)と同じ並び
    struct PreviewMaterial
    {
        Math::Vector4 baseColorFactor;
        Math::Vector3 emissiveFactor;
        float emissiveStrength;
        float metallicFactor;
        float roughnessFactor;
        float normalScale;
        float occlusionStrength;
        float emissiveFrontScale;
        Math::Vector3 padding;
    };

    constexpr UINT kThreadGroupSize = 8;

    // テクスチャがあり、GPUに送り終えていればその番号、無ければ既定のテクスチャの番号
    uint32_t ResolveTexture(const MaterialTexture& materialTexture, const Texture* pFallback, float* pOutLod)
    {
        if (materialTexture.valid)
        {
            const Texture* pTexture = TextureManager::Instance().Get(materialTexture.handle);
            if (pTexture && pTexture->IsReady() && pTexture->GetSRVNumber() >= 0)
            {
                if (pOutLod && pTexture->GetResource())
                {
                    // 球の見えている半分にテクスチャが1回り入るので、幅の比からミップを選ぶ
                    const float width = static_cast<float>(pTexture->GetResource()->GetDesc().Width);
                    *pOutLod = (std::max)(0.0f, std::log2(width / static_cast<float>(MaterialPreview::kSize)));
                }
                return static_cast<uint32_t>(pTexture->GetSRVNumber());
            }
        }
        return static_cast<uint32_t>(pFallback->GetSRVNumber());
    }
}

bool MaterialPreview::EnsureResources()
{
    if (m_isCreated)
    {
        return true;
    }
    if (m_hasFailed || !ComputeShader::IsSupported())
    {
        return false;
    }
    // 失敗しても、シェーダーを直せばホットリロードで使えるようになるので、作り直しはしない
    m_shader.Create(L"Asset/Shader/Debug/MaterialPreview_CS.hlsl");

    GPUTextureDesc desc;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.allowUAV = true;
    desc.width = kSize;
    desc.height = kSize;
    desc.pName = L"MaterialPreview";
    auto& device = GraphicsDevice::Instance();
    for (Slot& slot : m_slots)
    {
        if (!slot.texture.Create(desc))
        {
            m_hasFailed = true;
            return false;
        }
        // ImGuiのヒープの番号は返せないので、作るのは一度だけにする
        slot.imGuiSrvIndex = device.AllocateImGuiSRV(slot.texture.GetResource());
    }
    m_isCreated = true;
    return true;
}

int MaterialPreview::Request(const std::string& key, const Material& material)
{
    if (!EnsureResources())
    {
        return -1;
    }

    // 同じマテリアルの枠、無ければ最も長く頼まれていない枠(このフレームで頼まれた枠は使わない)
    Slot* pSlot = nullptr;
    for (Slot& slot : m_slots)
    {
        if (slot.key == key)
        {
            pSlot = &slot;
            break;
        }
    }
    if (!pSlot)
    {
        for (Slot& slot : m_slots)
        {
            if (!slot.isRequested && (!pSlot || slot.lastRequestedFrame < pSlot->lastRequestedFrame))
            {
                pSlot = &slot;
            }
        }
        if (!pSlot)
        {
            return -1;
        }
        pSlot->key = key;
        pSlot->hasContent = false;
    }

    auto& device = GraphicsDevice::Instance();
    pSlot->isRequested = true;
    pSlot->lastRequestedFrame = m_frame;
    pSlot->constants = material.Constants;
    pSlot->textureLod = 0.0f;
    pSlot->textureIndices[0] = ResolveTexture(material.BaseColor, device.GetWhiteTex(), &pSlot->textureLod);
    pSlot->textureIndices[1] = ResolveTexture(material.Normal, device.GetNormalTex(), nullptr);
    pSlot->textureIndices[2] = ResolveTexture(material.MetallicRoughness, device.GetWhiteTex(), nullptr);
    pSlot->textureIndices[3] = ResolveTexture(material.Occlusion, device.GetWhiteTex(), nullptr);
    pSlot->textureIndices[4] = ResolveTexture(material.Emissive, device.GetWhiteTex(), nullptr);
    return pSlot->hasContent ? pSlot->imGuiSrvIndex : -1;
}

void MaterialPreview::Dispatch()
{
    ++m_frame;
    if (!m_isCreated || !m_shader.IsValid())
    {
        return;
    }
    bool isAnyRequested = false;
    for (const Slot& slot : m_slots)
    {
        isAnyRequested |= slot.isRequested;
    }
    if (!isAnyRequested)
    {
        return;
    }

    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(device.GetCmdList(), "MaterialPreview");
    for (Slot& slot : m_slots)
    {
        if (slot.isRequested)
        {
            pContext->TransitionResource(slot.texture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }
    pContext->FlushResourceBarriers();

    m_shader.Begin();
    for (Slot& slot : m_slots)
    {
        if (!slot.isRequested)
        {
            continue;
        }
        const MaterialConstants& source = slot.constants;
        PreviewMaterial material = {};
        material.baseColorFactor = source.baseColorFactor;
        material.emissiveFactor = { source.emissiveFactorX, source.emissiveFactorY, source.emissiveFactorZ };
        material.emissiveStrength = source.emissiveStrength;
        material.metallicFactor = source.metallicFactor;
        material.roughnessFactor = source.roughnessFactor;
        material.normalScale = source.normalScale;
        material.occlusionStrength = source.occlusionStrength;
        material.emissiveFrontScale = source.emissiveFrontScale;

        PreviewConstants constants = {};
        constants.destinationIndex = static_cast<uint32_t>(slot.texture.GetUAVIndex());
        constants.size = kSize;
        constants.baseColorIndex = slot.textureIndices[0];
        constants.normalIndex = slot.textureIndices[1];
        constants.metallicRoughnessIndex = slot.textureIndices[2];
        constants.occlusionIndex = slot.textureIndices[3];
        constants.emissiveIndex = slot.textureIndices[4];
        constants.alphaMode = static_cast<uint32_t>(source.alphaMode);
        constants.textureLod = slot.textureLod;

        m_shader.SetConstants(constants);
        m_shader.SetConstantBuffer(material);
        m_shader.Dispatch(ComputeShader::DivideRoundUp(kSize, kThreadGroupSize), ComputeShader::DivideRoundUp(kSize, kThreadGroupSize));
    }

    // ImGuiが読めるように戻す
    for (Slot& slot : m_slots)
    {
        if (slot.isRequested)
        {
            pContext->TransitionResource(slot.texture.GetResource(), GPUTexture::kInitialState);
            slot.hasContent = true;
            slot.isRequested = false;
        }
    }
    pContext->FlushResourceBarriers();
}
