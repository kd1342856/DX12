#include "GameEditor.h"
#include "../../../../Library/ImGui/imgui.h"

void GameEditor::Initialize()
{

}

void GameEditor::Draw(EditorContext& ctx)
{
    ImGui::Begin("Game Editor");
    for (auto& panel : m_Panels)
    {
        panel->Draw(ctx);
    }
    ImGui::End();
}
