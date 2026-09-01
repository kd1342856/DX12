#include "../../../../Pch.h"
#include "AutoMirrorComponent.h"
#include "ReflectionComponent.h"
#include "../../../../Framework/Manager/GameManager.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/ECS/Components/Data/ModelRenderData.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../../Graphics/Geometry/Model/Model.h"
#include "../../../../Graphics/Geometry/Mesh/Mesh.h"
#include "../../../../Graphics/GPUResource/Material/Material.h"
#include "../../../../Framework/Manager/Asset/MeshManager.h"
#include <functional>
#include <unordered_map>

REGISTER_COMPONENT(AutoMirrorComponent);

void AutoMirrorComponent::Start()
{
}

void AutoMirrorComponent::GenerateMirrors()
{
    auto& ecs = GameManager::Instance().GetECS();
    if (!m_pGameObject) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AutoMirror: m_pGameObject is null");
        return;
    }
    
    // 子階層も含めて ModelRenderData を探す
    ModelRenderData* pModelRender = nullptr;
    std::function<void(GameObject*)> findModelRender = [&](GameObject* obj) {
        if (pModelRender) return;
        if (ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID())) {
            pModelRender = &obj->GetComponent<ModelRenderData>();
            return;
        }
        for (auto& child : obj->GetChildren()) {
            findModelRender(child.get());
        }
    };
    findModelRender(m_pGameObject);
    
    if (!pModelRender) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AutoMirror: Could not find ModelRenderData in %s or its children", m_pGameObject->GetName().c_str());
        return;
    }

    if (!pModelRender->m_spModelData) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AutoMirror: ModelData is null");
        return;
    }
    if (!pModelRender->m_spModelData->IsLoaded()) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AutoMirror: ModelData is not loaded yet");
        return;
    }
    
    auto& nodes = pModelRender->m_spModelData->GetNodes();
    
    // キーワードを小文字にして分割
    std::string keys = m_targetKeywords;
    std::transform(keys.begin(), keys.end(), keys.begin(), ::tolower);
    std::vector<std::string> keywords;
    size_t pos = 0;
    while ((pos = keys.find(',')) != std::string::npos) {
        keywords.push_back(keys.substr(0, pos));
        keys.erase(0, pos + 1);
    }
    keywords.push_back(keys);

    int mirrorCount = 0;

    for (const auto& node : nodes) {
        for (const auto& meshHandle : node.meshes) {
            auto mesh = MeshManager::Instance().Get(meshHandle);
            if (!mesh) continue;
            
            std::string matName = mesh->GetMaterial().Name;
            std::string searchTarget = node.name + "_" + matName;
            Logger::Instance().AddLog(Logger::LogLevel::Info, "AutoMirror checking: %s", searchTarget.c_str());
            std::transform(searchTarget.begin(), searchTarget.end(), searchTarget.begin(), ::tolower);
            
            bool match = false;
            for (const auto& kw : keywords) {
                if (!kw.empty() && searchTarget.find(kw) != std::string::npos) {
                    match = true;
                    break;
                }
            }

            if (match) {
                auto& verts = mesh->GetVertices();
                auto& faces = mesh->GetFaces();
                if (verts.empty()) continue;

                // Union-Find で連結成分（離れた窓）をクラスタリング
                std::vector<int> parent(verts.size());
                for(size_t i=0; i<verts.size(); ++i) parent[i] = (int)i;
                
                auto find = [&](int i, auto& find_ref) -> int {
                    if (parent[i] == i) return i;
                    return parent[i] = find_ref(parent[i], find_ref);
                };
                auto unite = [&](int i, int j, auto& find_ref) {
                    int rootI = find_ref(i, find_ref);
                    int rootJ = find_ref(j, find_ref);
                    if (rootI != rootJ) {
                        parent[rootI] = rootJ;
                    }
                };
                
                for (const auto& face : faces) {
                    unite(face.Idx[0], face.Idx[1], find);
                    unite(face.Idx[1], face.Idx[2], find);
                }
                
                struct ClusterInfo {
                    Math::Vector3 centerSum = {0,0,0};
                    Math::Vector3 normalSum = {0,0,0};
                    int count = 0;
                };
                std::unordered_map<int, ClusterInfo> clusters;
                
                for (size_t i = 0; i < verts.size(); ++i) {
                    int root = find((int)i, find);
                    clusters[root].centerSum += verts[i].Position;
                    clusters[root].normalSum += verts[i].Normal;
                    clusters[root].count++;
                }
                
                std::vector<ClusterInfo> mergedClusters;
                float mergeThresholdSq = 2.0f * 2.0f;

                for (auto& pair : clusters) {
                    ClusterInfo& info = pair.second;
                    Math::Vector3 center = info.centerSum / (float)info.count;
                    
                    bool merged = false;
                    for (auto& mc : mergedClusters) {
                        Math::Vector3 mcCenter = mc.centerSum / (float)mc.count;
                        if (Math::Vector3::DistanceSquared(center, mcCenter) < mergeThresholdSq) {
                            mc.centerSum += info.centerSum;
                            mc.normalSum += info.normalSum;
                            mc.count += info.count;
                            merged = true;
                            break;
                        }
                    }
                    if (!merged) {
                        mergedClusters.push_back(info);
                    }
                }
                
                int subCount = 0;
                for (auto& mc : mergedClusters) {
                    Math::Vector3 localCenter = mc.centerSum / (float)mc.count;
                    Math::Vector3 localNormal = mc.normalSum;
                    localNormal.Normalize();
                    
                    Math::Vector3 worldCenter = Math::Vector3::Transform(localCenter, node.originalGlobalTransform);
                    Math::Vector3 worldNormal = Math::Vector3::TransformNormal(localNormal, node.originalGlobalTransform);
                    worldNormal.Normalize();
                    
                    std::string objName = "AutoMirror_" + node.name + "_" + matName + "_" + std::to_string(subCount);
                    auto mirrorObj = Editor::GetScene()->CreateGameObject(objName);
                    
                    auto& trans = mirrorObj->GetComponent<TransformData>();
                    trans.m_position = worldCenter;
                    
                    mirrorObj->AddComponent<NativeScriptData>(NativeScriptData{});
                    auto& ecsScript = mirrorObj->GetComponent<NativeScriptData>();
                    auto refComp = std::make_shared<ReflectionComponent>();
                    refComp->SetGameObject(mirrorObj.get());
                    
                    refComp->m_planeNormal = worldNormal;
                    refComp->m_planePoint = Math::Vector3(0, 0, 0);

                    ecsScript.Instance = refComp;
                    
                    mirrorCount++;
                    subCount++;
                }
            }
        }
    }
    
    if (mirrorCount > 0) {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "AutoMirrorComponent: %d mirrors generated from model.", mirrorCount);
    }
}

