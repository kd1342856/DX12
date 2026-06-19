#include <SpriteBatch.h>
#include "Pch.h"
#include "SceneManager.h"
#include "../DirectX/Utility/Time.h"
#include "../../Graphics/Device/GraphicsDevice.h"
#include "../../Graphics/Descriptors/Heap/CBVSRVUAVHeap/CBVSRVUAVHeap.h"
#include "../DirectX/Utility/Logger.h"

void SceneManager::Init()
{
    m_fadeState = FadeState::None;
    m_fadeAlpha = 0.0f;

    // GDF縺ｮ繧ｷ繧ｹ繝・Β縺梧戟縺､鮟偵ユ繧ｯ繧ｹ繝√Ε繧貞叙蠕励＠縺ｦ菴ｿ逕ｨ縺吶ｋ
    m_pFadeTexture = GDF::Instance().GetBlackTex();
}

SceneManager& SceneManager::Instance()
{
    static SceneManager instance;
    return instance;
}

void SceneManager::Update()
{
    if (m_fadeState == FadeState::FadeOut)
    {
        m_fadeAlpha += GameTimer::Instance().DeltaTime() / m_fadeDuration;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Fading Out... Alpha: %.4f (Delta: %.4f)", m_fadeAlpha, GameTimer::Instance().DeltaTime());
        if (m_fadeAlpha >= 1.0f)
        {
            m_fadeAlpha = 1.0f;
            // シーンの切り替え実行
            m_spCurrentScene = m_spNextScene;
            m_spNextScene = nullptr;

            if (m_spCurrentScene)
            {
                m_spCurrentScene->Init();
            }

            m_fadeState = FadeState::FadeIn;
        }
    }
    else if (m_fadeState == FadeState::FadeIn)
    {
        m_fadeAlpha -= GameTimer::Instance().DeltaTime() / m_fadeDuration;
        if (m_fadeAlpha <= 0.0f)
        {
            m_fadeAlpha = 0.0f;
            m_fadeState = FadeState::None;
        }
    }

    if (m_spCurrentScene)
    {
        m_spCurrentScene->Update();
    }
}

void SceneManager::DrawFade()
{
    if (m_fadeState == FadeState::None || m_fadeAlpha <= 0.0f || !m_pFadeTexture)
        return;

    auto pGraphicsDevice = &GraphicsDevice::Instance();
    auto pSpriteBatch = pGraphicsDevice->GetSpriteBatch();
    if (!pSpriteBatch) return;

    D3D12_VIEWPORT viewport = {};
    viewport.Width = 1280.0f;
    viewport.Height = 720.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    pSpriteBatch->SetViewport(viewport);

    pSpriteBatch->Begin(pGraphicsDevice->GetCmdList(), DirectX::SpriteSortMode_Deferred);

    DirectX::XMVECTOR color = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, m_fadeAlpha);
    DirectX::XMFLOAT2 pos(0.0f, 0.0f);
    DirectX::XMFLOAT2 scale(1280.0f, 720.0f); // 1x1縺ｮ繝・け繧ｹ繝√Ε繧貞・逕ｻ髱｢縺ｫ蠑輔″蟒ｶ縺ｰ縺・
    auto texSize = DirectX::XMUINT2(1, 1);

    pSpriteBatch->Draw(
        pGraphicsDevice->GetCBVSRVUAVHeap()->GetGPUHandle(m_pFadeTexture->GetSRVNumber()),
        texSize,
        pos,
        nullptr,
        color,
        0.0f,
        DirectX::XMFLOAT2(0, 0),
        scale,
        DirectX::SpriteEffects_None,
        0.0f
    );

    pSpriteBatch->End();
}

void SceneManager::ChangeScene(std::shared_ptr<SceneBase> nextScene, float fadeDuration)
{
    Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Called! fadeDuration=%.2f", fadeDuration);
    if (m_fadeState != FadeState::None) 
    {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Ignored (Already Fading)");
        return; 
    }
    m_spNextScene = nextScene;
    m_fadeDuration = fadeDuration;
    m_fadeState = FadeState::FadeOut;
    Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Accepted! FadeOut Started.");
}



