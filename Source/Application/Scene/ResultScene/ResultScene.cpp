#include "../../../Pch.h"
#include "ResultScene.h"
#include "../TitleScene/TitleScene.h"
#include "../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../Graphics/Shader/FogShader/FogShader.h"
#include "../../../Graphics/Shader/MistShader/MistShader.h"
#include "../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/Components/Data/SpriteData.h"
#include "../../../Framework/ECS/Components/Data/TransformData.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../Object/Script/Ghost/GhostAI.h"
#include <imgui.h>
#include <algorithm>

ResultScene::ResultScene(bool isClear, float timeTaken, int ghostType, int huntsSurvived)
    : m_isClear(isClear), m_timeTaken(timeTaken), m_ghostType(ghostType), m_huntsSurvived(huntsSurvived)
{
}

void ResultScene::Init()
{
    m_time = 0.0f;

    // GameScene's fullscreen play mode leaves the mouse in relative/hidden mode (for FPS-style
    // camera look), and that mode is a global Input singleton state that survives the scene
    // change - without releasing it here, the cursor stays locked/invisible on the result screen,
    // making its UI unclickable.
    Input::Instance().SetMouseModeAbsolute();

    // Editor::s_showEditor (what F1 toggles) is likewise a global that survives the scene change -
    // force it off on entry so the debug windows don't clutter the result screen just because they
    // happened to be left open during gameplay. F1 can still bring them back for debugging.
    Editor::SetShowEditor(false);

    auto& ecs = GameManager::Instance().GetECS();

    // Hierarchy is per-scene: drop the previous scene's GameObjects. ResultScene builds raw ECS
    // entities, so its hierarchy stays empty and has no save path (Save Scene is a no-op here).
    Editor::Init();
    ecs.FlushCommands();

    // クリア画面の背景はMistShader(青白い霧)で描くので、背景スプライトは作らない
    if (!m_isClear)
    {
        m_deathEntity = ecs.CreateEntity();
        TransformData dTransform;
        dTransform.m_position = {640.0f, 360.0f, 0.0f};
        SpriteData dSprite;
        dSprite.m_filePath = "Asset/Texture/UI/Death.png";
        dSprite.m_size = {600.0f, 400.0f};
        dSprite.m_color = {1.0f, 1.0f, 1.0f, 0.0f}; // Start invisible
        dSprite.m_orderInLayer = 50;
        ecs.AddComponent(m_deathEntity, dTransform);
        ecs.AddComponent(m_deathEntity, dSprite);
    }

    m_buttonEntity = ecs.CreateEntity();
    TransformData bTransform;
    bTransform.m_position = {640.0f, 600.0f, 0.0f};
    SpriteData bSprite;
    bSprite.m_filePath = "Asset/Texture/UI/BackToTitle.png";
    bSprite.m_size = {300.0f, 200.0f};
    bSprite.m_color = {1.0f, 1.0f, 1.0f, 0.0f}; // Start invisible
    bSprite.m_orderInLayer = 10;
    ecs.AddComponent(m_buttonEntity, bTransform);
    ecs.AddComponent(m_buttonEntity, bSprite);
}

