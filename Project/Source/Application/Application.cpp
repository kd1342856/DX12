#include "../Pch.h"

#include "../Framework/DirectX/Utility/ClassAssembly.h"

#include "Application.h"
#include "Scene/GameScene/GameScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include "../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../Graphics/Shader/ShaderHotReload/ShaderHotReload.h"
#include "../Framework/Manager/Audio/AudioManager.h"
#include "../Graphics/Renderer/Renderer.h"
#include "../Framework/Manager/Scene/SceneManager.h"
#include "../Framework/Manager/Scene/Scene.h"
#include "../Framework/System/JobSystem/JobSystem.h"
#include "../Framework/DirectX/Utility/Profiler.h"
#include "../Framework/System/AppPaths/AppPaths.h"
#include "UI/OptionMenu.h"
#include "../Framework/ImGuiEditor/Panels/ShaderReloadPanel.h"
#include "../Framework/ImGuiEditor/EditorPreferences.h"
#include "../Framework/ImGuiEditor/EditHistory.h"
#include "../Framework/ImGuiEditor/EditorTools.h"

// ImGui の Win32 メッセージハンドラへ転送する
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "../Framework/DirectX/Utility/Thread.h"

void my_terminate_handler() {
    std::ofstream ofs(AppPaths::GetSavedFilePath(L"crash.log"), std::ios::app);
    ofs << "std::terminate was called! Unhandled exception!\n";
    try {
        throw; // 再スローして例外の型を特定する
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
	// 最初にパスを確定する(以降のファイル読み込み・DLL読み込み・ログ出力はすべてこれに依存する)
	AppPaths::Initialize();

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
	// Asset/Shader の更新を監視し、保存したシェーダーをその場でコンパイルし直す
	ShaderHotReload::Instance().Initialize(L"Asset/Shader");
	// エディターの設定(表示ガンマなど)。シーンではなく人ごとの設定なのでSavedに置く
	EditorPreferences::Load();
	// 閉じるボタンで、保存していないシェーダーの設定があれば聞く
	UnsavedChanges::Initialize(m_window.GetWndHandle());
	// オプション画面で保存した明るさ(表示ガンマ)。音量はAudioManager::Init()で読み込まれる
	OptionMenu::LoadSettings();

	// AudioManager
	AudioManager::Instance().Init();
	// 曲ごとの音量をSound Managerに表示するため、起動シーンに関係なく最初に登録しておく
	AudioManager::Instance().RegisterBGMTrack(TitleScene::kBGMPath, "TitleBGM", TitleScene::kBGMDefaultVolume);

	// GameManager 
	GameManager::Instance().Init();
	// GameManager::Init()で全シェーダーを登録し、PSOを事前に作り終えている。新しく作ったPSOがあればディスクに保存する
	ShaderManager::Instance().SavePipelineLibrary();

	// SceneManager
	SceneManager::Instance().Init();
	// ゲームはタイトル画面から始める(スタートでGameSceneへ)
	SceneManager::Instance().SetCurrentSceneWithoutFade(std::make_unique<TitleScene>());
	if (SceneManager::Instance().GetCurrentScene())
	{
		SceneManager::Instance().GetCurrentScene()->Init();
	}

	// ゲームループ
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

		// 前のフレームのコマンドを積み終えた後、次のフレームを積み始める前に差し替える
		ShaderHotReload::Instance().Update();

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
		// シェーダーのコンパイルエラーは、エディターを閉じていてもプレイ中でも画面の隅に出す
		ShaderReloadPanel::DrawErrorNotice();
		// Undo/Redoのショートカット(Ctrl+Z・Ctrl+Y)と、戻したときの知らせ
		EditHistory::Instance().Update();
		// 保存していない変更の確認と、ゲームと同じ見え方への切り替え(F4)
		UnsavedChanges::Update();
		GameLookPreview::Update();

		GDF::Instance().EndFrame();
	}

	ShaderHotReload::Instance().Terminate();
	SceneManager::Instance().Shutdown();
	GameManager::Instance().Shutdown();
	JobSystem::Instance().Shutdown();
	AudioManager::Instance().Shutdown();
	GDF::Instance().Shutdown();
}

void Application::SetDirectoryAndLoadDll()
{
	// assimpは遅延読み込み(DelayLoadDLLs)なので、最初に使われる前にフルパスで読み込んでおけば、
	// 遅延読み込み側は読み込み済みのモジュールを名前で見つけて使う。
	// <root>/Library内のビルド結果を探し、無ければ通常のDLL検索順(exeの隣など)で探す。
#ifdef _DEBUG
	const wchar_t* pConfigName = L"Debug";
	const wchar_t* pAssimpDllName = L"assimp-vc143-mtd.dll";
#else
	const wchar_t* pConfigName = L"Release";
	const wchar_t* pAssimpDllName = L"assimp-vc143-mt.dll";
#endif
	std::filesystem::path assimpDllPath = AppPaths::GetLibraryDir() / L"assimp" / L"build" / L"lib" / pConfigName / pAssimpDllName;
	if (!LoadLibraryExW(assimpDllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
	{
		LoadLibraryExW(pAssimpDllName, nullptr, 0);
	}
}
Application& Application::Instance()
{
    static Application instance;
    return instance;
}


