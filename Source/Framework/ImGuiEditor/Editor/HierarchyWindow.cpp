#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <functional>

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr()
{
    return Editor::GetScene();
}

// =============================================
// ヒエラルキーの表示フィルター
// 表示を絞り込むだけで、親子関係やシーンの中身には一切触れない。
// フィルター中は「条件に一致したオブジェクト」と「その祖先(ツリーを辿るため)」だけを描画し、
// 祖先側は文字を薄くして一致したものと区別する。
// =============================================
namespace HierarchyFilter
{
    enum Category : uint32_t {
        Cat_Model      = 1 << 0,
        Cat_Sprite     = 1 << 1,
        Cat_Camera     = 1 << 2,
        Cat_Collider   = 1 << 3,
        Cat_Animation  = 1 << 4,
        Cat_PointLight = 1 << 5,
        Cat_Script     = 1 << 6,
        Cat_Empty      = 1 << 7, // Transform以外何も持っていない(グループ用の空オブジェクト等)
    };

    struct CategoryInfo { Category cat; const char* label; };
    static const CategoryInfo kCategories[] = {
        { Cat_Model,      "Model"       },
        { Cat_Sprite,     "Sprite"      },
        { Cat_Camera,     "Camera"      },
        { Cat_Collider,   "Collider"    },
        { Cat_Animation,  "Animation"   },
        { Cat_PointLight, "Point Light" },
        { Cat_Script,     "Script (any)"},
        { Cat_Empty,      "Empty"       },
    };

    static char                  s_search[128] = "";
    static uint32_t              s_categoryMask = 0;
    static std::set<std::string> s_scripts;   // 選択中のスクリプト名
    static std::set<std::string> s_tags;      // 選択中のタグ
    static bool                  s_forceOpen = false; // 条件が変わった直後のフレームだけ祖先を展開する

    // 毎フレーム作り直す、描画対象の判定結果
    static std::unordered_set<GameObject*> s_matched;
    static std::unordered_set<GameObject*> s_visible;
    static int s_matchCount = 0;

    static bool IsActive() {
        return s_search[0] != '\0' || s_categoryMask != 0 || !s_scripts.empty() || !s_tags.empty();
    }

    static void Clear() {
        s_search[0] = '\0';
        s_categoryMask = 0;
        s_scripts.clear();
        s_tags.clear();
    }

