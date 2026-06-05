import sys

file_path = r'C:\GitHub\DX12\Source\Framework\ImGuiEditor\Editor\Editor.cpp'
with open(file_path, 'r', encoding='shift_jis') as f:
    content = f.read()

# 1. Add static variables
target1 = 'bool Editor::s_editorMode = true;\n'
vars = '''static bool s_showCreateScriptPopup = false;
static char s_newScriptName[256] = "";
static char s_newScriptPath[256] = "Source/Application/Object/Script/";

'''
content = content.replace(target1, target1 + vars)

# 2. Add GenerateScriptFiles
target2 = '#include "../../ECS/Components/AnimationComponent.h"\n'
func = r'''
#include <fstream>
#include <filesystem>

static void GenerateScriptFiles(const std::string& name, const std::string& path) {
    std::filesystem::create_directories(path);
    std::string hFile = path + name + ".h";
    std::string cppFile = path + name + ".cpp";

    std::ofstream h(hFile);
    if(h.is_open()) {
        h << "#pragma once\n";
        h << "#include \"../../../Framework/ECS/Components/ScriptComponent.h\"\n\n";
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
    
    std::string hEntry = "    <ClInclude Include=\"Source\\Application\\Object\\Script\\" + name + ".h\" />\n";
    std::string cppEntry = "    <ClCompile Include=\"Source\\Application\\Object\\Script\\" + name + ".cpp\" />\n";

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

    std::string hFilter = "    <ClInclude Include=\"Source\\Application\\Object\\Script\\" + name + ".h\">\n      <Filter>Source\\Application\\Object\\Script</Filter>\n    </ClInclude>\n";
    std::string cppFilter = "    <ClCompile Include=\"Source\\Application\\Object\\Script\\" + name + ".cpp\">\n      <Filter>Source\\Application\\Object\\Script</Filter>\n    </ClCompile>\n";

    std::string newFilterDef = "    <Filter Include=\"Source\\Application\\Object\\Script\" />\n";
    if(fContent.find("<Filter Include=\"Source\\Application\\Object\\Script\"") == std::string::npos) {
        size_t fTopPos = fContent.find("<ItemGroup>") + 11;
        fContent.insert(fTopPos, "\n" + newFilterDef);
    }

    size_t fIncPos = fContent.find("<ClInclude Include=");
    if(fIncPos != std::string::npos) fContent.insert(fIncPos, hFilter);
    
    size_t fCompPos = fContent.find("<ClCompile Include=");
    if(fCompPos != std::string::npos) fContent.insert(fCompPos, cppFilter);
    
    std::ofstream fOut(filtersPath);
    fOut << fContent;
    fOut.close();
}
'''
content = content.replace(target2, target2 + func)

# 3. Add Menu Item
target3 = '            if (ImGui::BeginPopup("AddComponentPopup")) {\n'
menu = '''                if (ImGui::MenuItem("Create New Script...")) {
                    s_showCreateScriptPopup = true;
                    s_newScriptName[0] = '\0';
                }
                ImGui::Separator();
'''
content = content.replace(target3, target3 + menu)

# 4. Add Modal Dialog
target4 = '                ImGui::EndPopup();\n            }\n'
modal = r'''
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
'''
content = content.replace(target4, target4 + modal)

# 5. Fix DrawGameView signature
content = content.replace('void Editor::DrawGameView(RenderTarget* pRenderTarget, bool fullscreen)', 'void Editor::DrawGameView(RenderTarget* pRenderTarget, uint32_t cameraEntity, bool fullscreen)')

with open(file_path, 'w', encoding='shift_jis') as f:
    f.write(content)
