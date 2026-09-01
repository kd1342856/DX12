#include "DebugPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"

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
}
