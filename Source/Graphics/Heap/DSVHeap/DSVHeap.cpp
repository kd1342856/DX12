#include "DSVHeap.h"

int DSVHeap::CreateDSV(ID3D12Resource* pBuffer, DXGI_FORMAT format)
{
	if (m_useCount < m_nextRegistNumber)
	{
		assert(0 && "Šm•ÛÏ‚Ý‚Ìƒq[ƒv—Ìˆæ‚ð’´‚¦‚Ü‚µ‚½");
		return 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (UINT64)m_nextRegistNumber * m_incrementSize;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	m_pDevice->GetDevice()->CreateDepthStencilView(pBuffer, &dsvDesc, handle);

	return m_nextRegistNumber++;
}
