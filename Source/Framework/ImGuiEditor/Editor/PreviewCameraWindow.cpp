#include "../../../Pch.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../ECS/Components/Data/CameraData.h"
#include "../../Manager/GameManager.h"

// 「Preview Camera」ウィンドウ: CameraDataの m_isDebugPreview を立てたカメラのTransformから
// 見た絵を表示するだけの専用ウィンドウ。以前はShaderEditor(Debug Panel)の中に反射RTの生画像を
// 直接埋め込んでいたが、エディタのフリーカメラを動かすとその反射カメラ自身も一緒に動いてしまい
// 中身が確認しづらかった。代わりに、普通のGameObject(TransformData+CameraData)をシーンに
// 1つ置いてTransformで自由に動かせるようにし、その見た目を毎フレーム専用RTに描画する
// (実際の描画はGameScene::RenderDebugPreviewCamera()が行う - RenderSystem::RenderScene()を
// そのまま流用しているだけで、反射カメラや平面反射のシステムとは完全に独立している)。
void Editor::DrawPreviewCamera()
{
    if (ImGui::Begin("Preview Camera"))
    {
        auto& ecs = GameManager::Instance().GetECS();
        bool found = false;
        for (auto& cam : ecs.GetComponentArray<CameraData>())
        {
            if (cam.m_isDebugPreview) { found = true; break; }
        }

        if (!found)
        {
            // u8プレフィックス必須: これが無いと、ソースがUTF-8(BOM付き)でもコンパイラの
            // 実行文字セット変換でシステムのコードページ(Shift-JIS等)に変換されてしまい、
            // UTF-8を前提にしているImGuiの表示と食い違って文字化けする。u8""はC++の仕様上、
            // 実行文字セットの設定に関わらず必ずUTF-8バイト列になることが保証されている。
            ImGui::TextWrapped(
                u8"有効なプレビューカメラがありません。CameraDataを持つGameObjectを配置し、"
                u8"InspectorのCameraDataで「Debug Preview Camera」をONにしてください。"
                u8"そのカメラは右クリックを押しながらの移動(通常のエディタ自由カメラと同じ操作)で"
                u8"このウィンドウにマウスを乗せている間だけ動かせます。");
        }

        auto* pRT = Renderer::GetDebugPreviewRenderTarget();
        if (pRT && pRT->GetImGuiSRVIndex() != -1)
        {
            auto handle = GraphicsDevice::Instance().GetImGuiSRVGPUHandle(pRT->GetImGuiSRVIndex());
            ImGui::Text("%dx%d (ImGuiSRVIndex=%d)", pRT->GetWidth(), pRT->GetHeight(), pRT->GetImGuiSRVIndex());
            float availWidth = ImGui::GetContentRegionAvail().x;
            if (availWidth < 64.0f) availWidth = 64.0f;
            float aspect = (float)pRT->GetHeight() / (float)pRT->GetWidth();
            ImGui::Image((ImTextureID)handle.ptr, ImVec2(availWidth, availWidth * aspect));
            // このウィンドウの映像に乗っている間だけ、右クリックドラッグでの操作を有効にする
            // (GameScene::UpdateCameraがこのフラグを見て、通常のエディタ自由カメラと衝突しないようにする)。
            s_previewCameraHovered = ImGui::IsItemHovered();
        }
        else
        {
            ImGui::TextDisabled("Preview RT not available");
        }
    }
    ImGui::End();
}
