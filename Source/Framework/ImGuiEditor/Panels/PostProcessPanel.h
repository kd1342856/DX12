#pragma once
#include "../IEditorPanel.h"

class PostProcessPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
