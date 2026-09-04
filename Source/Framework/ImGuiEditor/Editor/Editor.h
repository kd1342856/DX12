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
    static bool GetEditorMode() { return s_editorMode; }

    // F3で切り替えるStatistics/Profilerウィンドウ。フルエディタUI(エディタモードでしか
    // 描かれない - RenderEditor()参照)とは独立している。フルスクリーン/プレイヤー視点の
    // レンダーパスから呼ぶことで、エディタモードに入らなくてもProfilerを見られるようにする。
    static void DrawProfilerOverlay();

    // s_showEditor is the flag F1 toggles in Draw(); it's a global that survives scene changes, so
    // scenes that want the debug windows hidden by default on entry (e.g. ResultScene) need to be
    // able to force it off explicitly rather than just hoping it happened to be off already.
    static void SetShowEditor(bool show) { s_showEditor = show; }

    static std::shared_ptr<Scene> GetScene() { return s_scene; }

    // シーン切り替え時に呼ぶ。選択中オブジェクトを持ったままシーンを切り替えると、
    // 古いシーンのGameObjectがshared_ptrで生き延びてしまい(ECSからは既に消えているのに
    // C++オブジェクトだけ残る)、Inspector/AssetBrowserがそれを参照して
    // 「存在しないEntity」でクラッシュする原因になる。
    static void ClearSelection() { s_selectedObject = nullptr; }

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

    // Helper
    static void DrawHierarchyNode(std::shared_ptr<GameObject> obj);

private:
    static std::shared_ptr<GameObject> s_selectedObject;
    static std::string s_selectedAssetPath;
    static std::string s_currentAssetDir;

    static bool s_editorMode;
    static bool s_showEditor;
    static bool s_showProfilerOverlay;

    static std::shared_ptr<Scene> s_scene;

    static bool s_showGameEditor;
    static bool s_showShaderEditor;
    static bool s_showNavMeshEditor;
};
