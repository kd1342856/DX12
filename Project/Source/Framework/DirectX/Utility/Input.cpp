#include "../../../Pch.h"

Input& Input::Instance()
{
	static Input instance;
	return instance;
}

void Input::Init(HWND hWnd)
{
	m_hWnd = hWnd;
}

void Input::Update()
{
	// ウィンドウが選択されていない(ほかのアプリを操作している)間は、キーもマウスも受け取らず、
	// カーソルも中央へ戻さない(GetAsyncKeyStateはほかのアプリへの入力も拾ってしまうため)
	const bool isActive = (m_hWnd != nullptr) && (GetForegroundWindow() == m_hWnd);
	if (!isActive)
	{
		for (int i = 0; i < 256; ++i)
		{
			m_prevKeyState[i] = 0;
			m_keyState[i] = 0;
		}
		m_mouseDeltaX = 0;
		m_mouseDeltaY = 0;
		// 選択し直したときに、離れていた間のマウスの動きを1フレームの動きとして扱わない
		m_wasRelative = false;
		m_isWindowActive = false;
		return;
	}

	for (int i = 0; i < 256; ++i) {
		m_prevKeyState[i] = m_keyState[i];
		m_keyState[i] = (GetAsyncKeyState(i) & 0x8000) ? 0x80 : 0x00;
	}
	// 選択し直した最初のフレームは、そのとき押されているキーを「押した瞬間」として扱わない
	// (ウィンドウをクリックして戻ったときに、ゲームが左クリックとして受け取らないように)
	if (!m_isWindowActive)
	{
		for (int i = 0; i < 256; ++i)
		{
			m_prevKeyState[i] = m_keyState[i];
		}
	}
	m_isWindowActive = true;

	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(m_hWnd, &pt);

	m_mouseX = pt.x;
	m_mouseY = pt.y;

	if (m_isRelative)
	{
		RECT rect;
		GetClientRect(m_hWnd, &rect);
		int centerX = (rect.right - rect.left) / 2;
		int centerY = (rect.bottom - rect.top) / 2;

		if (!m_wasRelative) {
			m_mouseDeltaX = 0;
			m_mouseDeltaY = 0;
		} else {
			m_mouseDeltaX = m_mouseX - centerX;
			m_mouseDeltaY = m_mouseY - centerY;
			if (abs(m_mouseDeltaX) < 2) m_mouseDeltaX = 0;
			if (abs(m_mouseDeltaY) < 2) m_mouseDeltaY = 0;
		}

		POINT centerPt = { centerX, centerY };
		ClientToScreen(m_hWnd, &centerPt);
		SetCursorPos(centerPt.x, centerPt.y);

		m_prevMouseX = centerX;
		m_prevMouseY = centerY;
	}
	else
	{
		m_mouseDeltaX = m_mouseX - m_prevMouseX;
		m_mouseDeltaY = m_mouseY - m_prevMouseY;
		m_prevMouseX  = m_mouseX;
		m_prevMouseY  = m_mouseY;
	}

	m_wasRelative = m_isRelative;
}
void Input::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam) {}
