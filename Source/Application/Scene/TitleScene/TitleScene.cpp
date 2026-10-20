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
#include "../../../Framework/Manager/Scene/Scene.h"
#include "../../../Framework/Object/GameObject.h"
#include "../../../Framework/Manager/Audio/AudioManager.h"

void TitleScene::Init()
{
    m_time = 0.0f;
    m_cursorIndex = 0;

    // GameSceneのFPS視点(Relative)モードのままだとカーソルが毎フレーム中央に戻され、ボタンをクリックできない
    Input::Instance().SetMouseModeAbsolute();

    auto& ecs = GameManager::Instance().GetECS();

    // Editor::s_sceneはシーン遷移で自動的には破棄されないため、GameSceneのGameObjectがヒエラルキーに
    // 残ってしまう。GameScene::Init()と同様にEditor::Init()で空のSceneに差し替え、破棄コマンドを反映する。
    Editor::Init();
    ecs.FlushCommands();

    Editor::SetScenePath("Asset/Data/Scene/TitleScene.json");

    auto editorScene = Editor::GetScene();
    std::ifstream in(Editor::GetScenePath());
    if (in.is_open())
    {
        nlohmann::json j;
        in >> j;
        editorScene->Deserialize(j);
    }
    else
    {
        // 初回(JSON未作成)は既定のオブジェクトを作る。以降はヒエラルキーで編集してSave Sceneで保存する。
        auto create = [&](const char* name, Math::Vector3 pos, const char* tex, Math::Vector2 size, int order)
        {
            auto obj = editorScene->CreateGameObject(name);
            obj->GetComponent<TransformData>().m_position = pos;
            SpriteData sprite;
            sprite.m_filePath = tex;
            sprite.m_size = size;
            sprite.m_orderInLayer = order;
            obj->AddComponent(sprite);
        };
        create("TitleBackground", { 640.0f, 360.0f, 0.0f }, "Asset/Texture/UI/bg_title.png", { 1280.0f, 720.0f }, 100);
        create("TitleLogo",       { 640.0f, 250.0f, 0.0f }, "Asset/Texture/UI/Haihu.png",    { 600.0f, 400.0f }, 50);
        create("StartButton",     { 640.0f, 500.0f, 0.0f }, "Asset/Texture/UI/Start.png",    { 300.0f, 200.0f }, 50);
        create("ExitButton",      { 640.0f, 620.0f, 0.0f }, "Asset/Texture/UI/Exit.png",     { 300.0f, 200.0f }, 50);
    }

    // 名前で演出対象のオブジェクトを探す(見つからなければINVALID_ENTITYのまま演出をスキップ)
    auto find = [&](const char* name) -> Entity
    {
        for (auto& obj : editorScene->GetGameObjects())
            if (obj->GetName() == name) return obj->GetEntityID();
        return INVALID_ENTITY;
    };
    m_bgEntity    = find("TitleBackground");
    m_titleEntity = find("TitleLogo");
    m_startEntity = find("StartButton");
    m_exitEntity  = find("ExitButton");

    editorScene->Init();

    // 音量はSound Managerの「曲ごとの音量」で調整する
    AudioManager::Instance().PlayBGM(kBGMPath);
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
    bool decided = Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Enter) ||
                   Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Space);

    // マウス: スプライトにホバーで選択、クリックで決定
    if (auto spriteSys = GameManager::Instance().GetSpriteRenderSystem()) {
        const Entity buttons[2] = { m_startEntity, m_exitEntity };
        for (int i = 0; i < 2; ++i) {
            if (spriteSys->IsMouseOver(buttons[i])) {
                m_cursorIndex = i;
                if (Input::Instance().IsMouseLeftTrigger()) decided = true;
            }
        }
    }

    if (decided)
    {
        if (m_cursorIndex == 0) {
            SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 1.0f);
        } else {
            PostQuitMessage(0); // アプリケーション終了
        }
    }

    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::C)) {
        SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(true, 245.4f, 1 /*Kageonna*/, 2), 1.0f);
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::G)) {
        SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(false), 1.0f);
    }

    // スプライトのアニメーションと選択状態の反映(SpriteData/Transformが無いオブジェクトはスキップ)
    if (auto* t = ecs.TryGetComponent<TransformData>(m_titleEntity))
        t->m_position.y = 200.0f + sinf(m_time * 2.0f) * 10.0f; // フワフワ揺れる
    if (auto* s = ecs.TryGetComponent<SpriteData>(m_titleEntity))
        s->m_color = { 1.0f, 1.0f, 1.0f, 0.7f + 0.3f * sinf(m_time * 3.0f) }; // 明滅

    const Entity buttons[2] = { m_startEntity, m_exitEntity };
    for (int i = 0; i < 2; ++i) {
        if (auto* s = ecs.TryGetComponent<SpriteData>(buttons[i])) {
            s->m_color = (m_cursorIndex == i) ? Math::Vector4{1.0f, 0.2f, 0.2f, 1.0f} : Math::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
            s->m_size  = (m_cursorIndex == i) ? Math::Vector2{220.0f, 66.0f} : Math::Vector2{200.0f, 60.0f};
        }
    }

    // 背景色（不気味な脈打ち）
    float red = 0.1f + 0.05f * sinf(m_time * 2.0f);
    float blue = 0.1f + 0.05f * cosf(m_time * 1.5f);

    Renderer::BeginFrame();
    GraphicsDevice::Instance().SetBackBuffer();
    Renderer::BindDefaultViewport(); // 未設定だとシザーが空でスプライトが全てクリップされる
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
    // オブジェクトはEditor::s_scene(ヒエラルキー)が所有する。次のシーンのInit()でEditor::Init()
    // により空のSceneへ差し替わる時に一緒に破棄されるので、ここでEntityを直接消さない。
    Editor::ClearSelection();

    AudioManager::Instance().StopBGM();
}
