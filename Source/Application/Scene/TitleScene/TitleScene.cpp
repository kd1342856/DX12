#include "../../../Pch.h"
#include "TitleScene.h"
#include "../GameScene/GameScene.h"
#include "../ResultScene/ResultScene.h"
#include "../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/Components/Data/SpriteData.h"
#include "../../../Framework/ECS/Components/Data/TransformData.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Graphics/Renderer/Renderer.h"

void TitleScene::Init()
{
    m_time = 0.0f;
    m_cursorIndex = 0;

    auto& ecs = GameManager::Instance().GetECS();

    // 背景画像
    m_bgEntity = ecs.CreateEntity();
    TransformData bgTransform;
    bgTransform.m_position = { 640.0f, 360.0f, 0.0f }; // 画面中央
    SpriteData bgSprite;
    bgSprite.m_filePath = "Asset/Texture/UI/bg_title.png"; // 新しい背景
    bgSprite.m_size = { 1280.0f, 720.0f }; // 画面全体
    bgSprite.m_orderInLayer = 100; // 最背面 (1.0f depth)
    ecs.AddComponent(m_bgEntity, bgTransform);
    ecs.AddComponent(m_bgEntity, bgSprite);

    // タイトルロゴ
    m_titleEntity = ecs.CreateEntity();
    TransformData tTransform;
    tTransform.m_position = { 640.0f, 250.0f, 0.0f }; // 画面中央上部
    SpriteData tSprite;
    tSprite.m_filePath = "Asset/Texture/UI/Haihu.png"; // AI生成画像
    tSprite.m_size = { 600.0f, 400.0f };
    tSprite.m_orderInLayer = 50;
    ecs.AddComponent(m_titleEntity, tTransform);
    ecs.AddComponent(m_titleEntity, tSprite);

    // Startボタン
    m_startEntity = ecs.CreateEntity();
    TransformData sTransform;
    sTransform.m_position = { 640.0f, 500.0f, 0.0f };
    SpriteData sSprite;
    sSprite.m_filePath = "Asset/Texture/UI/Start.png"; // AI生成画像
    sSprite.m_size = { 300.0f, 200.0f }; // 600x400だと大きすぎるので半分にスケール
    sSprite.m_orderInLayer = 50;
    ecs.AddComponent(m_startEntity, sTransform);
    ecs.AddComponent(m_startEntity, sSprite);

    // Exitボタン
    m_exitEntity = ecs.CreateEntity();
    TransformData eTransform;
    eTransform.m_position = { 640.0f, 620.0f, 0.0f };
    SpriteData eSprite;
    eSprite.m_filePath = "Asset/Texture/UI/Exit.png"; // AI生成画像
    eSprite.m_size = { 300.0f, 200.0f }; // 半分にスケール
    eSprite.m_orderInLayer = 50;
    ecs.AddComponent(m_exitEntity, eTransform);
    ecs.AddComponent(m_exitEntity, eSprite);
}

void TitleScene::Update(float deltaTime)
{
    m_time += deltaTime;

    auto& ecs = GameManager::Instance().GetECS();

    // キー入力（カーソル移動）
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Up) || 
        Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Down)) {
        m_cursorIndex = (m_cursorIndex == 0) ? 1 : 0;
    }

    // 決定キー
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Enter) || 
        Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Space))
    {
        if (m_cursorIndex == 0) {
            SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 1.0f);
        } else {
            PostQuitMessage(0); // アプリケーション終了
        }
    }

    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::C)) {
        SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(true, 125.4f, "Onryo", 5000), 1.0f);
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::G)) {
        SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(false), 1.0f);
    }

    // スプライトのアニメーションと選択状態の反映
    auto& tTransform = ecs.GetComponent<TransformData>(m_titleEntity);
    auto& tSprite    = ecs.GetComponent<SpriteData>(m_titleEntity);
    tTransform.m_position.y = 200.0f + sinf(m_time * 2.0f) * 10.0f; // フワフワ揺れる
    tSprite.m_color = { 1.0f, 1.0f, 1.0f, 0.7f + 0.3f * sinf(m_time * 3.0f) }; // 明滅

    auto& sSprite = ecs.GetComponent<SpriteData>(m_startEntity);
    sSprite.m_color = (m_cursorIndex == 0) ? Math::Vector4{1.0f, 0.2f, 0.2f, 1.0f} : Math::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
    sSprite.m_size  = (m_cursorIndex == 0) ? Math::Vector2{220.0f, 66.0f} : Math::Vector2{200.0f, 60.0f};

    auto& eSprite = ecs.GetComponent<SpriteData>(m_exitEntity);
    eSprite.m_color = (m_cursorIndex == 1) ? Math::Vector4{1.0f, 0.2f, 0.2f, 1.0f} : Math::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
    eSprite.m_size  = (m_cursorIndex == 1) ? Math::Vector2{220.0f, 66.0f} : Math::Vector2{200.0f, 60.0f};

    // 背景色（不気味な脈打ち）
    float red = 0.1f + 0.05f * sinf(m_time * 2.0f);
    float blue = 0.1f + 0.05f * cosf(m_time * 1.5f);

    Renderer::BeginFrame();
    GraphicsDevice::Instance().SetBackBuffer();
    GraphicsDevice::Instance().ClearBackBuffer(red, 0.0f, blue, 1.0f);

    auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
    if (spriteRenderSystem) {
        spriteRenderSystem->Render();
    }

    Editor::Draw();

    Renderer::EndFrame();
}

void TitleScene::Unload()
{
    auto& ecs = GameManager::Instance().GetECS();
    ecs.DestroyEntity(m_bgEntity);
    ecs.DestroyEntity(m_titleEntity);
    ecs.DestroyEntity(m_startEntity);
    ecs.DestroyEntity(m_exitEntity);
}

