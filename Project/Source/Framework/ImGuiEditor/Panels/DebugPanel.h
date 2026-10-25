#pragma once
#include "../IEditorPanel.h"

class DebugPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
