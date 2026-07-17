#include "../Pch.h"

#include "../Framework/DirectX/Utility/ClassAssembly.h"

#include "Application.h"
#include "Scene/GameScene/GameScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include "../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../Framework/Manager/Audio/AudioManager.h"
#include "../Framework/Manager/Scene/SceneManager.h"
#include "../Framework/Manager/Scene/Scene.h"

// ImGui邵ｺ・ｮWin32郢晢ｽ｡郢昴・縺晉ｹ晢ｽｼ郢ｧ・ｸ郢昜ｸ莞ｦ郢晏ｳｨﾎ・
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// XAudio2邵ｺ・ｪ邵ｺ・ｩ郢晄ｧｭﾎ晉ｹ昶・縺帷ｹ晢ｽｬ郢昴・繝ｩ邵ｺ・ｧ陷咲ｩゑｽｽ諛岩・郢ｧ荵昴＆郢晢ｽｳ郢晄亢繝ｻ郢晞亂ﾎｦ郢晏現繝ｻ邵ｺ貅假ｽ∫ｸｲ・€ULTITHREADED邵ｺ・ｧCOM郢ｧ雋槭・隴帶ｺｷ蝟ｧ
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
		MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
		return;
	}

	Input::Instance().Init(m_window.GetWndHandle());

	if (!GDF::Instance().Init(m_window.GetWndHandle(), SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		MessageBoxW(nullptr, L"Failed to initialize Graphics Device Framework.", L"Error", MB_OK | MB_ICONERROR);
		return;
	}

	// ImGui邵ｺ・ｮWin32郢晁・繝｣郢ｧ・ｯ郢ｧ・ｨ郢晢ｽｳ郢晉甥繝ｻ隴帶ｺｷ蝟ｧ
	ImGui_ImplWin32_Init(m_window.GetWndHandle());

	// ShaderManager邵ｺ・ｮ陋ｻ譎・ｄ陋ｹ繝ｻ
	ShaderManager::Instance().Initialize(&GDF::Instance().GetGraphicsDevice());

	// AudioManager邵ｺ・ｮ陋ｻ譎・ｄ陋ｹ繝ｻ
	AudioManager::Instance().Init();

	// GameManager 縺ｮ蛻晄悄蛹厄ｼ・omponent/System 逋ｻ骭ｲ繧貞・驛ｨ縺薙％縺ｧ荳€諡ｬ・・
	GameManager::Instance().Init();

	// SceneManager 縺ｮ蛻晄悄蛹・
	SceneManager::Instance().Init();
	SceneManager::Instance().SetCurrentSceneWithoutFade(std::make_unique<GameScene>());
	if (SceneManager::Instance().GetCurrentScene())
	{
		SceneManager::Instance().GetCurrentScene()->Init();
	}

	// 郢ｧ・ｲ郢晢ｽｼ郢晉ｹ晢ｽｫ郢晢ｽｼ郢昴・
	while (true)
	{
		if (!m_window.ProcessMessage())
			break;

		// 郢ｧ・ｿ郢ｧ・､郢晄ｧｭ繝ｻ隴厄ｽｴ隴・ｽｰ(DeltaTime邵ｺ・ｨFPS髫ｪ蝓滂ｽｸ・ｬ)
		GameTimer::Instance().Update();

		// AudioManager邵ｺ・ｮ隴厄ｽｴ隴・ｽｰ
		AudioManager::Instance().Update();

		// 郢ｧ・ｦ郢ｧ・｣郢晢ｽｳ郢晏ｳｨ縺育ｹｧ・ｿ郢ｧ・､郢晏現ﾎ晉ｸｺ・ｫFPS髯ｦ・ｨ驕会ｽｺ
		GameTimer::Instance().UpdateWindowTitle(m_window.GetWndHandle(), L"DX12Framework");

		GDF::Instance().BeginFrame();

		// GameManager 縺悟・ System 繧帝?・分縺ｫ Update
		auto* pScene = dynamic_cast<Scene*>(SceneManager::Instance().GetCurrentScene());
		GameManager::Instance().Update(GameTimer::Instance().DeltaTime(), pScene);

		// SceneManager 縺ｯ繝輔ぉ繝ｼ繝臥ｮ｡逅・・縺ｿ
		SceneManager::Instance().Update();
		SceneManager::Instance().DrawFade();

		GDF::Instance().EndFrame();
	}

	// シングルトンの明示的な終了処理
	GameManager::Instance().Shutdown();
	JobSystem::Instance().Shutdown();
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


