#pragma once
#include "../System/Window/Window.h"
#include "Scene/SceneBase.h"

class Application
{
public:

	void Execute();

	// シーンをセット
	void SetScene(std::shared_ptr<SceneBase> spScene) { m_spScene = spScene; }

private:
	// DLLの読み込みとディレクトリ設定
	void SetDirectoryAndLoadDll();

	Window m_window;
	std::shared_ptr<SceneBase> m_spScene;

	// 画面サイズ
	static constexpr int SCREEN_WIDTH = 1280;
	static constexpr int SCREEN_HEIGHT = 720;

	Application(){}

public:
	static Application& Instance()
	{
		static Application instance;
		return instance;
	}

};
