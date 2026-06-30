
#include "../Framework/DirectX/Utility/ClassAssembly.h"

ClassAssembly& ClassAssembly::Instance() { static ClassAssembly instance; return instance; }
Logger& Logger::Instance() { static Logger instance; return instance; }

#include "Application.h"
#include "Scene/GameScene/GameScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include "../Framework/Manager/AudioManager.h"
#include "../Framework/Manager/SceneManager.h"

// ImGui縺ｮWin32繝｡繝・そ繝ｼ繧ｸ繝上Φ繝峨Λ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// XAudio2縺ｪ縺ｩ繝槭Ν繝√せ繝ｬ繝・ラ縺ｧ蜍穂ｽ懊☆繧九さ繝ｳ繝昴・繝阪Φ繝医・縺溘ａ縲｀ULTITHREADED縺ｧCOM繧貞・譛溷喧
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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

	Input::Instance().Init(m_window.GetWndHandle());

	if (!GDF::Instance().Init(m_window.GetWndHandle(), SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		assert(0 && "GDF init failed");
		return;
	}

	// ImGui縺ｮWin32繝舌ャ繧ｯ繧ｨ繝ｳ繝牙・譛溷喧
	ImGui_ImplWin32_Init(m_window.GetWndHandle());

	// ShaderManager縺ｮ蛻晄悄蛹・
	ShaderManager::Instance().Init();

	// AudioManager縺ｮ蛻晄悄蛹・
	AudioManager::Instance().Init();

	// SceneManager縺ｮ蛻晄悄蛹・
	SceneManager::Instance().Init();
	SceneManager::Instance().SetCurrentSceneWithoutFade(std::make_shared<TitleScene>());
	if (SceneManager::Instance().GetCurrentScene())
	{
		SceneManager::Instance().GetCurrentScene()->Init();
	}

	// 繧ｲ繝ｼ繝繝ｫ繝ｼ繝・
	while (true)
	{
		if (!m_window.ProcessMessage())
			break;

		// 繧ｿ繧､繝槭・譖ｴ譁ｰ(DeltaTime縺ｨFPS險域ｸｬ)
		GameTimer::Instance().Update();

		// AudioManager縺ｮ譖ｴ譁ｰ
		AudioManager::Instance().Update();

		// 繧ｦ繧｣繝ｳ繝峨え繧ｿ繧､繝医Ν縺ｫFPS陦ｨ遉ｺ
		GameTimer::Instance().UpdateWindowTitle(m_window.GetWndHandle(), L"DX12Framework");

		GDF::Instance().BeginFrame();

		SceneManager::Instance().Update();
		SceneManager::Instance().DrawFade();

		GDF::Instance().EndFrame();
	}

	// AudioManager縺ｮ邨ゆｺ・
	AudioManager::Instance().Shutdown();
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
Application& Application::Instance()
{
    static Application instance;
    return instance;
}

