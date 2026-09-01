#pragma once
#include <vector>
#include <memory>
#include "../IEditorPanel.h"
#include "../EditorContext.h"

class GameEditor
{
public:
    void Initialize();
    void Draw(EditorContext& ctx);
private:
    std::vector<std::unique_ptr<IEditorPanel>> m_Panels;
};
