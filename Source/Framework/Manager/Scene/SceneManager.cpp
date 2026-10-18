#include "../../../Pch.h"
#include <SpriteBatch.h>
#include "SceneManager.h"
#include "../../../Application/Object/Script/System/GameSequence.h"
#include "../../ImGuiEditor/Editor/Editor.h"

void SceneManager::Init()
{
    m_fadeState = FadeState::None;
    m_fadeAlpha = 0.0f;

    // GDFのシステムが持っているテクスチャを取得して使用する
    m_pFadeTexture = GDF::Instance().GetBlackTex();
}

SceneManager& SceneManager::Instance()
{
    static SceneManager instance;
    return instance;
}

void SceneManager::Shutdown()
{
    m_currentScene.reset();
    m_nextScene.reset();
}

void SceneManager::Update()
{
    // Set for exactly the frame the scene swap below happens. That swap includes a mid-frame
    // GraphicsQueue::Flush() (to let the old scene's GPU resources retire before it's destroyed);
    // that flush submits/retires the graphics context GDF::BeginFrame() already opened for this
    // frame, and nothing re-acquires a fresh one afterward. Calling the freshly-swapped-in scene's
    // Update() (which renders) on that same frame then dereferences a context that's no longer
    // checked out -> crash inside GraphicsDevice::GetCmdList(). Skipping just this one frame's
    // render lets the next frame's normal GDF::BeginFrame() re-establish a valid context first.
    bool justSwappedThisFrame = false;

    if (m_fadeState == FadeState::FadeOut)
    {
        m_fadeAlpha += GameTimer::Instance().DeltaTime() / m_fadeDuration;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Fading Out... Alpha: %.4f (Delta: %.4f)", m_fadeAlpha, GameTimer::Instance().DeltaTime());
        if (m_fadeAlpha >= 1.0f)
        {
            m_fadeAlpha = 1.0f;

            // Paint the back buffer solid black *before* the mid-frame Flush() below invalidates
            // this frame's graphics context. Without this, the swap frame (see justSwappedThisFrame)
            // skips the new scene's render entirely, leaving the back buffer holding whatever was
            // there before - a one-frame flash of stale/garbage content - since DrawFade()'s overlay
            // has nothing underneath it to composite onto that frame.
            GraphicsDevice::Instance().SetBackBuffer();
            GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);

            // シーンを破棄する前に、GPU側の処理が完了するのを待っておく
            // (使用中のリソースが残ったまま解放すると問題になるため)
            GraphicsDevice::Instance().GetQueueManager()->GetGraphicsQueue()->Flush();

            // Scenes that create their own raw ECS entities directly (TitleScene, ResultScene -
            // anything not built on the JSON/GameObject-based Scene) override Unload() to destroy
            // them, but nothing was ever calling it: the entities just leaked into whichever scene
            // came next (e.g. Title's background/logo/button sprites still being iterated - and
            // drawn - by SpriteRenderSystem after switching to GameScene).
            if (m_currentScene)
            {
                m_currentScene->Unload();
            }

            CollisionManager::Instance().SetScene(nullptr);
            // 古いシーンが登録していた静的コライダー(オクトツリー内の生ポインタを含む)を
            // ここで確実にクリアしておく。しないと、次のシーンで同じEntity IDが
            // 再利用された時に「登録済み」と判定されて再登録されず、
            // 破棄済みのCollisionShapeを指すダングリングポインタが衝突判定で
            // 参照されてクラッシュする(シーン切り替え時に落ちていた不具合の原因)。
            CollisionManager::Instance().ResetForSceneChange();
            // GameSequence::s_instanceも同じ理由(Scene破棄時にOnDestroy()が呼ばれない)で
            // ダングリングポインタになり得るためここでクリアする。
            GameSequence::ResetInstance();
            // Editorの選択中オブジェクトも同じ理由でクリアする。
            // (shared_ptrで古いシーンのGameObjectが生き延び、Inspectorから
            //  存在しないEntityを参照してクラッシュする原因になっていた)
            Editor::ClearSelection();
            m_currentScene = nullptr;

            // GameObjectの破棄はECSコマンドバッファに積まれるだけで即座には実行されない。
            // ここでFlushCommands()せずに次のシーンをInit()すると、破棄待ちのEntity/
            // コンポーネント(特にNativeScriptDataの中のスクリプトインスタンス)が
            // ECS上に残ったまま新シーンの初期化処理が走ってしまい、新シーン側のコードが
            // それを拾って「既に解放されたGameObjectへの生ポインタ」を辿ってクラッシュする
            // (これが繰り返し発生していたクラッシュの根本原因だった)。
            // 次のシーンをセットする前に必ず破棄コマンドを反映させる。
            GameManager::Instance().GetECS().FlushCommands();

            // 新しいシーンをセットする
            m_currentScene = std::move(m_nextScene);

            if (m_currentScene)
            {
                m_currentScene->Init();
            }

            m_fadeState = FadeState::FadeIn;
            justSwappedThisFrame = true;
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

    if (m_currentScene && !justSwappedThisFrame)
    {
        m_currentScene->Update(GameTimer::Instance().DeltaTime());
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
    DirectX::XMFLOAT2 scale(1280.0f, 720.0f); // 1x1のテクスチャを全画面に引き伸ばす
    auto texSize = DirectX::XMUINT2(1, 1);

    pSpriteBatch->Draw(
        pGraphicsDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(m_pFadeTexture->GetSRVNumber()),
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

void SceneManager::ChangeScene(std::unique_ptr<SceneBase> nextScene, float fadeDuration)
{
    Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Called! fadeDuration=%.2f", fadeDuration);
    if (m_fadeState != FadeState::None) 
    {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Ignored (Already Fading)");
        return; 
    }
    m_nextScene = std::move(nextScene);
    m_fadeDuration = fadeDuration;
    m_fadeState = FadeState::FadeOut;
    Logger::Instance().AddLog(Logger::LogLevel::Info, "ChangeScene Accepted! FadeOut Started.");
}





void SceneManager::SetCurrentSceneWithoutFade(std::unique_ptr<SceneBase> scene)
{
    m_currentScene = std::move(scene);
}

