#pragma once
#include "CommandContext.h"
#include "../FrameResource.h"

class GraphicsDevice;

class GraphicsCommandContext : public CommandContext
{
public:
    GraphicsCommandContext() = default;
    virtual ~GraphicsCommandContext() = default;

    bool Init(ID3D12Device* pDevice);
    void Shutdown();

    // 記録の開始
    void Begin(FrameResource& frameResource);
    
    // CommandContext のインターフェース実装
    virtual void Begin() override; // 引数なし版（基本使わないがインターフェース用）
    virtual void Close() override;
    virtual void ResetAllocator() override;

private:
    ID3D12CommandAllocator* m_pCurrentAllocator = nullptr;
};

