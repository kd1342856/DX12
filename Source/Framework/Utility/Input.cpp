
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
	for (int i = 0; i < 256; ++i) {
		m_prevKeyState[i] = m_keyState[i];
		m_keyState[i] = (GetAsyncKeyState(i) & 0x8000) ? 0x80 : 0x00;
	}

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
