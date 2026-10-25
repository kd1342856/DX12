#include "MaterialPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../../Framework/ECS/Components/Data/ModelRenderData.h"
#include "../../../Graphics/Geometry/Model/Model.h"
#include "../../../Graphics/Geometry/Mesh/Mesh.h"
#include "../../../Framework/Manager/Asset/MeshManager.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/ECS.h"
#include "../../Object/GameObject.h"

void MaterialPanel::Draw(EditorContext& ctx)
{
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!ctx.SelectedObject)
        {
            ImGui::TextDisabled("Select an object in Hierarchy to edit its material.");
            return;
        }

        auto& ecs = GameManager::Instance().GetECS();
        auto* pModelData = ecs.TryGetComponent<ModelRenderData>(ctx.SelectedObject->GetEntityID());
        
        if (!pModelData || !pModelData->m_spModelData || !pModelData->m_spModelData->IsLoaded())
        {
            ImGui::TextDisabled("Selected object has no loaded 3D Model.");
            return;
        }

        // Search for the first valid mesh material to edit.
        // A more advanced panel would list all submeshes.
        auto& nodes = pModelData->m_spModelData->GetNodesRef();
        bool foundMaterial = false;
        
        for (auto& node : nodes)
        {
            for (auto& meshHandle : node.meshes)
            {
                if (meshHandle.IsValid())
                {
                    auto pMesh = MeshManager::Instance().Get(meshHandle);
                    if (!pMesh) continue;

                    auto& material = pMesh->GetMaterialRef();
                    
                    std::string matName = material.Name.empty() ? "Unnamed Material" : material.Name;
                    ImGui::Text("Material Name: %s", matName.c_str());
                    
                    // Base Color
                    ImGui::ColorEdit4("Base Color", &material.Constants.baseColorFactor.x);
                    
                    // Metallic Roughness
                    ImGui::SliderFloat("Metallic", &material.Constants.metallicFactor, 0.0f, 1.0f);
                    ImGui::SliderFloat("Roughness", &material.Constants.roughnessFactor, 0.0f, 1.0f);
                    
                    // Emissive
                    float emissive[3] = { material.Constants.emissiveFactorX, material.Constants.emissiveFactorY, material.Constants.emissiveFactorZ };
                    if (ImGui::ColorEdit3("Emissive Factor", emissive))
                    {
                        material.Constants.emissiveFactorX = emissive[0];
                        material.Constants.emissiveFactorY = emissive[1];
                        material.Constants.emissiveFactorZ = emissive[2];
                    }
                    ImGui::DragFloat("Emissive Strength", &material.Constants.emissiveStrength, 0.01f, 0.0f, 100.0f);
                    
                    // Normal Scale
                    ImGui::SliderFloat("Normal Scale", &material.Constants.normalScale, 0.0f, 2.0f);

                    // Occlusion
                    ImGui::SliderFloat("Occlusion Strength", &material.Constants.occlusionStrength, 0.0f, 1.0f);

                    foundMaterial = true;
                    break;
                }
            }
            if (foundMaterial) break;
        }

        if (!foundMaterial)
        {
            ImGui::TextDisabled("No material found on this model.");
        }
    }
}
