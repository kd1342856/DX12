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

    static std::shared_ptr<Scene> GetScene() { return s_scene; }

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

    static std::shared_ptr<Scene> s_scene;
};
