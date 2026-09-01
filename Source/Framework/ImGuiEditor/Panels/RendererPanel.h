#pragma once
#include "../IEditorPanel.h"

class RendererPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
