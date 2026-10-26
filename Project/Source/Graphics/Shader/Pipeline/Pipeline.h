#pragma once
#include <vector>
#include <wrl.h>
#include <dxcapi.h>
#include <d3d12.h>
#include "../RootSignature/RootSignature.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

enum class CullMode {
	None = D3D12_CULL_MODE_NONE,
	Front = D3D12_CULL_MODE_FRONT,
	Back = D3D12_CULL_MODE_BACK,
};
enum class BlendMode { None, Add, Alpha };
enum class InputLayout { POSITION, TEXCOORD, NORMAL, TANGENT, COLOR, SKININDEX, SKINWEIGHT };
enum class PrimitiveTopologyType {
	Undefined = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
	Point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
	Line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	Patch = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
};

struct PipelineDesc {
	std::vector<IDxcBlob*> pBlobs;
	std::vector<DXGI_FORMAT> Formats;
	std::vector<InputLayout> InputLayouts;
	RootSignature* pRootSignature = nullptr;
	CullMode CullMode = CullMode::Back;
	BlendMode BlendMode = BlendMode::None;
	PrimitiveTopologyType TopologyType = PrimitiveTopologyType::Triangle;
	bool IsDepth = true;
	bool IsDepthMask = true;
	bool IsWireFrame = false;
	D3D12_COMPARISON_FUNC DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	int DepthBias = 0;
};

// ShaderManagerが持つPSOへの参照。シェーダーのホットリロードでPSOが作り直されても、同じ参照から新しいPSOを引ける。
// 生のポインタの代わりに持っておき、使うたびにGet()する(ID3D12PipelineState*へは暗黙に変換できる)
class PipelineStateRef
{
public:
	PipelineStateRef() = default;
	explicit PipelineStateRef(const Microsoft::WRL::ComPtr<ID3D12PipelineState>* pSlot)
		: m_pSlot(pSlot)
	{
	}

	ID3D12PipelineState* Get() const
	{
		return m_pSlot ? m_pSlot->Get() : nullptr;
	}

	operator ID3D12PipelineState*() const
	{
		return Get();
	}

private:
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>* m_pSlot = nullptr;
};

class Pipeline {
public:
	// pLibraryとpNameを渡すと、先にPSOライブラリ(ディスクキャッシュ)からpNameで読み込みを試み、
	// 見つからない(または中身が違う)ときだけ新しく作成する。ライブラリへの登録は呼び出し側で行う
	void Create(GraphicsDevice* pGraphicsDevice, const PipelineDesc& desc, ID3D12PipelineLibrary* pLibrary = nullptr, const wchar_t* pName = nullptr);
	PrimitiveTopologyType GetTopologyType() const { return m_topologyType; }
	ID3D12PipelineState* GetPipeline() const { return m_pPipelineState.Get(); }
	bool IsLoadedFromLibrary() const { return m_isLoadedFromLibrary; }
private:
	void SetInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements, const std::vector<InputLayout>& inputLayouts);
	void SetBlendMode(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc, BlendMode blendMode);
	GraphicsDevice* m_pDevice = nullptr;
	PrimitiveTopologyType m_topologyType = PrimitiveTopologyType::Undefined;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;
	bool m_isLoadedFromLibrary = false;
};