#pragma once
#include "GPUBuffer.h"

class ConstantBuffer : public GPUBuffer
{
public:
    ConstantBuffer() {}
    virtual ~ConstantBuffer() {}

    void Create(GraphicsDevice* pDevice, UINT size);

    void* GetMappedData() const { return m_pMappedData; }
    void UpdateData(const void* pData, UINT size);

private:
    void* m_pMappedData = nullptr;
};
