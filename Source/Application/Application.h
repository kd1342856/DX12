#pragma once
#include "../System/Window/Window.h"

class Application
{
public:

	void Execute();
private:
	void SetDirectoryAndLoadDll();

	Window m_window;
	Application(){}

public:
	static Application& Instance()
	{
		static Application instance;
		return instance;
	}

};
