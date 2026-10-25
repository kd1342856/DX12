#pragma once
#include <cstdint>

class Scene;
class GameObject;
class RenderTarget;

class Editor
{
public:
    static void Init();
    static void Draw();
    static bool GetEditorMode()
    {
        return s_editorMode;
    }

    // F3で切り替えるStatistics/Profilerウィンドウ。フルエディタUI(エディタモードでしか
    // 描かれない - RenderEditor()参照)とは独立している。フルスクリーン/プレイヤー視点の
    // レンダーパスから呼ぶことで、エディタモードに入らなくてもProfilerを見られるようにする。
    static void DrawProfilerOverlay();

    // s_showEditor is the flag F1 toggles in Draw(); it's a global that survives scene changes, so
    // scenes that want the debug windows hidden by default on entry (e.g. ResultScene) need to be
    // able to force it off explicitly rather than just hoping it happened to be off already.
    static void SetShowEditor(bool show)
    {
        s_showEditor = show;
    }

    static std::shared_ptr<Scene> GetScene()
    {
        return s_scene;
    }

    // 現在のヒエラルキー(s_scene)の保存先JSON。シーンごとに異なるファイルを持たせ、
    // 別シーンのヒエラルキーを別シーンのファイルに上書き保存しないようにする。
    // Init()で空に戻るので、各シーンのInit()でEditor::Init()の後に設定すること。
    // 空の場合は保存不可(File > Save Sceneは何もしない)。
    // 設定時に、同じ場所のエディタ専用JSON(XXX.editor.json: ヒエラルキーのフォルダ構成)も読み込む。
    static void SetScenePath(const std::string& path);
    static const std::string& GetScenePath()
    {
        return s_scenePath;
    }

    // 「Preview Camera」ウィンドウに今マウスが乗っているか。GameSceneの右クリックドラッグでの
    // カメラ操作(通常のエディタ自由カメラと同じ操作感)を、どちらのウィンドウを操作しているかで
    // 出し分けるために使う。PreviewCameraWindow.cppがDrawPreviewCamera()内で毎フレーム更新する。
    static bool IsPreviewCameraHovered()
    {
        return s_previewCameraHovered;
    }

    // シーン切り替え時に呼ぶ。選択中オブジェクトを持ったままシーンを切り替えると、
    // 古いシーンのGameObjectがshared_ptrで生き延びてしまい(ECSからは既に消えているのに
    // C++オブジェクトだけ残る)、Inspector/AssetBrowserがそれを参照して
    // 「存在しないEntity」でクラッシュする原因になる。
    static void ClearSelection()
    {
        s_selectedObject = nullptr;
    }

    // ヒエラルキーのフォルダ構成(表示専用。GameObjectの親子関係には影響しない)。
    // シーンJSONとは別の「XXX.editor.json」に保存し、Save Sceneと一緒に書き出す。
    static void LoadEditorData();
    static void SaveEditorData();

private:
    // Core
    static void DrawDockSpace();
    static void DrawMenuBar();
    static void DrawToolbar();

    // Windows
    static void DrawHierarchy();
    static void DrawInspector();
    static void DrawAssetBrowser();
    static void DrawStatistics();
    static void DrawConsole();
    static void DrawPreviewCamera();
    static void DrawSoundManager();

    // Helper
    static void DrawHierarchyNode(std::shared_ptr<GameObject> obj);
    static void DrawHierarchyFolder(uint32_t folderId, const std::vector<std::shared_ptr<GameObject>>& roots);

private:
    static std::shared_ptr<GameObject> s_selectedObject;
    static std::string s_selectedAssetPath;
    static std::string s_currentAssetDir;

    static bool s_editorMode;
    static bool s_showEditor;
    static bool s_showProfilerOverlay;

    static std::shared_ptr<Scene> s_scene;
    static std::string s_scenePath;

    static bool s_showGameEditor;
    static bool s_showShaderEditor;
    static bool s_showNavMeshEditor;
    static bool s_showPreviewCamera;
    static bool s_showSoundManager;
    static bool s_previewCameraHovered;
};
