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

	while (true)
	{
		if (!m_window.ProcessMessage())
		{
			break;
		}
		GraphicsDevice::Instance().ScreenFlip();
	}
	GraphicsDevice::Instance().Shutdown();
}
