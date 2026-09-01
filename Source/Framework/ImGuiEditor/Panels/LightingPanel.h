#pragma once
#include "../IEditorPanel.h"

class LightingPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