void ResultScene::Update(float deltaTime)
{
    m_time += deltaTime;

    auto& ecs = GameManager::Instance().GetECS();

    // Animate UI visibility
    float uiAlpha = std::clamp(m_time - 3.0f, 0.0f, 1.0f);
    if (m_buttonEntity != 0)
    {
        auto& btnSprite = ecs.GetComponent<SpriteData>(m_buttonEntity);
        btnSprite.m_color.w = uiAlpha; // Fade in after 3 seconds
    }

    if (!m_isClear && m_deathEntity != 0)
    {
        float deathAlpha = std::clamp(m_time / 3.0f, 0.0f, 1.0f);
        auto& deathSprite = ecs.GetComponent<SpriteData>(m_deathEntity);
        deathSprite.m_color.w = deathAlpha;

        // Add vertical jitter
        auto& deathTransform = ecs.GetComponent<TransformData>(m_deathEntity);
        float offsetY = sinf(m_time * 2.0f) * 5.0f;
        deathTransform.m_position.y = 360.0f + offsetY;
    }

    Renderer::BeginFrame();
    GraphicsDevice::Instance().SetBackBuffer();

    // 背景: ゲームオーバーは赤い霧(FogShader)、クリアは青白い霧(MistShader)
    auto* pCmdList = GraphicsDevice::Instance().GetCmdList();

    D3D12_VIEWPORT viewport = {};
    viewport.Width = 1280.0f;
    viewport.Height = 720.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = 1280;
    scissorRect.bottom = 720;

    pCmdList->RSSetViewports(1, &viewport);
    pCmdList->RSSetScissorRects(1, &scissorRect);

    GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
    if (m_isClear)
    {
        ShaderLibrary::Instance().Get<MistShader>().Draw(m_time);
    }
    else
    {
        ShaderLibrary::Instance().Get<FogShader>().Draw(m_time);
    }

    auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
    if (spriteRenderSystem)
    {
        spriteRenderSystem->Render();
    }

    if (m_isClear)
    {
        DrawClearReport();
    }

    // Click directly on the BackToTitle sprite (hit-tested in the sprite's own 1280x720 space).
    // The old ImGui InvisibleButton used window-local SetCursorPos with screen-center coordinates
    // inside an auto-resizing window, so it never actually sat over the sprite.
    if (uiAlpha > 0.5f)
    {
        auto spriteSys = GameManager::Instance().GetSpriteRenderSystem();
        if (spriteSys && spriteSys->IsClicked(m_buttonEntity))
        {
            SceneManager::Instance().ChangeScene(std::make_unique<TitleScene>(), 1.0f);
        }
    }

    // F1 toggles Editor::Draw()'s own s_showEditor flag internally, same as GameScene/TitleScene -
    // hidden by default here since the debug windows would clutter the result screen, but callable
    // on demand for debugging.
    Editor::Draw();

    Renderer::EndFrame();
}

// 評価ランク: クリアタイム(秒)に、生き延びたハント1回につきkHuntPenalty秒を足した値で決める
// (ハントを起こさせずに早く祓うほど高評価)
char ResultScene::CalcRank() const
{
    const float kHuntPenalty = 30.0f;
    const float kRankS = 240.0f; // 4分
    const float kRankA = 420.0f; // 7分
    const float kRankB = 600.0f; // 10分
    float score = m_timeTaken + kHuntPenalty * (float)m_huntsSurvived;
    if (score <= kRankS)
    {
        return 'S';
    }
    if (score <= kRankA)
    {
        return 'A';
    }
    if (score <= kRankB)
    {
        return 'B';
    }
    return 'C';
}