void AutoMirrorComponent::ImGuiUpdate()
{
    ImGui::Text("Auto Mirror Manager");
    ImGui::Text("Generates ReflectionComponent on button click");
    
    if (ImGui::Button("Generate Mirrors Now!")) {
        GenerateMirrors();
    }
    ImGui::Separator();
    
    char buffer[256];
    strncpy_s(buffer, m_targetKeywords.c_str(), sizeof(buffer));
    if (ImGui::InputText("Target Keywords", buffer, sizeof(buffer))) {
        m_targetKeywords = buffer;
    }
    ImGui::DragFloat3("Local Normal", &m_localNormal.x, 0.01f, -1.0f, 1.0f);
    if (ImGui::Button("Normalize")) {
        m_localNormal.Normalize();
    }
}

void AutoMirrorComponent::Serialize(nlohmann::json& out) const
{
    out["targetKeywords"] = m_targetKeywords;
    out["localNormal"] = { m_localNormal.x, m_localNormal.y, m_localNormal.z };
}

void AutoMirrorComponent::Deserialize(const nlohmann::json& in)
{
    if (in.contains("targetKeywords")) {
        m_targetKeywords = in["targetKeywords"];
    }
    if (in.contains("localNormal")) {
        auto arr = in["localNormal"];
        m_localNormal = { arr[0], arr[1], arr[2] };
    }
}
