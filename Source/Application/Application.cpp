#include "Application.h"
#include "Scene/GameScene/GameScene.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	Application::Instance().Execute();
	CoUninitialize();
	return 0;
}

void Application::Execute()
{
	SetDirectoryAndLoadDll();

	if (!m_window.Create(SCREEN_WIDTH, SCREEN_HEIGHT, L"DX12Framework", L"Window"))
	{
		assert(0 && "Window create failed");
		return;
	}

	if (!GDF::Instance().Init(m_window.GetWndHandle(), SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		assert(0 && "GDF init failed");
		return;
	}

	// ShaderManager初期化
	ShaderManager::Instance().Init();

	// シーン作成
	auto spScene = std::make_shared<GameScene>();
	SetScene(spScene);
	m_spScene->Init();

	// ゲームループ
	while (true)
	{
		if (!m_window.ProcessMessage())
			break;

		GDF::Instance().BeginFrame();

		if (m_spScene)
			m_spScene->Update();

		GDF::Instance().EndFrame();
	}

	GDF::Instance().Shutdown();
}

void Application::SetDirectoryAndLoadDll()
{
#ifdef _DEBUG
	SetDllDirectoryA("Library/assimp/build/lib/Debug");
	LoadLibraryExA("assimp-vc143-mtd.dll", NULL, NULL);
#else
	SetDllDirectoryA("Library/assimp/build/lib/Release");
	LoadLibraryExA("assimp-vc143-mt.dll", NULL, NULL);
#endif
}