#pragma once

class Scene;
class GameObject;

class Editor {
public:
    static void DrawHierarchyAndInspector(Scene* scene);
    static void DrawAssetEditor();
private:
    static void DrawHierarchyNode(std::shared_ptr<GameObject> obj);
    static std::shared_ptr<GameObject> s_selectedObject;
    static std::string s_selectedAssetPath;
    static std::string s_currentAssetDir;
    static int s_selectedModelType;
};