#pragma once
#include "../IEditorPanel.h"

class ShadowPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
