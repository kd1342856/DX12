#include "Editor.h"
#include "../../Manager/Scene.h"
#include "../../../Graphics/Buffer/RenderTarget/RenderTarget.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../Object/GameObject.h"
#include "../../ECS/ComponentBase.h"
#include "../../ECS/Components/ModelRendererComponent.h"
#include "../../DirectX/Utility/Input.h"
#include "../../ECS/Components/AnimationComponent.h"
#include <fstream>
#include <filesystem>
#include <regex>

static void GenerateScriptFiles(const std::string& name, const std::string& path) {
    std::filesystem::create_directories(path);
    std::string hFile = path + name + ".h";
    std::string cppFile = path + name + ".cpp";

    std::string cleanPath = path;
    for (char& c : cleanPath) {
        if (c == '/') c = '\\';
    }
    if (!cleanPath.empty() && cleanPath.back() != '\\') {
        cleanPath += '\\';
    }

    // Calculate relative path to Source directory
    int slashCount = 0;
    for (char c : cleanPath) {
        if (c == '\\') slashCount++;
    }
    std::string relPath = "";
    // If cleanPath starts with Source\, slashCount is depth. To get to Source, we need slashCount - 1 "../"
    int upCount = slashCount > 0 ? slashCount - 1 : 0;
    for (int i = 0; i < upCount; ++i) {
        relPath += "../";
    }

    std::ofstream h(hFile);
    if(h.is_open()) {
        h << "#pragma once\n";
        h << "#include \"" << relPath << "Framework/ECS/Components/ScriptComponent.h\"\n\n";
        h << "class " << name << " : public ScriptComponent {\n";
        h << "public:\n";
        h << "    void Awake() override;\n";
        h << "    void Start() override;\n";
        h << "    void Update() override;\n";
        h << "    void PostUpdate() override;\n";
        h << "    void PreDraw() override;\n";
        h << "    void Draw() override;\n";
        h << "    void Serialize(nlohmann::json& out) const override;\n";
        h << "    void Deserialize(const nlohmann::json& in) override;\n";
        h << "    void ImGuiUpdate() override;\n";
        h << "};\n";
        h.close();
    }

    std::ofstream c(cppFile);
    if(c.is_open()) {
        c << "#include \"Pch.h\"\n";
        c << "#include \"" << name << ".h\"\n\n";
        c << "REGISTER_COMPONENT(" << name << ");\n\n";
        c << "void " << name << "::Awake() {\n}\n\n";
        c << "void " << name << "::Start() {\n}\n\n";
        c << "void " << name << "::Update() {\n}\n\n";
        c << "void " << name << "::PostUpdate() {\n}\n\n";
        c << "void " << name << "::PreDraw() {\n}\n\n";
        c << "void " << name << "::Draw() {\n}\n\n";
        c << "void " << name << "::Serialize(nlohmann::json& out) const {\n}\n\n";
        c << "void " << name << "::Deserialize(const nlohmann::json& in) {\n}\n\n";
        c << "void " << name << "::ImGuiUpdate() {\n}\n";
        c.close();
    }

    std::string vcxprojPath = "DX12Framework.vcxproj";
    std::string filtersPath = "DX12Framework.vcxproj.filters";
    
    std::string filterName = cleanPath;
    if (!filterName.empty() && filterName.back() == '\\') {
        filterName.pop_back();
    }

    std::string hEntry = "    <ClInclude Include=\"" + cleanPath + name + ".h\" />\n";
    std::string cppEntry = "    <ClCompile Include=\"" + cleanPath + name + ".cpp\" />\n";

    std::ifstream v(vcxprojPath);
    std::string vContent((std::istreambuf_iterator<char>(v)), std::istreambuf_iterator<char>());
    v.close();
    
    size_t incPos = vContent.find("<ClInclude Include=");
    if(incPos != std::string::npos) vContent.insert(incPos, hEntry);
    
    size_t compPos = vContent.find("<ClCompile Include=");
    if(compPos != std::string::npos) vContent.insert(compPos, cppEntry);
    
    std::ofstream vOut(vcxprojPath);
    vOut << vContent;
    vOut.close();

    std::ifstream f(filtersPath);
    std::string fContent((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    std::string hFilter = "    <ClInclude Include=\"" + cleanPath + name + ".h\">\n      <Filter>" + filterName + "</Filter>\n    </ClInclude>\n";
    std::string cppFilter = "    <ClCompile Include=\"" + cleanPath + name + ".cpp\">\n      <Filter>" + filterName + "</Filter>\n    </ClCompile>\n";

    // Recursively add all missing parent filters
    std::string currentFilter = filterName;
    while (!currentFilter.empty()) {
        std::string filterDef = "    <Filter Include=\"" + currentFilter + "\" />\n";
        if(fContent.find("<Filter Include=\"" + currentFilter + "\"") == std::string::npos) {
            size_t fTopPos = fContent.find("<ItemGroup>") + 11;
            fContent.insert(fTopPos, "\n" + filterDef);
        }
        size_t slashPos = currentFilter.find_last_of('\\');
        if (slashPos != std::string::npos) {
            currentFilter = currentFilter.substr(0, slashPos);
        } else {
            break;
        }
    }

    size_t fIncPos = fContent.find("<ClInclude Include=");
    if(fIncPos != std::string::npos) fContent.insert(fIncPos, hFilter);
    
    size_t fCompPos = fContent.find("<ClCompile Include=");
    if(fCompPos != std::string::npos) fContent.insert(fCompPos, cppFilter);
    
    std::ofstream fOut(filtersPath);
    fOut << fContent;
    fOut.close();
}

std::shared_ptr<GameObject> Editor::s_selectedObject = nullptr;
std::string Editor::s_selectedAssetPath = "";
std::string Editor::s_currentAssetDir = "Asset/Model";
int Editor::s_selectedModelType = 0;
bool Editor::s_editorMode = true;

static bool s_showCreateScriptPopup = false;
static char s_newScriptName[256] = "";
static char s_newScriptPath[256] = "Source/Application/Object/Script/";

void Editor::DrawHierarchyAndInspector(Scene* scene) {
    if (!scene) return;

    Logger::Instance().DrawImGuiWindow();

    // Editor Control Window
    ImGui::Begin("Editor Control");
    if (ImGui::BeginTabBar("EditorControlTabs")) {
        if (ImGui::BeginTabItem("Scene")) {
            ImGui::Checkbox("Editor Mode", &s_editorMode);
            
            bool debugWire = CollisionManager::Instance().IsDebugWireEnabled();
            if (ImGui::Checkbox("Debug Wire", &debugWire)) {
                CollisionManager::Instance().SetDebugWireEnabled(debugWire);
            }
            ImGui::Separator();
            if (ImGui::Button("Save Scene (Ctrl+S)")) {
                nlohmann::json j;
                scene->Serialize(j);
                std::ofstream o("Asset/Data/Scene/GameScene.json");
                o << std::setw(4) << j << std::endl;
                Logger::Instance().AddLog(Logger::LogLevel::Info, "シーンを保存しました");
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
                Logger::Instance().AddLog(Logger::LogLevel::Info, "シーンを保存しました");
    }

    // Delete Key (Using Framework Input to bypass ImGui mapping issues)
    if (Input::Instance().IsKeyTrigger(VK_DELETE) && !ImGui::GetIO().WantTextInput) {
        if (s_selectedObject) 
        {
            s_selectedObject->Destroy();
            s_selectedObject = nullptr;
        }
    }

    // Hierarchy
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
                if (ImGui::MenuItem("Create New Script...")) {
                    s_showCreateScriptPopup = true;
                    s_newScriptName[0] = '\0';
                }
                ImGui::Separator();

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

            if (s_showCreateScriptPopup) {
                ImGui::OpenPopup("Create New Script Component");
            }
            if (ImGui::BeginPopupModal("Create New Script Component", &s_showCreateScriptPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("Script Name", s_newScriptName, IM_ARRAYSIZE(s_newScriptName));
                ImGui::InputText("Save Path", s_newScriptPath, IM_ARRAYSIZE(s_newScriptPath));
                
                if (ImGui::Button("Create", ImVec2(120, 0))) {
                    std::string name = s_newScriptName;
                    std::string path = s_newScriptPath;
                    if (!name.empty()) {
                        GenerateScriptFiles(name, path);
                    }
                    s_showCreateScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    s_showCreateScriptPopup = false;
                    ImGui::CloseCurrentPopup();
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
    if (ImGui::Button("?K?p (Apply to Selected Object)")) {
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


void Editor::DrawGameView(RenderTarget* pRenderTarget, uint32_t cameraEntity, bool fullscreen)
{
    if (!pRenderTarget) return;

    // ?w?i??????????AE????????E?B???h?E?????
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
        // 16:9??A?X?y?N?g?????\???T?C?Y?v?E
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

        // ???????E???????E?J?[?\????u????
        ImVec2 offset = ImVec2((contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f);
        ImGui::SetCursorPos(ImGui::GetCursorPos() + offset);

        // ??????ImGui?pSRV?C???`E???N?X??g?p????
        auto srvHandle = GraphicsDevice::Instance().GetImGuiSRVGPUHandle(pRenderTarget->GetImGuiSRVIndex());
        ImGui::Image((ImTextureID)srvHandle.ptr, displaySize);
    }
    ImGui::End();
}






