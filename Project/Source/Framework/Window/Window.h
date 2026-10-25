#pragma once

class Window
{
public:
	bool Create(int clientWidth, int clientHeight, const std::wstring& titleName, const std::wstring& windowClassName);
	bool ProcessMessage();

	HWND GetWndHandle() const { return m_hWnd; }

	// ボーダーレス全画面の切り替え(F11 / Alt+Enter)。
	// 描画解像度(スワップチェーン)は変えず、DXGIがウィンドウ全体へ拡大して表示する。
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen() { SetFullscreen(!m_isFullscreen); }
	bool IsFullscreen() const { return m_isFullscreen; }

private:
	bool m_isFullscreen = false;
	DWORD m_windowedStyle = 0;
	WINDOWPLACEMENT m_windowedPlacement = { sizeof(WINDOWPLACEMENT) };

	HWND m_hWnd;	//	ウィンドウハンドル
};