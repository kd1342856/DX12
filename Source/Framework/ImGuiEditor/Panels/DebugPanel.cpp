#include "DebugPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

void DebugPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Debug) return;

    if (ImGui::CollapsingHeader("Debug Views", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginCombo("View Mode", DebugViewNames[static_cast<int>(ctx.Debug->CurrentDebugView)].Name))
        {
            for (const auto& item : DebugViewNames)
            {
                bool is_selected = (ctx.Debug->CurrentDebugView == item.Value);
                if (ImGui::Selectable(item.Name, is_selected))
                {
                    ctx.Debug->CurrentDebugView = item.Value;
                    ctx.Debug->IsDirty = true;
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // DEBUG: show the raw planar reflection render target, bypassing the glass compositing
    // entirely - to check whether the reflection camera actually renders the expected image,
    // independent of any UV-reprojection/compositing bugs.
    if (ImGui::CollapsingHeader("Reflection RT (debug)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* pRT = Renderer::GetPlanarReflectionRenderTarget();
        if (pRT && pRT->GetImGuiSRVIndex() != -1)
        {
            auto handle = GraphicsDevice::Instance().GetImGuiSRVGPUHandle(pRT->GetImGuiSRVIndex());
            ImGui::Text("%dx%d (ImGuiSRVIndex=%d)", pRT->GetWidth(), pRT->GetHeight(), pRT->GetImGuiSRVIndex());
            ImVec2 imgPos = ImGui::GetCursorScreenPos();
            ImVec2 imgSize(384, 384);
            // Bright border so we can tell "Image() drew nothing" apart from "it drew, but the
            // content itself is black" - the border is drawn regardless either way.
            ImGui::GetWindowDrawList()->AddRectFilled(imgPos, ImVec2(imgPos.x + imgSize.x, imgPos.y + imgSize.y), IM_COL32(255, 0, 255, 255));
            ImGui::Image((ImTextureID)handle.ptr, imgSize);
        }
        else
        {
            ImGui::TextDisabled("Reflection RT not available");
        }
    }
}
