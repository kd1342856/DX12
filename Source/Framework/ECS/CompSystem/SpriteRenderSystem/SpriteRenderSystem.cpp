#include <SpriteBatch.h>
#include "SpriteRenderSystem.h"
#include <SpriteBatch.h>

void SpriteRenderSystem::Update() {
}

void SpriteRenderSystem::Render() {
    if (!m_pCoordinator) return;

    auto pGraphicsDevice = &GraphicsDevice::Instance();
    auto pSpriteBatch = pGraphicsDevice->GetSpriteBatch();
    if (!pSpriteBatch) return;

    D3D12_VIEWPORT viewport = {};
    viewport.Width = 1280.0f;
    viewport.Height = 720.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    pSpriteBatch->SetViewport(viewport);

    // Begin SpriteBatch
    pSpriteBatch->Begin(pGraphicsDevice->GetCmdList(), DirectX::SpriteSortMode_Deferred);

    for (auto const& entity : m_entities)
    {
        auto& cSprite = m_pCoordinator->GetComponent<SpriteData>(entity);
        auto& cTransform = m_pCoordinator->GetComponent<TransformData>(entity);

        // Request texture load if not loaded
        if (!cSprite.m_spTexture && !cSprite.m_filePath.empty()) {
            cSprite.m_spTexture = ResourceManager::Instance().LoadTextureAsync(cSprite.m_filePath);
        }

        // Skip if texture is not ready
        if (!cSprite.m_spTexture || !cSprite.m_spTexture->m_pBuffer) {
            continue;
        }

        // Get 2D position and scale from TransformData
        DirectX::SimpleMath::Vector3 pos = cTransform.m_position;
        DirectX::SimpleMath::Vector3 scale = cTransform.m_scale;
        
        float rotZ = 0.0f; 

        auto desc = cSprite.m_spTexture->m_pBuffer->GetDesc();
        DirectX::XMUINT2 texSize((uint32_t)desc.Width, (uint32_t)desc.Height);

        // Calculate pivot (origin)
        DirectX::XMFLOAT2 origin(
            (float)desc.Width * cSprite.m_pivot.x,
            (float)desc.Height * cSprite.m_pivot.y
        );

        // Calculate draw scale
        DirectX::XMFLOAT2 drawScale(
            (cSprite.m_size.x / (float)desc.Width) * scale.x,
            (cSprite.m_size.y / (float)desc.Height) * scale.y
        );

        // Screen position
        DirectX::XMFLOAT2 screenPos(pos.x, pos.y);

        pSpriteBatch->Draw(
            pGraphicsDevice->GetCBVSRVUAVHeap()->GetGPUHandle(cSprite.m_spTexture->m_srvNumber), // Texture2D
            texSize, // Get size
            screenPos, // Position
            nullptr, // Source Rect (null = full image)
            cSprite.m_color, // Color
            rotZ, // Rotation
            origin, // Origin (Pivot)
            drawScale, // Scale
            DirectX::SpriteEffects_None,
            (float)cSprite.m_orderInLayer / 100.0f // Layer Depth
        );
    }

    pSpriteBatch->End();
}



