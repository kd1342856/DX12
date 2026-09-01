#pragma once
#include "../IEditorPanel.h"

class MaterialPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
