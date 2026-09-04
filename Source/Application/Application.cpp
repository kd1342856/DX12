#include "../Pch.h"

#include "../Framework/DirectX/Utility/ClassAssembly.h"

#include "Application.h"
#include "Scene/GameScene/GameScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include "../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../Framework/Manager/Audio/AudioManager.h"
#include "../Graphics/Renderer/Renderer.h"
#include "../Framework/Manager/Scene/SceneManager.h"
#include "../Framework/Manager/Scene/Scene.h"
#include "../Framework/System/JobSystem/JobSystem.h"
#include "../Framework/DirectX/Utility/Profiler.h"

// ImGui �� Win32 ���b�Z�[�W�n���h����]������
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "../Framework/DirectX/Utility/Thread.h"

void my_terminate_handler() {
    std::ofstream ofs("crash.log", std::ios::app);
    ofs << "std::terminate was called! Unhandled exception!\n";
    try {
        throw; // �ăX���[���ė�O�̌^�����
    } catch (const std::exception& e) {
        ofs << "Exception type: std::exception, Message: " << e.what() << "\n";
    } catch (...) {
        ofs << "Unknown exception type.\n";
    }
    ofs.close();
    std::abort();
}

namespace Thread { std::thread::id g_mainThreadId; }

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	std::set_terminate(my_terminate_handler);

	// Register the main thread ID first for thread safety assertions
	Thread::RegisterMainThread();
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Application::Instance().Execute();
	CoUninitialize();
	return 0;
}

void Application::Execute()
{
	SetDirectoryAndLoadDll();

	if (!m_window.Create(SCREEN_WIDTH, SCREEN_HEIGHT, L"Haihu", L"Window"))
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

	Renderer::InitializeRenderTargets(SCREEN_WIDTH, SCREEN_HEIGHT);

	ImGui_ImplWin32_Init(m_window.GetWndHandle());

	// ShaderManager
	ShaderManager::Instance().Initialize(&GDF::Instance().GetGraphicsDevice());

	// AudioManager
	AudioManager::Instance().Init();

	// GameManager 
	GameManager::Instance().Init();

	// SceneManager
	SceneManager::Instance().Init();
	SceneManager::Instance().SetCurrentSceneWithoutFade(std::make_unique<GameScene>());
	if (SceneManager::Instance().GetCurrentScene())
	{
		SceneManager::Instance().GetCurrentScene()->Init();
	}

	// �Q�[�����[�v
	while (true)
	{
		if (!m_window.ProcessMessage())
			break;

		GameTimer::Instance().Update();

		// AudioManager 
		AudioManager::Instance().Update();

		// SetWindowText forces a non-client repaint, so do it once per second only.
		if (GameTimer::Instance().IsFpsUpdated())
		{
			GameTimer::Instance().UpdateWindowTitle(m_window.GetWndHandle(), L"Haihu");
		}

		GDF::Instance().BeginFrame();


		auto* pScene = dynamic_cast<Scene*>(SceneManager::Instance().GetCurrentScene());
		{
			PROFILE_CPU_SCOPE("GameManager::Update");
			GameManager::Instance().Update(GameTimer::Instance().DeltaTime(), pScene);
		}

		{
			// SceneManager::Update()は現在のシーンのRender()も実行しており、そちらは
			// 既にもっと細かいスコープ(Shadow/Scene/PostProcess等)を持っている - これは
			// その塊全体(ゲーム側のUpdate+Render)がフレームのどれくらいを占めるかを示すだけ。
			PROFILE_CPU_SCOPE("SceneManager::Update");
			SceneManager::Instance().Update();
		}
		SceneManager::Instance().DrawFade();

		GDF::Instance().EndFrame();
	}

	SceneManager::Instance().Shutdown();
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


