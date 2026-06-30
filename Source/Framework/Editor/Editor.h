#pragma once
#include <cstdint>

class Scene;
class GameObject;
class RenderTarget; // ëOï˚êÈåæ

class Editor {
public:
    static bool GetEditorMode() { return s_editorMode; }
    static void DrawHierarchyAndInspector(Scene* scene);
    static void DrawAssetEditor();
    static void DrawGameView(RenderTarget* pRenderTarget, uint32_t cameraEntity, bool fullscreen);
private:
    static void DrawHierarchyNode(std::shared_ptr<GameObject> obj);
    static std::shared_ptr<GameObject> s_selectedObject;
    static std::string s_selectedAssetPath;
    static std::string s_currentAssetDir;
    static int s_selectedModelType;
    static bool s_editorMode;
};