    static uint32_t GetCategories(GameObject* obj, std::string* outScriptName) {
        auto& ecs = GameManager::Instance().GetECS();
        Entity e = obj->GetEntityID();
        uint32_t mask = 0;
        if (e == INVALID_ENTITY) return mask;
        if (ecs.TryGetComponent<ModelRenderData>(e))        mask |= Cat_Model;
        if (ecs.TryGetComponent<SpriteData>(e))             mask |= Cat_Sprite;
        if (ecs.TryGetComponent<CameraData>(e))             mask |= Cat_Camera;
        if (ecs.TryGetComponent<ColliderData>(e))           mask |= Cat_Collider;
        if (ecs.TryGetComponent<AnimationDataComponent>(e)) mask |= Cat_Animation;
        if (ecs.TryGetComponent<PointLightData>(e))         mask |= Cat_PointLight;
        if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(e)) {
            mask |= Cat_Script;
            if (outScriptName) *outScriptName = pScript->ScriptName;
        }
        if (mask == 0) mask |= Cat_Empty;
        return mask;
    }

    static bool ContainsIgnoreCase(const std::string& text, const char* pattern) {
        std::string a = text, b = pattern;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return a.find(b) != std::string::npos;
    }

    static bool Matches(GameObject* obj) {
        if (s_search[0] != '\0' && !ContainsIgnoreCase(obj->GetName(), s_search)) return false;
        if (!s_tags.empty() && s_tags.count(obj->GetTag()) == 0) return false;

        // 種類(コンポーネント)とスクリプト名はOR条件: どれか1つでも持っていれば一致
        if (s_categoryMask != 0 || !s_scripts.empty()) {
            std::string scriptName;
            uint32_t mask = GetCategories(obj, &scriptName);
            bool typeHit   = (mask & s_categoryMask) != 0;
            bool scriptHit = !scriptName.empty() && s_scripts.count(scriptName) != 0;
            if (!typeHit && !scriptHit) return false;
        }
        return true;
    }

    // 子孫に一致があれば自分も(祖先として)表示対象にする
    static bool Evaluate(const std::shared_ptr<GameObject>& obj) {
        bool anyChild = false;
        for (auto& child : obj->GetChildren()) {
            if (Evaluate(child)) anyChild = true;
        }
        bool self = Matches(obj.get());
        if (self) { s_matched.insert(obj.get()); ++s_matchCount; }
        if (self || anyChild) { s_visible.insert(obj.get()); return true; }
        return false;
    }

    // シーン内に存在するスクリプト名・タグを集める(フィルターメニューの選択肢用)
    static void Collect(const std::shared_ptr<GameObject>& obj, std::set<std::string>& scripts, std::set<std::string>& tags) {
        std::string scriptName;
        GetCategories(obj.get(), &scriptName);
        if (!scriptName.empty()) scripts.insert(scriptName);
        tags.insert(obj->GetTag());
        for (auto& child : obj->GetChildren()) Collect(child, scripts, tags);
    }

    static void DrawToolbar(Scene* scene) {
        bool changed = false;

        float buttonW = ImGui::CalcTextSize("Filter (00)").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(-(buttonW + ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::InputTextWithHint("##HierarchySearch", "Search...", s_search, sizeof(s_search))) changed = true;

        ImGui::SameLine();
        int activeCount = 0;
        for (auto& c : kCategories) if (s_categoryMask & c.cat) ++activeCount;
        activeCount += (int)s_scripts.size() + (int)s_tags.size();

        if (activeCount > 0) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        std::string label = activeCount > 0 ? ("Filter (" + std::to_string(activeCount) + ")") : std::string("Filter");
        if (ImGui::Button((label + "##HierarchyFilterBtn").c_str(), ImVec2(buttonW, 0))) {
            ImGui::OpenPopup("HierarchyFilterPopup");
        }
        if (activeCount > 0) ImGui::PopStyleColor();

        if (ImGui::BeginPopup("HierarchyFilterPopup")) {
            ImGui::TextDisabled(u8"種類 (いずれかを持つ)");
            for (auto& c : kCategories) {
                bool on = (s_categoryMask & c.cat) != 0;
                if (ImGui::Checkbox(c.label, &on)) {
                    if (on) s_categoryMask |= c.cat; else s_categoryMask &= ~c.cat;
                    changed = true;
                }
            }

            std::set<std::string> scripts, tags;
            for (auto& obj : scene->GetGameObjects()) Collect(obj, scripts, tags);

            if (!scripts.empty() && ImGui::BeginMenu("Scripts")) {
                for (auto& name : scripts) {
                    bool on = s_scripts.count(name) != 0;
                    if (ImGui::Checkbox(name.c_str(), &on)) {
                        if (on) s_scripts.insert(name); else s_scripts.erase(name);
                        changed = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (!tags.empty() && ImGui::BeginMenu("Tags")) {
                for (auto& tag : tags) {
                    bool on = s_tags.count(tag) != 0;
                    if (ImGui::Checkbox(tag.c_str(), &on)) {
                        if (on) s_tags.insert(tag); else s_tags.erase(tag);
                        changed = true;
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Clear All")) { Clear(); changed = true; }
            ImGui::EndPopup();
        }

        if (changed) s_forceOpen = true;
    }
}

// =============================================
// ヒエラルキーのフォルダ
// エディタ上の見た目だけの整理用。GameObjectの親子関係・ECSには一切触れず、
// 「ルートGameObjectのUUID → フォルダID」の対応だけを持つ。
// 親を持つ(子)オブジェクトはフォルダに入らず、今まで通り親の下に表示される。
// 保存先はシーンJSONの隣の「XXX.editor.json」。
// =============================================
namespace HierarchyFolders
{
    struct Folder {
        uint32_t    id = 0;
        std::string name;
        uint32_t    parent = 0; // 0 = ルート直下
        bool        open = true;
    };

    static std::vector<Folder>                    s_folders;
    static std::unordered_map<uint64_t, uint32_t> s_objectFolder; // ルートGameObjectのUUID -> フォルダID
    static uint32_t                               s_nextId = 1;

    static uint32_t s_renamingId = 0;
    static char     s_renameBuf[128] = "";
    static bool     s_focusRename = false;

    // フィルター中、表示対象のオブジェクトを(子孫フォルダ含め)中に持つフォルダ
    static std::unordered_set<uint32_t> s_visibleFolders;

    static Folder* Find(uint32_t id) {
        if (id == 0) return nullptr;
        for (auto& f : s_folders) if (f.id == id) return &f;
        return nullptr;
    }

    static uint32_t FolderOf(GameObject* obj) {
        if (obj->GetParent()) return 0;
        auto it = s_objectFolder.find(obj->GetUUID());
        if (it == s_objectFolder.end() || !Find(it->second)) return 0;
        return it->second;
    }

    static void SetFolder(GameObject* obj, uint32_t id) {
        if (id == 0) s_objectFolder.erase(obj->GetUUID());
        else         s_objectFolder[obj->GetUUID()] = id;
    }

    // idがancestor自身、またはその子孫フォルダか
    static bool IsInside(uint32_t id, uint32_t ancestor) {
        for (Folder* f = Find(id); f; f = Find(f->parent)) {
            if (f->id == ancestor) return true;
        }
        return false;
    }

    static std::vector<uint32_t> ChildFolders(uint32_t parent) {
        std::vector<uint32_t> ids;
        for (auto& f : s_folders) if (f.parent == parent) ids.push_back(f.id);
        return ids;
    }

    static void BeginRename(uint32_t id) {
        Folder* f = Find(id);
        if (!f) return;
        s_renamingId = id;
        strncpy_s(s_renameBuf, f->name.c_str(), _TRUNCATE);
        s_focusRename = true;
    }

    // 作成直後はそのまま名前入力モードにする
    static uint32_t Create(uint32_t parent) {
        Folder f;
        f.id = s_nextId++;
        f.name = "New Folder";
        f.parent = parent;
        s_folders.push_back(f);
        if (Folder* p = Find(parent)) p->open = true;
        BeginRename(f.id);
        return f.id;
    }

    // フォルダだけを消し、中身(子フォルダ・オブジェクト)はひとつ上の階層へ移す
    static void Remove(uint32_t id) {
        Folder* f = Find(id);
        if (!f) return;
        const uint32_t up = f->parent;
        for (auto& other : s_folders) if (other.parent == id) other.parent = up;
        for (auto it = s_objectFolder.begin(); it != s_objectFolder.end(); ) {
            if (it->second == id) {
                if (up == 0) { it = s_objectFolder.erase(it); continue; }
                it->second = up;
            }
            ++it;
        }
        s_folders.erase(std::remove_if(s_folders.begin(), s_folders.end(), [id](const Folder& x) { return x.id == id; }), s_folders.end());
        if (s_renamingId == id) s_renamingId = 0;
    }

    static std::string FullPath(uint32_t id) {
        std::string path;
        for (Folder* f = Find(id); f; f = Find(f->parent)) {
            path = path.empty() ? f->name : f->name + "/" + path;
        }
        return path;
    }

    static std::string EditorDataPath(const std::string& scenePath) {
        const std::string ext = ".json";
        if (scenePath.size() > ext.size() && scenePath.compare(scenePath.size() - ext.size(), ext.size(), ext) == 0) {
            return scenePath.substr(0, scenePath.size() - ext.size()) + ".editor.json";
        }
        return scenePath + ".editor.json";
    }

    // オブジェクトをフォルダへ移す。子オブジェクトだった場合はルートに戻してから入れる
    static void MoveObject(GameObject* raw, uint32_t folderId) {
        auto obj = raw->shared_from_this();
        if (obj->GetParent()) obj->SetParent(nullptr);
        SetFolder(obj.get(), folderId);
    }

    // フォルダ移動用のメニュー(オブジェクトの右クリックメニューから使う)
    static void DrawMoveMenuItems(GameObject* obj, uint32_t parent) {
        for (uint32_t id : ChildFolders(parent)) {
            Folder* f = Find(id);
            if (!f) continue;
            ImGui::PushID((int)id);
            bool current = FolderOf(obj) == id;
            if (ImGui::MenuItem(f->name.c_str(), nullptr, current)) MoveObject(obj, id);
            ImGui::Indent();
            DrawMoveMenuItems(obj, id);
            ImGui::Unindent();
            ImGui::PopID();
        }
    }
}

void Editor::SetScenePath(const std::string& path) {
    s_scenePath = path;
    LoadEditorData();
}

void Editor::LoadEditorData() {
    using namespace HierarchyFolders;
    s_folders.clear();
    s_objectFolder.clear();
    s_nextId = 1;
    s_renamingId = 0;

    if (s_scenePath.empty()) return;
    std::ifstream in(EditorDataPath(s_scenePath));
    if (!in.is_open()) return; // まだ一度も保存していないシーン

    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, "Editor data parse failed: %s", EditorDataPath(s_scenePath).c_str());
        return;
    }

    if (j.contains("hierarchyFolders")) {
        for (auto& jf : j["hierarchyFolders"]) {
            Folder f;
            f.id     = jf.value("id", 0u);
            f.name   = jf.value("name", std::string("Folder"));
            f.parent = jf.value("parent", 0u);
            f.open   = jf.value("open", true);
            if (f.id == 0) continue;
            s_folders.push_back(f);
            s_nextId = (std::max)(s_nextId, f.id + 1);
        }
        // 壊れた参照(存在しない親・循環)はルートに戻す
        for (auto& f : s_folders) {
            if (f.parent != 0 && (!Find(f.parent) || IsInside(f.parent, f.id))) f.parent = 0;
        }
    }
    if (j.contains("hierarchyObjects")) {
        for (auto& jo : j["hierarchyObjects"]) {
            uint64_t uuid = jo.value("uuid", (uint64_t)0);
            uint32_t id   = jo.value("folder", 0u);
            if (uuid != 0 && Find(id)) s_objectFolder[uuid] = id;
        }
    }
}

void Editor::SaveEditorData() {
    using namespace HierarchyFolders;
    if (s_scenePath.empty()) return;

    // 今のシーンのルートに存在しないオブジェクトの対応は書き出さない(削除済みなど)
    std::unordered_set<uint64_t> alive;
    if (s_scene) for (auto& obj : s_scene->GetGameObjects()) alive.insert(obj->GetUUID());

    nlohmann::json j;
    j["hierarchyFolders"] = nlohmann::json::array();
    for (auto& f : s_folders) {
        j["hierarchyFolders"].push_back({ {"id", f.id}, {"name", f.name}, {"parent", f.parent}, {"open", f.open} });
    }
    j["hierarchyObjects"] = nlohmann::json::array();
    for (auto& [uuid, id] : s_objectFolder) {
        if (alive.count(uuid) && Find(id)) j["hierarchyObjects"].push_back({ {"uuid", uuid}, {"folder", id} });
    }

    const std::string path = EditorDataPath(s_scenePath);
    std::ofstream o(path);
    if (o.is_open()) {
        o << std::setw(4) << j << std::endl;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Editor Data Saved: %s", path.c_str());
    }
}

void Editor::DrawHierarchyFolder(uint32_t folderId, const std::vector<std::shared_ptr<GameObject>>& roots) {
    using namespace HierarchyFolders;
    const bool filtering = HierarchyFilter::IsActive();
    if (filtering && s_visibleFolders.count(folderId) == 0) return;
    if (!Find(folderId)) return;

    ImGui::PushID("HierarchyFolder");
    ImGui::PushID((int)folderId);

    bool opened = false;
    if (s_renamingId == folderId) {
        // 名前変更中: ツリーノードの代わりに入力欄を出す(Enter/フォーカスを外すと確定)
        if (s_focusRename) { ImGui::SetKeyboardFocusHere(); s_focusRename = false; }
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool enter = ImGui::InputText("##Rename", s_renameBuf, sizeof(s_renameBuf),
                                      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (enter || ImGui::IsItemDeactivated()) {
            if (Folder* f = Find(folderId); f && s_renameBuf[0] != '\0') f->name = s_renameBuf;
            s_renamingId = 0;
        }
        opened = Find(folderId)->open;
        if (opened) ImGui::TreePush("##RenameChildren");
    } else {
        Folder* f = Find(folderId);
        if (filtering && HierarchyFilter::s_forceOpen) ImGui::SetNextItemOpen(true);
        else                                           ImGui::SetNextItemOpen(f->open);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.40f, 1.0f));
        opened = ImGui::TreeNodeEx("##Folder", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick,
                                   "[ %s ]", f->name.c_str());
        ImGui::PopStyleColor();
        f->open = opened;

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create Subfolder")) Create(folderId);
            if (ImGui::MenuItem("Create Empty Object Here")) {
                auto obj = s_scene->CreateGameObject("GameObject");
                SetFolder(obj.get(), folderId);
                if (Folder* ff = Find(folderId)) ff->open = true;
            }
            if (ImGui::MenuItem("Rename")) BeginRename(folderId);
            ImGui::Separator();
            if (ImGui::MenuItem(u8"Delete Folder (中身はひとつ上へ)")) Remove(folderId);
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("HIERARCHY_FOLDER", &folderId, sizeof(uint32_t));
            if (Folder* ff = Find(folderId)) ImGui::Text("[ %s ]", ff->name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
                MoveObject(*(GameObject**)payload->Data, folderId);
                if (Folder* ff = Find(folderId)) ff->open = true;
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_FOLDER")) {
                uint32_t dropped = *(const uint32_t*)payload->Data;
                // 自分自身や自分の子孫フォルダの中には入れられない
                if (Folder* df = Find(dropped); df && !IsInside(folderId, dropped)) {
                    df->parent = folderId;
                    if (Folder* ff = Find(folderId)) ff->open = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    // メニュー操作で自分が消えた場合
    if (!Find(folderId)) {
        if (opened) ImGui::TreePop();
        ImGui::PopID();
        ImGui::PopID();
        return;
    }

    if (opened) {
        for (uint32_t childId : ChildFolders(folderId)) {
            DrawHierarchyFolder(childId, roots);
        }
        for (auto& obj : roots) {
            if (FolderOf(obj.get()) != folderId) continue;
            if (filtering && HierarchyFilter::s_visible.count(obj.get()) == 0) continue;
            DrawHierarchyNode(obj);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
    ImGui::PopID();
}


// シリアライズ済みのGameObject(JSON)からシーンにオブジェクトを生成する(prefab配置・ペースト共通)。
// 同じJSONから何度生成してもUUIDが重複しないよう、UUIDは取り除いて新規発行させる。
// parentを渡すとその子として生成する。シーン読み込み時と同じく子階層も含めてAwake/Startを呼ぶ。
static std::shared_ptr<GameObject> SpawnFromJson(Scene* scene, nlohmann::json j, const std::shared_ptr<GameObject>& parent)
{
    std::function<void(nlohmann::json&)> stripUUID = [&](nlohmann::json& o) {
        o.erase("UUID");
        if (o.contains("Children")) {
            for (auto& child : o["Children"]) stripUUID(child);
        }
    };
    stripUUID(j);

    std::shared_ptr<GameObject> obj;
    if (parent) {
        size_t prevSize = parent->GetChildren().size();
        scene->DeserializeGameObject(j, parent);
        if (parent->GetChildren().size() <= prevSize) return nullptr;
        obj = parent->GetChildren().back();
    } else {
        size_t prevSize = scene->GetGameObjects().size();
        scene->DeserializeGameObject(j, nullptr);
        if (scene->GetGameObjects().size() <= prevSize) return nullptr;
        obj = scene->GetGameObjects().back();
    }

    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(const std::shared_ptr<GameObject>&, bool)> callScripts = [&](const std::shared_ptr<GameObject>& node, bool start) {
        if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(node->GetEntityID())) {
            if (pScript->Instance) {
                if (start) pScript->Instance->Start();
                else pScript->Instance->Awake();
            }
        }
        for (auto& child : node->GetChildren()) callScripts(child, start);
    };
    callScripts(obj, false);
    callScripts(obj, true);
    return obj;
}

// =============================================
// ヒエラルキーのコピー&ペースト(Ctrl+C / Ctrl+V)
// コピー時点のJSONを丸ごと保持するので、元のオブジェクトを消した後や別シーンに切り替えた後でも
// ペーストできる。子オブジェクトも含めて複製する。
// =============================================
namespace HierarchyClipboard
{
    static nlohmann::json s_data;
    static std::string    s_name;
    static bool           s_hasData = false;

    static void Copy(const std::shared_ptr<GameObject>& obj) {
        if (!obj || obj->IsPendingDestroy() || !obj->GetScene()) return;
        s_data = obj->GetScene()->SerializeGameObject(obj);
        s_name = obj->GetName();
        s_hasData = true;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Copied: %s", s_name.c_str());
    }

    // target(右クリックしたオブジェクト/選択中のオブジェクト)と同じ階層・同じフォルダにペーストする。
    // targetが無ければルートに置く。
    static std::shared_ptr<GameObject> Paste(Scene* scene, const std::shared_ptr<GameObject>& target) {
        if (!s_hasData || !scene) return nullptr;
        std::shared_ptr<GameObject> sibling = (target && target->GetScene() == scene && !target->IsPendingDestroy()) ? target : nullptr;

        std::shared_ptr<GameObject> parent;
        if (sibling && sibling->GetParent()) parent = sibling->GetParent()->shared_from_this();

        auto obj = SpawnFromJson(scene, s_data, parent);
        if (!obj) return nullptr;
        if (sibling && !parent) {
            HierarchyFolders::SetFolder(obj.get(), HierarchyFolders::FolderOf(sibling.get()));
        }
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Pasted: %s", s_name.c_str());
        return obj;
    }
}

// Asset/Data/Prefab/内のprefab一覧をメニューに出し、選ばれたものをシーンに追加する(エディタでの配置用)。
// Scene::Instantiate()は実行時生成用でDynamic扱い(シーン保存の対象外)になるため使わず、通常の
// オブジェクトとして追加する。同じprefabを何度置いてもUUIDが重複しないよう、UUIDは取り除いて新規発行させる。
static std::shared_ptr<GameObject> DrawCreateFromPrefabMenu(Scene* scene)
{
    namespace fs = std::filesystem;
    const fs::path dir = "Asset/Data/Prefab";
    std::error_code ec;
    std::vector<fs::path> files;
    if (fs::exists(dir, ec)) {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        ImGui::TextDisabled(u8"(Asset/Data/Prefab/ に prefab がありません)");
        return nullptr;
    }

    for (const auto& path : files) {
        if (!ImGui::MenuItem(path.stem().string().c_str())) continue;

        nlohmann::json j;
        try {
            std::ifstream in(path);
            in >> j;
        } catch (const std::exception& e) {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Prefab load failed: %s (%s)", path.string().c_str(), e.what());
            return nullptr;
        }
        if (j.is_array()) {
            if (j.empty()) return nullptr;
            j = j[0];
        }

        auto obj = SpawnFromJson(scene, j, nullptr);
        if (!obj) return nullptr;

        Logger::Instance().AddLog(Logger::LogLevel::Info, "Prefab added: %s", path.string().c_str());
        return obj;
    }
    return nullptr;
}

void Editor::DrawHierarchy() {
    auto sceneObj = GetCurrentScenePtr();
    Scene* scene = sceneObj.get();
    if (!scene) return;
    ImGui::Begin("Hierarchy");

    // Ctrl+C / Ctrl+V: Hierarchyウィンドウにフォーカスがある時だけ(検索欄やフォルダ名の入力中は除く)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) {
            HierarchyClipboard::Copy(s_selectedObject);
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
            if (auto obj = HierarchyClipboard::Paste(scene, s_selectedObject)) s_selectedObject = obj;
        }
    }

    // 背景を右クリックした時のポップアップメニュー
    if (ImGui::BeginPopupContextWindow("HierarchyPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Empty Object")) {
            scene->CreateGameObject("GameObject");
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, HierarchyClipboard::s_hasData)) {
            if (auto obj = HierarchyClipboard::Paste(scene, nullptr)) s_selectedObject = obj;
        }
        if (ImGui::BeginMenu("Create From Prefab")) {
            if (auto obj = DrawCreateFromPrefabMenu(scene)) {
                s_selectedObject = obj;
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Create Folder")) {
            HierarchyFolders::Create(0);
        }
        ImGui::EndPopup();
    }

    HierarchyFilter::DrawToolbar(scene);

    auto rootsCopy = scene->GetGameObjects();

    HierarchyFilter::s_matched.clear();
    HierarchyFilter::s_visible.clear();
    HierarchyFilter::s_matchCount = 0;
    const bool filtering = HierarchyFilter::IsActive();
    if (filtering) {
        for (auto& obj : rootsCopy) HierarchyFilter::Evaluate(obj);
        ImGui::TextDisabled("%d match(es)", HierarchyFilter::s_matchCount);
    }
    ImGui::Separator();

    // フィルター中は、表示対象を含むフォルダ(とその祖先フォルダ)だけを出す
    HierarchyFolders::s_visibleFolders.clear();
    if (filtering) {
        for (auto& obj : rootsCopy) {
            if (HierarchyFilter::s_visible.count(obj.get()) == 0) continue;
            for (auto* f = HierarchyFolders::Find(HierarchyFolders::FolderOf(obj.get())); f; f = HierarchyFolders::Find(f->parent)) {
                HierarchyFolders::s_visibleFolders.insert(f->id);
            }
        }
    }

    // フォルダ → フォルダに入っていないルートオブジェクト の順に描画
    for (uint32_t id : HierarchyFolders::ChildFolders(0)) {
        DrawHierarchyFolder(id, rootsCopy);
    }
    for (auto& obj : rootsCopy) {
        if (HierarchyFolders::FolderOf(obj.get()) != 0) continue;
        if (filtering && HierarchyFilter::s_visible.count(obj.get()) == 0) continue;
        DrawHierarchyNode(obj);
    }
    HierarchyFilter::s_forceOpen = false;

    // 空き領域にドロップしたら親子関係を解除してルートに戻す(フォルダからも出す)
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
            HierarchyFolders::MoveObject(*(GameObject**)payload->Data, 0);
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_FOLDER")) {
            if (auto* f = HierarchyFolders::Find(*(const uint32_t*)payload->Data)) f->parent = 0;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

}

void Editor::DrawHierarchyNode(std::shared_ptr<GameObject> obj) {
    ImGuiTreeNodeFlags flags = ((s_selectedObject == obj) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    
    const bool filtering = HierarchyFilter::IsActive();

    // フィルター中は表示対象の子がいなければ葉として描く(開いても空、を避ける)
    bool hasChildren = false;
    for (auto& child : obj->GetChildren()) {
        if (!filtering || HierarchyFilter::s_visible.count(child.get()) != 0) { hasChildren = true; break; }
    }
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    // フィルター中に「一致した子孫を持つだけの祖先」は薄く表示する
    const bool dimmed = filtering && HierarchyFilter::s_matched.count(obj.get()) == 0;
    if (filtering && hasChildren && HierarchyFilter::s_forceOpen) {
        ImGui::SetNextItemOpen(true);
    }

    ImGui::PushID(obj.get());
    if (dimmed) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    bool opened = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s", obj->GetName().c_str());
    if (dimmed) ImGui::PopStyleColor();
    
    // Shiftを押しながらのクリックでは選択を切り替えない - ドラッグ&ドロップでカメラ等を
    // どこかのInspector上のドロップターゲット(例: ReflectionComponentのFixed Camera欄)に
    // 割り当てたい時、クリックした瞬間にInspectorがドラッグ元のオブジェクトへ切り替わって
    // しまうと、ドロップ先のUIが消えてドラッグを完了できないため。
    if (ImGui::IsItemClicked() && !ImGui::GetIO().KeyShift) {
        s_selectedObject = obj;
    }

        if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Empty Child")) {
            auto child = obj->GetScene()->CreateGameObject("GameObject");
            child->SetParent(obj);
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            HierarchyClipboard::Copy(obj);
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, HierarchyClipboard::s_hasData)) {
            if (auto pasted = HierarchyClipboard::Paste(obj->GetScene(), obj)) s_selectedObject = pasted;
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
                HierarchyFolders::SetFolder(obj.get(), 0);
            }
        }
        if (!HierarchyFolders::s_folders.empty() && ImGui::BeginMenu("Move to Folder")) {
            if (ImGui::MenuItem("(None)", nullptr, HierarchyFolders::FolderOf(obj.get()) == 0 && !obj->GetParent())) {
                HierarchyFolders::MoveObject(obj.get(), 0);
            }
            ImGui::Separator();
            HierarchyFolders::DrawMoveMenuItems(obj.get(), 0);
            ImGui::EndMenu();
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
                if (!isChild) {
                    droppedObj->SetParent(obj);
                    // 子になった時点でフォルダからは外す(後でUnparentした時に古いフォルダへ戻らないように)
                    HierarchyFolders::SetFolder(droppedObj.get(), 0);
                }
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
                if (filtering && HierarchyFilter::s_visible.count(child.get()) == 0) continue;
                DrawHierarchyNode(child);
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}
