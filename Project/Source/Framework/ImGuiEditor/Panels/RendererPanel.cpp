#include "RendererPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "SettingsWidgets.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../ECS/CompSystem/Systems/RenderSystem.h"
#include "../../../Graphics/Renderer/PathTracer.h"

#ifdef _DEBUG
namespace
{
    // D3D12デバイス(スワップチェーン、全メッシュ/テクスチャ/PSO、ImGui等)をライブ再作成して
    // デバッグレイヤーの変更を再起動無しで反映するのは、得られるものの割に侵襲的で
    // リスクの大きい作業。プロセスの新しいコピーを起動してこちらを終了する方が、
    // そのリスク無しで同じ「ワンクリックで手動再起動不要」という体験を得られる。
    void RelaunchApplication()
    {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;

        // 子プロセスもAppPaths::Initializeで自分の場所からProjectフォルダを見つけ直すが、
        // 念のため、このプロセスの現在の作業ディレクトリ(=Projectフォルダ)をそのまま渡す。
        wchar_t cwd[MAX_PATH];
        DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        const wchar_t* pCwd = (cwdLen > 0 && cwdLen < MAX_PATH) ? cwd : nullptr;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, pCwd, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        // Win32メッセージループ(Window::ProcessMessage)と同じスレッドなので、次の
        // PeekMessageで拾われて、通常のシャットダウン処理がきれいに走る。
        PostQuitMessage(0);
    }
}
#endif

void RendererPanel::Draw(EditorContext& ctx)
{
    if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ctx.Renderer)
        {
            bool changed = false;

            ImGui::Text("SSR (Screen Space Reflection)");
            if (ImGui::Checkbox("Enable SSR", &ctx.Renderer->EnableSSR)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->EnableSSR);
            if (ImGui::SliderFloat("SSR Step Size", &ctx.Renderer->SSRStepSize, 0.05f, 1.5f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->SSRStepSize);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"平面反射(専用の反射カメラ)が設定されていないガラス面のフォールバック。");
            }
            if (ImGui::SliderFloat("SSR Opaque Intensity", &ctx.Renderer->SSROpaqueIntensity, 0.0f, 2.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->SSROpaqueIntensity);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"床・タイル等のOpaque材質向けSSR(NormalPrepassの法線+深度を再利用)。\n粗さ/金属度は考慮しない鏡面反射の近似(非金属F0=0.04固定)。0で無効。");
            }

            ImGui::Separator();
            ImGui::Text("Ray Tracing");
            ImGui::Checkbox("Raytraced Shadows (High+)", &ctx.Renderer->EnableRaytracedShadows);
            SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->EnableRaytracedShadows);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"スポット・点光源・月の影をレイで調べる(画質「高」以上)。反射は画質「最高」で自動的に有効。");
            }

            ImGui::Separator();
            ImGui::Text("Reference Path Tracer (dev)");
            // CPU側だけで使う設定なので、IsDirtyを待たずに毎フレーム反映される
            ImGui::BeginDisabled(!PathTracer::Instance().IsAvailable());
            ImGui::Checkbox("Enable Path Tracer", &ctx.Renderer->EnableReferencePathTracer);
            SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->EnableReferencePathTracer);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"拡散の反射を繰り返す正解の絵(カメラを止めると足し続けてノイズが消える)。\nラスタライズの描画(DDGI・反射プローブ・影)と見比べる。鏡面・スキニングのモデル・半透明は写らない。");
            }
            if (ImGui::SliderInt("Max Bounces", &ctx.Renderer->PathTracerMaxBounces, 0, 8))
            {
                PathTracer::Instance().ResetAccumulation();
            }
            if (SettingsWidgets::DefaultMark(*ctx.Renderer, ctx.Renderer->PathTracerMaxBounces))
            {
                PathTracer::Instance().ResetAccumulation();
            }
            if (ctx.Renderer->EnableReferencePathTracer)
            {
                ImGui::Text("Samples: %u", PathTracer::Instance().GetSampleCount());
            }

            if (changed) {
                ctx.Renderer->IsDirty = true;
            }
        }

        ImGui::Separator();
        ImGui::Text("Culling (debug)");
        ImGui::Checkbox("Frustum Culling", &RenderSystem::s_enableFrustumCulling);
        ImGui::Checkbox("Room Culling", &RenderSystem::s_enableRoomCulling);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Turn these off to check whether something disappearing/not rendering is\n"
                "caused by culling or is unrelated to it. Takes effect immediately.");
        }

#ifdef _DEBUG
        ImGui::Separator();
        // このセッションで実際に動いているものの実測値。GraphicsDevice::Init()で
        // 一度だけ決まる - 下のチェックボックスとは独立しているので、fpsから
        // 推測する必要が無い。これがチェックボックスと食い違っている場合、
        // チェックボックスは*次回*起動時に何が起こるかを表している(ツールチップ参照) -
        // ライブには決して反映されない。
        ImGui::Text("Debug Layer Active This Session: %s", GraphicsDevice::IsDebugLayerActive() ? "ON" : "OFF");

        static bool s_debugLayerRequested = GraphicsDevice::IsDebugLayerRequested();
        bool debugLayerChanged = ImGui::Checkbox("D3D12 Debug Layer", &s_debugLayerRequested);
        if (debugLayerChanged)
        {
            GraphicsDevice::SetDebugLayerRequested(s_debugLayerRequested);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "The D3D12 debug layer can only be enabled/disabled before the device is\n"
                "created, so it can't apply live - restart the app to pick up the change.\n"
                "Turning it off gets Debug-build performance much closer to Release; turn it\n"
                "back on when you actually need to catch a GPU-side bug.");
        }

        bool pendingChange = s_debugLayerRequested != GraphicsDevice::IsDebugLayerActive();
        if (!pendingChange) ImGui::BeginDisabled();
        if (ImGui::Button("Apply & Restart"))
        {
            RelaunchApplication();
        }
        if (!pendingChange) ImGui::EndDisabled();
        if (pendingChange)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "restart needed to apply");
        }
#endif
    }
}
