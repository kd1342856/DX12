#pragma once

class Window
{
public:
	bool Create(int clientWidth, int clientHeight, const std::wstring& titleName, const std::wstring& windowClassName);
	bool ProcessMessage();

	HWND GetWndHandle() const { return m_hWnd; }

private:
	HWND m_hWnd;	//	ウィンドウハンドル
};