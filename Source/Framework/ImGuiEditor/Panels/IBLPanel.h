#pragma once
#include "../IEditorPanel.h"

class IBLPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
