#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() 
{
    return Editor::GetScene();
}


void Editor::DrawHierarchy() {
    auto sceneObj = GetCurrentScenePtr();
    Scene* scene = sceneObj.get();
    if (!scene) return;
    ImGui::Begin("Hierarchy");
    
    // ?w?i??E???E???E???E?????E????E???E???E???E??u?W??E???E??g??E?????E
    if (ImGui::BeginPopupContextWindow("HierarchyPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Empty Object")) {
            scene->CreateGameObject("GameObject");
        }
        ImGui::EndPopup();
    }

    auto rootsCopy = scene->GetGameObjects();
    for (auto& obj : rootsCopy) {
        DrawHierarchyNode(obj);
    }
    
    // ??E???E????h?E??E?????E??E??h????E?E??E??e????E???E??E????[?g?K??E???E??????E??E
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
            GameObject* droppedRaw = *(GameObject**)payload->Data;
            auto droppedObj = droppedRaw->shared_from_this();
            droppedObj->SetParent(nullptr);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

}

void Editor::DrawHierarchyNode(std::shared_ptr<GameObject> obj) {
    ImGuiTreeNodeFlags flags = ((s_selectedObject == obj) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    
    bool hasChildren = !obj->GetChildren().empty();
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    ImGui::PushID(obj.get());
    bool opened = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s", obj->GetName().c_str());
    
    if (ImGui::IsItemClicked()) {
        s_selectedObject = obj;
    }

        if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Empty Child")) {
            auto child = obj->GetScene()->CreateGameObject("GameObject");
            child->SetParent(obj);
        }
        if (ImGui::MenuItem("Delete")) {
            GameObject* curr = s_selectedObject ? s_selectedObject.get() : nullptr;
            while (curr) {
                if (curr == obj.get()) {
                    s_selectedObject = nullptr;
                    break;
                }
                curr = curr->GetParent();
            }
            obj->Destroy();
        }
        if (obj->GetParent()) {
            if (ImGui::MenuItem("Unparent")) {
                obj->SetParent(nullptr);
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource()) {
        GameObject* payload = obj.get();
        ImGui::SetDragDropPayload("GAMEOBJECT", &payload, sizeof(GameObject*));
        ImGui::Text("%s", obj->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
            GameObject* droppedRaw = *(GameObject**)payload->Data;
            if (droppedRaw != obj.get()) {
                auto droppedObj = droppedRaw->shared_from_this();
                bool isChild = false;
                GameObject* curr = obj.get();
                while (curr) {
                    if (curr == droppedRaw) { isChild = true; break; }
                    curr = curr->GetParent();
                }
                if (!isChild) droppedObj->SetParent(obj);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (opened) 
    {
        if (hasChildren) 
        {
            auto childrenCopy = obj->GetChildren();
            for (auto& child : childrenCopy) 
            {
                DrawHierarchyNode(child);
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}
