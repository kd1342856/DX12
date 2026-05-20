#include "GameScene.h"
#include <fstream>
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Framework/ECS/Components/TransformComponent.h"
#include "../../../Framework/ECS/Components/CameraComponent.h"
#include "../../../Framework/ECS/Components/ModelRendererComponent.h"
#include "../../../Framework/ECS/Components/ScriptComponent.h"
#include "../../../Framework/ECS/Components/PostProcessComponent.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"

void GameScene::Init()
{
    m_upRenderTarget = std::make_unique<RenderTarget>();
    m_upRenderTarget->Create(1280, 720);

    GameManager::Instance().Init();

    m_spScene = std::make_shared<Scene>();
    m_spScene->Init();

    std::ifstream in("Asset/Data/Scene/GameScene.json");
    if (in.is_open()) {
        nlohmann::json j;
        in >> j;
        m_spScene->Deserialize(j);
    } else 
    {
        auto cameraObj = m_spScene->CreateGameObject("MainCamera");
        auto pCamT = cameraObj->AddComponent<TransformComponent>();
        pCamT->GetData().m_position = Math::Vector3(0, 0, -5.0f);
        cameraObj->AddComponent<CameraComponent>();
        cameraObj->AddComponent<PostProcessComponent>(); // Add PostProcessComponent to Camera
    }
}

void GameScene::Update()
{
    // Scene Update
    m_spScene->Update();

    // 1. Render to RenderTarget
    GraphicsDevice::Instance().SetRenderTarget(m_upRenderTarget.get());
    m_upRenderTarget->Clear(0.2f, 0.2f, 0.3f, 1.0f);
    m_spScene->GetRenderSystem()->Update();

    GraphicsDevice::Instance().SetResourceBarrier(
        m_upRenderTarget->GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    // 2. Render to BackBuffer
    GraphicsDevice::Instance().SetBackBuffer();

    
    // Get exposure from MainCamera's PostProcessComponent if it exists
    float exposure = 1.0f;
    for (auto& obj : m_spScene->GetGameObjects()) {
        if (obj->GetName() == "MainCamera") {
            if (auto ppComp = obj->GetComponent<PostProcessComponent>()) {
                exposure = ppComp->GetData().m_exposure;
            }
            break;
        }
    }

    // PostProcess Draw
    //CBufferData::PostProcess cbPP;
    //cbPP.Exposure = exposure;
    //GDF::Instance().BindCBuffer(0, cbPP);
    //ShaderManager::Instance().m_postProcessShader.Draw(m_upRenderTarget.get());

    ImGui::Begin("Game View");
    {
        // ウインドウのサイズに合わせてゲーム画面を引き伸ばして表示
        ImVec2 viewSize = ImGui::GetContentRegionAvail();

        // 前回の修正で作った ImGui用のSRVインデックスからGPUハンドルを取得して描画
        auto srvHandle = GraphicsDevice::Instance().GetImGuiSRVGPUHandle(m_upRenderTarget->GetImGuiSRVIndex());

        // ImGuiのウインドウ内にレンダーターゲットの絵を貼り付ける
        ImGui::Image((ImTextureID)srvHandle.ptr, viewSize);
    }
    ImGui::End();

    // Editor UI
    Editor::DrawHierarchyAndInspector(m_spScene.get());
}