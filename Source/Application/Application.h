#pragma once
#include "../../Source/Framework/Window/Window.h"

class Application
{
public:

	void Execute();

private:
	// DLLの読み込みとチE��レクトリ設宁E
	void SetDirectoryAndLoadDll();

	Window m_window;

	// 画面サイズ
	static constexpr int SCREEN_WIDTH = 1280;
	static constexpr int SCREEN_HEIGHT = 720;

	Application(){}
public:
	static Application& Instance();

};

