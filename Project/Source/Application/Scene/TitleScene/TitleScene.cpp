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
#include "../../UI/OptionMenu.h"
#include <imgui.h>

namespace
{
    // 画面右下の歯車(オプションボタン)。画像を用意せずImGuiの図形で描く
    // 戻り値: マウスが乗っているか
    bool DrawGearButton(float angle, bool selected)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float scale = vp->Size.y / 720.0f;
        const float outerR = 30.0f * scale;
        const ImVec2 center(vp->Pos.x + vp->Size.x - 64.0f * scale, vp->Pos.y + vp->Size.y - 64.0f * scale);

        const bool hovered = ImGui::IsMouseHoveringRect(ImVec2(center.x - outerR, center.y - outerR),
                                                        ImVec2(center.x + outerR, center.y + outerR), false);
        const ImU32 col = (selected || hovered) ? IM_COL32(255, 51, 51, 255) : IM_COL32(230, 230, 230, 220);

        ImDrawList* dl = ImGui::GetBackgroundDrawList(); // スプライトの上、エディタのウィンドウより下
        const float bodyR = outerR * 0.72f;
        dl->AddCircle(center, bodyR * 0.72f, col, 32, bodyR * 0.5f); // 中央に穴の開いたリング

        // 歯(台形)を8枚
        constexpr int kTeeth = 8;
        for (int i = 0; i < kTeeth; ++i) {
            const float a = angle + DirectX::XM_2PI * i / kTeeth;
            const float c = cosf(a), s = sinf(a);
            auto pt = [&](float along, float side) {
                return ImVec2(center.x + c * along - s * side, center.y + s * along + c * side);
            };
            const float baseW = outerR * 0.26f, tipW = outerR * 0.17f;
            dl->AddQuadFilled(pt(bodyR * 0.85f, -baseW), pt(outerR, -tipW), pt(outerR, tipW), pt(bodyR * 0.85f, baseW), col);
        }
        return hovered;
    }
}

void TitleScene::Init()
{
    m_time = 0.0f;
    m_cursorIndex = 0;
    m_gearAngle = 0.0f;

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

    // 歯車はホバー/選択中だけゆっくり回す
    const bool gearHovered = DrawGearButton(m_gearAngle, m_cursorIndex == 2 && !OptionMenu::IsOpen());

    if (OptionMenu::IsOpen())
    {
        // オプション画面を開いている間はタイトルの操作を受け付けない(ESCか「閉じる」で戻る)
        if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Escape)) OptionMenu::Close();
    }
    else
    {
        // キー入力（カーソル移動）: スタート → 終了 → オプション
        if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Down)) {
            m_cursorIndex = (m_cursorIndex + 1) % 3;
        }
        if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Up)) {
            m_cursorIndex = (m_cursorIndex + 2) % 3;
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
        if (gearHovered) {
            m_cursorIndex = 2;
            if (Input::Instance().IsMouseLeftTrigger()) decided = true;
        }

        if (decided)
        {
            if (m_cursorIndex == 0) {
                SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 1.0f);
            } else if (m_cursorIndex == 1) {
                PostQuitMessage(0); // アプリケーション終了
            } else {
                OptionMenu::Open();
            }
        }
        else if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Escape)) {
            OptionMenu::Open();
        }

        if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::C)) {
            SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(true, 245.4f, 1 /*Kageonna*/, 2), 1.0f);
        }
        if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::G)) {
            SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(false), 1.0f);
        }
    }

    OptionMenu::Draw();

    if (m_cursorIndex == 2 && !OptionMenu::IsOpen()) m_gearAngle += deltaTime * 1.2f;

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
    OptionMenu::Close();

    AudioManager::Instance().StopBGM();
}
