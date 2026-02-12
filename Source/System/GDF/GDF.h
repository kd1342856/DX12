#pragma once

class GraphicsDevice;
class CBufferAllocator;

// =============================================
// GDF (Graphics Device Facade)
// GraphicsDeviceの内部構造を隠蔽し、
// シンプルなAPIを提供するファサードクラス
// =============================================
class GDF
{
public:

	// デバイス初期化
	bool Init(HWND hwnd, int width, int height);

	// フレーム開始(ヒープセット・定数バッファリセット込み)
	void BeginFrame();

	// フレーム終了(コマンド実行・プレゼント)
	void EndFrame();

	// 定数バッファバインド
	template<typename T>
	void BindCBuffer(int slot, const T& data);

	// 終了処理
	void Shutdown();

	// 低レベルアクセス(必要な場合のみ使用)
	GraphicsDevice& GetGraphicsDevice();
	ID3D12Device8* GetDevice() const;
	ID3D12GraphicsCommandList6* GetCmdList() const;



private:
	GDF() {}
	~GDF() {}

public:
	static GDF& Instance()
	{
		static GDF instance;
		return instance;
	}
};

// BindCBufferのテンプレート実装
// (GraphicsDevice/CBufferAllocatorの定義が見えている状態で展開される)
template<typename T>
inline void GDF::BindCBuffer(int slot, const T& data)
{
	GraphicsDevice::Instance().GetCBufferAllocator()->BindAndAttachData(slot, data);
}