void ResultScene::DrawClearReport()
{
    // スプライトと同じ1280x720の仮想座標で配置し、実際のウィンドウの大きさへ拡大縮小して描く。
    // ImGui 1.92はフォントを必要なサイズでその都度ラスタライズするので、大きく描いてもぼやけない。
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sx = vp->Size.x / 1280.0f;
    const float sy = vp->Size.y / 720.0f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList(); // スプライトの上、エディタのウィンドウより下
    // 筆文字フォント(GraphicsDeviceが"ResultFont"という名前で読み込む)。無ければ標準フォントで描く
    ImFont* font = ImGui::GetFont();
    for (ImFont* f : ImGui::GetIO().Fonts->Fonts)
    {
        if (strcmp(f->GetDebugName(), "ResultFont") == 0)
        {
            font = f;
            break;
        }
    }

    // 時間差でフェードインさせる(start秒から0.6秒かけて)
    auto fade = [&](float start)
    { return std::clamp((m_time - start) / 0.6f, 0.0f, 1.0f); };
    auto toScreen = [&](float x, float y)
    { return ImVec2(vp->Pos.x + x * sx, vp->Pos.y + y * sy); };
    // align: 0=左揃え, 0.5=中央, 1=右揃え。影を付けて、明るい背景の上でも読めるようにする
    auto text = [&](const char* str, float x, float y, float size, ImVec4 color, float alpha, float align = 0.0f)
    {
        if (alpha <= 0.0f)
        {
            return;
        }
        float px = size * sy;
        ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, str);
        ImVec2 pos = toScreen(x, y);
        pos.x -= ts.x * align;
        pos.y -= ts.y * 0.5f;
        float shadow = (std::max)(2.0f, px * 0.05f);
        dl->AddText(font, px, ImVec2(pos.x + shadow, pos.y + shadow), ImGui::GetColorU32(ImVec4(0, 0, 0, 0.85f * alpha)), str);
        dl->AddText(font, px, pos, ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w * alpha)), str);
    };

    const ImVec4 kInk = {0.94f, 0.89f, 0.79f, 1.0f}; // 古い紙のような色
    const ImVec4 kLabel = {0.80f, 0.72f, 0.60f, 1.0f};
    const ImVec4 kTitle = {0.78f, 0.16f, 0.12f, 1.0f}; // 朱色

    // 見出し
    text(u8"除霊完了", 640.0f, 150.0f, 84.0f, kTitle, fade(0.2f), 0.5f);

    // ゴーストの正体
    const char* ghostName = u8"不明";
    const char* ghostDesc = "";
    switch (m_ghostType)
    {
    case (int)GhostAI::GhostType::Jibakurei:
        ghostName = u8"地縛霊";
        ghostDesc = u8"その場所に縛られた霊。刺激には反応しない。";
        break;
    case (int)GhostAI::GhostType::Kageonna:
        ghostName = u8"影女";
        ghostDesc = u8"明かりを嫌って消して回る影の妖怪。部屋の電気の下でだけ祓える。";
        break;
    case (int)GhostAI::GhostType::Yanari:
        ghostName = u8"家鳴";
        ghostDesc = u8"家を軋ませ、音に寄ってくる小鬼。";
        break;
    }
    const float kLabelX = 230.0f;
    const float kValueX = 520.0f;
    text(u8"ゴーストの正体", kLabelX, 265.0f, 30.0f, kLabel, fade(1.0f));
    text(ghostName, kValueX, 265.0f, 44.0f, kInk, fade(1.0f));
    text(ghostDesc, kValueX, 312.0f, 21.0f, kLabel, fade(1.0f));

    // クリアタイム
    int totalSec = (int)m_timeTaken;
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), u8"%d分%02d秒", totalSec / 60, totalSec % 60);
    text(u8"クリアタイム", kLabelX, 385.0f, 30.0f, kLabel, fade(1.4f));
    text(timeStr, kValueX, 385.0f, 40.0f, kInk, fade(1.4f));

    // 生き延びたハント
    char huntStr[32];
    snprintf(huntStr, sizeof(huntStr), u8"%d回", m_huntsSurvived);
    text(u8"生き延びたハント", kLabelX, 465.0f, 30.0f, kLabel, fade(1.8f));
    text(huntStr, kValueX, 465.0f, 40.0f, kInk, fade(1.8f));

    // 評価ランク: 判子を押すように、大きい所から縮みながら現れる
    char rank = CalcRank();
    ImVec4 rankColor;
    switch (rank)
    {
    case 'S':
        rankColor = {0.95f, 0.78f, 0.25f, 1.0f};
        break; // 金
    case 'A':
        rankColor = {0.85f, 0.20f, 0.15f, 1.0f};
        break; // 朱
    case 'B':
        rankColor = {0.85f, 0.80f, 0.70f, 1.0f};
        break;
    default:
        rankColor = {0.60f, 0.58f, 0.55f, 1.0f};
        break;
    }
    const float kStampStart = 2.4f;
    float stamp = std::clamp((m_time - kStampStart) / 0.25f, 0.0f, 1.0f);
    float stampScale = 1.0f + (1.0f - stamp) * 0.8f;
    char rankStr[2] = {rank, '\0'};
    text(u8"評価", 1010.0f, 265.0f, 30.0f, kLabel, fade(kStampStart - 0.3f), 0.5f);
    text(rankStr, 1010.0f, 390.0f, 160.0f * stampScale, rankColor, stamp, 0.5f);
}

void ResultScene::Unload()
{
    auto& ecs = GameManager::Instance().GetECS();
    if (m_bgEntity != 0)
    {
        ecs.DestroyEntity(m_bgEntity);
    }
    if (m_deathEntity != 0)
    {
        ecs.DestroyEntity(m_deathEntity);
    }
    if (m_buttonEntity != 0)
    {
        ecs.DestroyEntity(m_buttonEntity);
    }
}
