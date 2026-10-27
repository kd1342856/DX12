#pragma once
#include "../IEditorPanel.h"

// ‚·‚«ŠÔ‚Ì‰A‚è(SSAO)‚Ìİ’è
class SSAOPanel : public IEditorPanel
{
public:
    void Draw(EditorContext& ctx) override;
};
