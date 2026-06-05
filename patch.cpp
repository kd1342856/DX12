#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::string file = "C:\\GitHub\\DX12\\Source\\Framework\\ImGuiEditor\\Editor\\Editor.cpp";
    std::ifstream in(file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // 1. Insert static variables
    std::string varTarget = "int Editor::s_selectedModelType = 0;\n";
    size_t varPos = content.find(varTarget);
    if(varPos != std::string::npos) {
        std::string vars = "static bool s_showCreateScriptPopup = false;\nstatic char s_newScriptName[256] = \"\";\nstatic char s_newScriptPath[256] = \"Source/Application/Object/Script/\";\n";
        content.insert(varPos + varTarget.length(), vars);
    }

    // 2. Insert GenerateScriptFiles
    std::string funcTarget = "#include \"../../ECS/Components/AnimationComponent.h\"\n";
    size_t funcPos = content.find(funcTarget);
    if(funcPos != std::string::npos) {
        std::string func = R"(
#include <fstream>
#include <filesystem>
#include <regex>

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

    std::string hFilter = "    <ClInclude Include=\"Source\\Application\\Object\\Script\\" + name + ".h\">\n      <Filter>Application\\Object\\Script</Filter>\n    </ClInclude>\n";
    std::string cppFilter = "    <ClCompile Include=\"Source\\Application\\Object\\Script\\" + name + ".cpp\">\n      <Filter>Application\\Object\\Script</Filter>\n    </ClCompile>\n";

    size_t fIncPos = fContent.find("<ClInclude Include=");
    if(fIncPos != std::string::npos) fContent.insert(fIncPos, hFilter);
    
    size_t fCompPos = fContent.find("<ClCompile Include=");
    if(fCompPos != std::string::npos) fContent.insert(fCompPos, cppFilter);
    
    std::ofstream fOut(filtersPath);
    fOut << fContent;
    fOut.close();
}
)";
        content.insert(funcPos + funcTarget.length(), func);
    }

    // 3. Insert Popup Call
    std::string menuTarget = "if (ImGui::BeginPopup(\"AddComponentPopup\")) {\n";
    size_t menuPos = content.find(menuTarget);
    if(menuPos != std::string::npos) {
        std::string menu = R"(                if (ImGui::MenuItem("Create New Script...")) {
                    s_showCreateScriptPopup = true;
                    s_newScriptName[0] = '\0';
                }
                ImGui::Separator();
)";
        content.insert(menuPos + menuTarget.length(), menu);
    }

    // 4. Insert Modal
    std::string endPopupTarget = "ImGui::EndPopup();\n            }\n";
    size_t endPopupPos = content.find(endPopupTarget);
    if(endPopupPos != std::string::npos) {
        std::string modal = R"(
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
)";
        content.insert(endPopupPos + endPopupTarget.length(), modal);
    }

    std::ofstream out(file);
    out << content;
    out.close();
    return 0;
}
