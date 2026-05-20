#include "Application.h"
#include "Framework/DirectX/Utility/Input.h"
#include "Scene/GameScene/GameScene.h"

// ImGuiのWin32メッセージ処理（Windows.hより前にimgui.hが必要）
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

	// ImGuiのWin32バックエンド初期化（GDFのInit後=GraphicsDevice初期化後に行う）
	ImGui_ImplWin32_Init(m_window.GetWndHandle());

	// ShaderManager初期化
	ShaderManager::Instance().Init();

	// シーン生成
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
