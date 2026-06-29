#include "Window.h"

// ImGui Win32メッセージハンドラのextern宣言
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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

	DWORD style = WS_OVERLAPPEDWINDOW - WS_THICKFRAME;
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

	ShowWindow(m_hWnd, SW_SHOW);

	UpdateWindow(m_hWnd);

	return true;
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
