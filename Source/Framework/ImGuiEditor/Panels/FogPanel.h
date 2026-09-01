#pragma once
#include "../IEditorPanel.h"

class FogPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
