#pragma once

class Window
{
public:
	bool Create(int clientWidth, int clientHeight, const std::wstring& titleName, const std::wstring& windowClassName);
	bool ProcessMessage();

	HWND GetWndHandle() const { return m_hWnd; }

	// ボーダーレス全画面の切り替え(F11 / Alt+Enter)。
	// ウィンドウがモニター全体の大きさになり、WM_SIZEでスワップチェーンと描画解像度もそれに合わせて作り直される。
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen() { SetFullscreen(!m_isFullscreen); }
	bool IsFullscreen() const { return m_isFullscreen; }

private:
	bool m_isFullscreen = false;
	DWORD m_windowedStyle = 0;
	WINDOWPLACEMENT m_windowedPlacement = { sizeof(WINDOWPLACEMENT) };

	HWND m_hWnd;	//	ウィンドウハンドル
};