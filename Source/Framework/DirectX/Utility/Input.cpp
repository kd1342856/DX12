#include "Input.h"

void Input::Init(HWND hWnd)
{
	m_mouse.SetWindow(hWnd);

	m_prevMouseX  = 0;
	m_prevMouseY  = 0;
	m_mouseDeltaX = 0;
	m_mouseDeltaY = 0;
}

void Input::Update()
{
	m_keyboardState = m_keyboard.GetState();
	m_keyboardTracker.Update(m_keyboardState);

	m_mouseState = m_mouse.GetState();
	m_mouseTracker.Update(m_mouseState);

	if (m_mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE)
	{
		m_mouseDeltaX = m_mouseState.x;
		m_mouseDeltaY = m_mouseState.y;
	}
	else
	{
		m_mouseDeltaX = m_mouseState.x - m_prevMouseX;
		m_mouseDeltaY = m_mouseState.y - m_prevMouseY;
		m_prevMouseX  = m_mouseState.x;
		m_prevMouseY  = m_mouseState.y;
	}
}

void Input::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		DirectX::Mouse::ProcessMessage(msg, wParam, lParam);
		break;
	default:
		break;
	}

	switch (msg)
	{
	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		DirectX::Keyboard::ProcessMessage(msg, wParam, lParam);
		break;
	default:
		break;
	}
}