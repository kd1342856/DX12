#include "ColliderComponent.h"
#include <string>


void ColliderComponent::ImGuiUpdate() {
    auto& data = GetData();
    
    ImGui::Checkbox("Is Static", &data.m_isStatic);
    ImGui::Separator();

    if (ImGui::Button("Add Box")) {
        auto shape = std::make_shared<CollisionShapeBox>();
        shape->m_entity = GetGameObject()->GetEntityID();
        data.m_shapes.push_back(shape);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Sphere")) {
        auto shape = std::make_shared<CollisionShapeSphere>();
        shape->m_entity = GetGameObject()->GetEntityID();
        data.m_shapes.push_back(shape);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Capsule")) {
        auto shape = std::make_shared<CollisionShapeCapsule>();
        shape->m_entity = GetGameObject()->GetEntityID();
        data.m_shapes.push_back(shape);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Mesh")) {
        auto shape = std::make_shared<CollisionShapeMesh>();
        shape->m_entity = GetGameObject()->GetEntityID();
        data.m_shapes.push_back(shape);
    }
    
    ImGui::Separator();
    for (size_t i = 0; i < data.m_shapes.size(); ++i) {
        auto& shape = data.m_shapes[i];
        std::string header = shape->m_name + " [" + std::to_string(i) + "]";
        if (ImGui::TreeNode(header.c_str())) {
            
            ImGui::Checkbox("Is Trigger", &shape->m_isTrigger);
            
            shape->Editor_ImGui();

            if (ImGui::Button("Delete")) {
                data.m_shapes.erase(data.m_shapes.begin() + i);
                ImGui::TreePop();
                break;
            }
            ImGui::TreePop();
        }
    }
}

void ColliderComponent::Serialize(nlohmann::json& out) const {
    auto& data = const_cast<ColliderComponent*>(this)->GetData();
    
    nlohmann::json shapesArray = nlohmann::json::array();
    for (const auto& s : data.m_shapes) {
        nlohmann::json shapeJson;
        shapeJson["ShapeId"] = static_cast<int>(s->GetShapeId());
        shapeJson["IsTrigger"] = s->m_isTrigger;
        s->Serialize(shapeJson);
        shapesArray.push_back(shapeJson);
    }
    out["Shapes"] = shapesArray;
    out["IsStatic"] = data.m_isStatic;
}

void ColliderComponent::Deserialize(const nlohmann::json& in) {
    auto& data = GetData();
    
    if (in.contains("IsStatic")) data.m_isStatic = in["IsStatic"];

    if (in.contains("Shapes") && in["Shapes"].is_array()) {
        data.m_shapes.clear();
        for (const auto& shapeJson : in["Shapes"]) {
            if (shapeJson.contains("ShapeId")) {
                int shapeId = shapeJson["ShapeId"];
                std::shared_ptr<CollisionShape> s = nullptr;
                
                if (shapeId == CollisionShape::Box) s = std::make_shared<CollisionShapeBox>();
                else if (shapeId == CollisionShape::Sphere) s = std::make_shared<CollisionShapeSphere>();
                else if (shapeId == CollisionShape::Capsule) s = std::make_shared<CollisionShapeCapsule>();
                else if (shapeId == CollisionShape::Mesh) s = std::make_shared<CollisionShapeMesh>();

                if (s) {
                    s->m_entity = GetGameObject()->GetEntityID();
                    if (shapeJson.contains("IsTrigger")) s->m_isTrigger = shapeJson["IsTrigger"];
                    s->Deserialize(shapeJson);
                    data.m_shapes.push_back(s);
                }
            }
        }
    }
}