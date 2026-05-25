#include "Editor.h"
#include "../../Manager/Scene.h"
#include "../../../Graphics/Buffer/RenderTarget/RenderTarget.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../Object/GameObject.h"
#include "../../ECS/ComponentBase.h"
#include "../../ECS/Components/ModelRendererComponent.h"
#include "../../DirectX/Utility/Input.h"
#include "../../ECS/Components/AnimationComponent.h"

std::shared_ptr<GameObject> Editor::s_selectedObject = nullptr;
std::string Editor::s_selectedAssetPath = "";
std::string Editor::s_currentAssetDir = "Asset/Model";
int Editor::s_selectedModelType = 0;

void Editor::DrawHierarchyAndInspector(Scene* scene) {
    if (!scene) return;

    // Editor Control Window
    ImGui::Begin("Editor Control");
    if (ImGui::BeginTabBar("EditorControlTabs")) {
        if (ImGui::BeginTabItem("Scene")) {
            if (ImGui::Button("Save Scene (Ctrl+S)")) {
                nlohmann::json j;
                scene->Serialize(j);
                std::ofstream o("Asset/Data/Scene/GameScene.json");
                o << std::setw(4) << j << std::endl;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // Ctrl+S
    if (ImGui::IsKeyPressed(ImGuiKey_S, false) && ImGui::GetIO().KeyCtrl) {
        nlohmann::json j;
        scene->Serialize(j);
        std::ofstream o("Asset/Data/Scene/GameScene.json");
        o << std::setw(4) << j << std::endl;
    }

    // Delete Key (Using Framework Input to bypass ImGui mapping issues)
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Delete) && !ImGui::GetIO().WantTextInput) {
        if (s_selectedObject) 
        {
            s_selectedObject->Destroy();
            s_selectedObject = nullptr;
        }
    }

    // Hierarchy
    ImGui::Begin("Hierarchy");
    
    // 閭梧勹縺ｧ縺ｮ蜿ｳ繧ｯ繝ｪ繝・け縺ｧ遨ｺ縺ｮ繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ菴懈・
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
    
    // 繧ｦ繧｣繝ｳ繝峨え蜈ｨ菴薙∈縺ｮ繝峨Ο繝・・縺ｧ隕ｪ繧定ｧ｣髯､・医Ν繝ｼ繝磯嚴螻､縺ｸ遘ｻ蜍包ｼ・
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

    // Inspector
    ImGui::Begin("Inspector");
    if (s_selectedObject) 
    {
        // Name Edit
        char nameBuffer[256];
        strcpy_s(nameBuffer, s_selectedObject->GetName().c_str());
        if (ImGui::InputText("Name", nameBuffer, 256)) {
            s_selectedObject->SetName(nameBuffer);
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            s_selectedObject->Destroy();
            s_selectedObject = nullptr;
        }

        if (s_selectedObject) {
            ImGui::Separator();

            // Components
            for (auto& comp : s_selectedObject->GetComponentsList())
            {
                if (ImGui::CollapsingHeader(comp->GetComponentName(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    comp->ImGuiUpdate();
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                auto& factories = ClassAssembly::Instance().GetFactories();
                for (auto& pair : factories) {
                    if (ImGui::MenuItem(pair.first.c_str())) {
                        auto newComp = ClassAssembly::Instance().Create(pair.first);
                        if (newComp) {
                            s_selectedObject->AddComponent(newComp);
                        }
                    }
                }
                ImGui::Separator();

                bool hasAnim = s_selectedObject->GetComponent<AnimationComponent>() != nullptr;
                if (ImGui::MenuItem("AnimationComponent", nullptr, false, !hasAnim))
                {
                    s_selectedObject->AddComponent<AnimationComponent>();
                }
                ImGui::EndPopup();
            }
        } 
    } else {
        ImGui::Text("Nothing Selected");
    }
    ImGui::End();

    DrawAssetEditor();
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
void Editor::DrawAssetEditor() 
{
    ImGui::Begin("Asset Editor");

    if (!s_selectedObject)
    {
        ImGui::TextDisabled("No object selected in Hierarchy.");
        ImGui::End();
        return;
    }

    auto pModelComp = s_selectedObject->GetComponent<ModelRendererComponent>();
    if (!pModelComp) {
        ImGui::TextDisabled("Selected object does not have a ModelRendererComponent.");
        ImGui::End();
        return;
    }

    // Back button
    if (s_currentAssetDir != "Asset/Model" && s_currentAssetDir != "Asset\\\\Model") {
        if (ImGui::Button("Back (..)")) {
            s_currentAssetDir = std::filesystem::path(s_currentAssetDir).parent_path().string();
            if (s_currentAssetDir.empty() || s_currentAssetDir == "." || s_currentAssetDir == "Asset") s_currentAssetDir = "Asset/Model";
        }
        ImGui::Separator();
    }

    // Ensure directory exists
    if (!std::filesystem::exists(s_currentAssetDir)) {
        std::filesystem::create_directories(s_currentAssetDir);
    }

    // Directory contents
    for (const auto& entry : std::filesystem::directory_iterator(s_currentAssetDir)) {
        std::string filename = entry.path().filename().string();
        if (entry.is_directory()) {
            if (ImGui::Selectable(("[Dir] " + filename).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    s_currentAssetDir = entry.path().string();
                }
            }
        } else {
            std::string pathStr = entry.path().string();
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
            bool isSelected = (s_selectedAssetPath == pathStr);
            if (ImGui::Selectable(filename.c_str(), isSelected)) {
                s_selectedAssetPath = pathStr;
            }
        }
    }

    ImGui::Separator();

    ImGui::Text("Selected Asset: %s", s_selectedAssetPath.c_str());
    ImGui::Text("Model Type:");
    ImGui::RadioButton("Static", &s_selectedModelType, 0); ImGui::SameLine();
    ImGui::RadioButton("Dynamic", &s_selectedModelType, 1);
    if (ImGui::Button("驕ｩ逕ｨ (Apply to Selected Object)")) {
        if (!s_selectedAssetPath.empty()) {
            auto& data = pModelComp->GetData();
            data.m_modelType = (ModelType)s_selectedModelType;
            data.m_spModelData = std::make_shared<ModelData>();
            data.m_spModelData->Load(s_selectedAssetPath);
            data.m_filePath = s_selectedAssetPath;
        }
    }

    ImGui::End();
}


void Editor::DrawGameView(RenderTarget* pRenderTarget, bool fullscreen)
{
    if (!pRenderTarget) return;

    // 背景を透過させず、黒い通常のウィンドウにする
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    if (fullscreen)
    {
        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    ImGui::Begin("Game View", nullptr, windowFlags);
    {
        // 16:9のアスペクト維持で表示サイズ計算
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        float targetAspect = 16.0f / 9.0f;
        float actualAspect = contentSize.x / contentSize.y;

        ImVec2 displaySize;
        if (actualAspect > targetAspect) {
            displaySize.y = contentSize.y;
            displaySize.x = contentSize.y * targetAspect;
        } else {
            displaySize.x = contentSize.x;
            displaySize.y = contentSize.x / targetAspect;
        }

        // 中央に寄せるためのカーソル位置調整
        ImVec2 offset = ImVec2((contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f);
        ImGui::SetCursorPos(ImGui::GetCursorPos() + offset);

        // 正しいImGui用SRVインデックスを使用する
        auto srvHandle = GraphicsDevice::Instance().GetImGuiSRVGPUHandle(pRenderTarget->GetImGuiSRVIndex());
        ImGui::Image((ImTextureID)srvHandle.ptr, displaySize);
    }
    ImGui::End();
}