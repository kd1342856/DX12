#include "../../../Pch.h"
#include "ResultScene.h"
#include "../TitleScene/TitleScene.h"
#include "../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../Graphics/Shader/FogShader/FogShader.h"
#include "../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/Components/Data/SpriteData.h"
#include "../../../Framework/ECS/Components/Data/TransformData.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <imgui.h>
#include <algorithm>

ResultScene::ResultScene(bool isClear, float timeTaken, const std::string& ghostName, int reward)
    : m_isClear(isClear), m_timeTaken(timeTaken), m_ghostName(ghostName), m_reward(reward)
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

    if (m_isClear) {
        m_bgEntity = ecs.CreateEntity();
        TransformData bgTransform;
        bgTransform.m_position = { 640.0f, 360.0f, 0.0f };
        SpriteData bgSprite;
        bgSprite.m_filePath = "Asset/Texture/UI/bg_clear.png";
        bgSprite.m_size = { 1280.0f, 720.0f };
        bgSprite.m_orderInLayer = 100;
        ecs.AddComponent(m_bgEntity, bgTransform);
        ecs.AddComponent(m_bgEntity, bgSprite);
    } else {
        m_deathEntity = ecs.CreateEntity();
        TransformData dTransform;
        dTransform.m_position = { 640.0f, 360.0f, 0.0f };
        SpriteData dSprite;
        dSprite.m_filePath = "Asset/Texture/UI/Death.png";
        dSprite.m_size = { 600.0f, 400.0f };
        dSprite.m_color = { 1.0f, 1.0f, 1.0f, 0.0f }; // Start invisible
        dSprite.m_orderInLayer = 50;
        ecs.AddComponent(m_deathEntity, dTransform);
        ecs.AddComponent(m_deathEntity, dSprite);
    }

    m_buttonEntity = ecs.CreateEntity();
    TransformData bTransform;
    bTransform.m_position = { 640.0f, 600.0f, 0.0f };
    SpriteData bSprite;
    bSprite.m_filePath = "Asset/Texture/UI/BackToTitle.png";
    bSprite.m_size = { 300.0f, 200.0f };
    bSprite.m_color = { 1.0f, 1.0f, 1.0f, 0.0f }; // Start invisible
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
    if (m_buttonEntity != 0) {
        auto& btnSprite = ecs.GetComponent<SpriteData>(m_buttonEntity);
        btnSprite.m_color.w = uiAlpha; // Fade in after 3 seconds
    }
    
    if (!m_isClear && m_deathEntity != 0) {
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
    
    if (m_isClear) {
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
        if (spriteRenderSystem) {
            spriteRenderSystem->Render();
        }
    } else {
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
        ShaderLibrary::Instance().Get<FogShader>().Draw(m_time);

        auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
        if (spriteRenderSystem) {
            spriteRenderSystem->Render();
        }
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("ResultMenu", nullptr, windowFlags);

    if (m_isClear) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.8f, 1.0f));
        ImGui::SetWindowFontScale(2.0f);
        ImGui::Spacing();
        ImGui::Text("Ghost Identified: %s", m_ghostName.c_str());
        ImGui::Text("Time Taken: %.1f s", m_timeTaken);
        ImGui::Text("Reward: $%d", m_reward);
        ImGui::PopStyleColor();
    }
    
    if (uiAlpha > 0.5f) {
        // Invisible button to catch clicks over the sprite area
        ImGui::SetCursorPos(ImVec2(center.x - 150.0f, center.y + 150.0f));
        if (ImGui::InvisibleButton("ReturnBtn", ImVec2(300.0f, 200.0f))) {
            SceneManager::Instance().ChangeScene(std::make_unique<TitleScene>(), 1.0f);
        }
    }

    ImGui::End();

    // F1 toggles Editor::Draw()'s own s_showEditor flag internally, same as GameScene/TitleScene -
    // hidden by default here since the debug windows would clutter the result screen, but callable
    // on demand for debugging.
    Editor::Draw();

    Renderer::EndFrame();
}

void ResultScene::Unload()
{
    auto& ecs = GameManager::Instance().GetECS();
    if (m_bgEntity != 0) ecs.DestroyEntity(m_bgEntity);
    if (m_deathEntity != 0) ecs.DestroyEntity(m_deathEntity);
    if (m_buttonEntity != 0) ecs.DestroyEntity(m_buttonEntity);
}
