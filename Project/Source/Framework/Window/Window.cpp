#include "../../Pch.h"
#include "Window.h"

// ImGui Win32メッセージハンドラのextern宣言
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 全画面切り替え: F11 または Alt+Enter
	if (auto* pWindow = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
	{
		const bool isRepeat = (lParam & (1 << 30)) != 0;
		const bool altEnter = message == WM_SYSKEYDOWN && wParam == VK_RETURN && (lParam & (1 << 29));
		const bool f11 = message == WM_KEYDOWN && wParam == VK_F11;
		if ((altEnter || f11) && !isRepeat)
		{
			pWindow->ToggleFullscreen();
			return 0;
		}
		// Alt+Enter時のビープ音を抑止
		if (message == WM_SYSCHAR && wParam == VK_RETURN)
		{
			return 0;
		}
	}

	// ImGuiにメッセージを横流しする（マウス・キーボード入力をImGuiが受け取れるように）
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	// Inputシステムへのメッセージ横流し
	Input::Instance().ProcessMessage(message, wParam, lParam);

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		// スワップチェーンと画面の大きさのRTは、次のフレームの先頭で作り直す(最小化は無視される)
		if (wParam != SIZE_MINIMIZED)
		{
			GraphicsDevice::Instance().RequestResize(LOWORD(lParam), HIWORD(lParam));
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
}
bool Window::Create(int clientWidth, int clientHeight, const std::wstring& titleName, const std::wstring& windowClassName)
{
	HINSTANCE hInst = GetModuleHandle(0);

	//====================
	//メインウィンドウ
	//====================

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = (WNDPROC)WindowProcedure;
	wc.lpszClassName = windowClassName.c_str();
	wc.hInstance = hInst;

	if (!RegisterClassEx(&wc))
	{
		return false;
	}

	// 枠をドラッグして大きさを変えられる(描画解像度もウィンドウに合わせて変わる)
	DWORD style = WS_OVERLAPPEDWINDOW;
	RECT rect = { 0, 0, clientWidth, clientHeight };
	AdjustWindowRect(&rect, style, FALSE);

	m_hWnd = CreateWindow(
		windowClassName.c_str(),
		titleName.c_str(),
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInst,
		this
	);

	if (m_hWnd == nullptr)
	{
		return false;
	}

	SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	m_windowedStyle = style;

	ShowWindow(m_hWnd, SW_SHOW);

	UpdateWindow(m_hWnd);

	return true;
}

void Window::SetFullscreen(bool fullscreen)
{
	if (fullscreen == m_isFullscreen || m_hWnd == nullptr) return;

	if (fullscreen)
	{
		// 元に戻すために現在の位置・サイズを保存
		GetWindowPlacement(m_hWnd, &m_windowedPlacement);

		MONITORINFO mi = { sizeof(MONITORINFO) };
		GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);

		SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(m_hWnd, HWND_TOP,
			mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
	}
	else
	{
		SetWindowLongPtr(m_hWnd, GWL_STYLE, m_windowedStyle | WS_VISIBLE);
		SetWindowPlacement(m_hWnd, &m_windowedPlacement);
		SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	}

	m_isFullscreen = fullscreen;
}

bool Window::ProcessMessage()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}
