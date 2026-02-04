#include "Application.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);	//	COM初期化

	Application::Instance().Execute();

	CoUninitialize();	//	COM解放
	return 0;
}
void Application::Execute()
{
	static const int width = 1280;
	static const int height = 720;
	if (!m_window.Create(1280, 720, L"DX12Framework", L"Window"))
	{
		assert(0 && "ウィンドウ作成失敗。");
		return;
	}
	if (!GraphicsDevice::Instance().Init(m_window.GetWndHandle(), width, height))
	{
		assert(0 && "グラフィックスデバイス初期化。");
		return;
	}

	ModelData modelData;
	modelData.Load("Asset/Model/Cube/Cube.gltf");

	RenderingSetting renderingSetting = {};
	renderingSetting.InputLayouts = 
	{ InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::COLOR, InputLayout::NORMAL, InputLayout::TANGENT };
	renderingSetting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	renderingSetting.IsDepth = false;
	renderingSetting.IsDepthMask = false;

	Shader shader;
	shader.Create(&GraphicsDevice::Instance(), L"Study", 
		renderingSetting, {RangeType::CBV, RangeType::SRV, RangeType::SRV , RangeType::SRV , RangeType::SRV });

	Math::Matrix mView = Math::Matrix::CreateTranslation(0, 0, 3);

	Math::Matrix mProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.01, 1000.0f);

	CBufferData::Camera cbCamera;
	cbCamera.mView = mView;
	cbCamera.mProj = mProj;

	while (true)
	{
		if (!m_window.ProcessMessage())
		{
			break;
		}
		GraphicsDevice::Instance().Prepare();

		GraphicsDevice::Instance().GetCBVSRVUAVHeap()->SetHeap();

		GraphicsDevice::Instance().GetCBufferAllocator()->ResetCurrentUseNumber();
		shader.Begin(width, height);

		GraphicsDevice::Instance().GetCBufferAllocator()->BindAndAttachData(0, cbCamera);

		shader.DrawModel(modelData);

		GraphicsDevice::Instance().ScreenFlip();
	}
	GraphicsDevice::Instance().Shutdown();
}
